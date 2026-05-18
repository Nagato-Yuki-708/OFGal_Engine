// Copyright 2026 MrSeagull. All Rights Reserved.
#include "WindowsSystem.h"
#include <sstream>
#include <cstring>
#include <vector>

// 返回当前 exe 所在目录（包含结尾的 '\\'），例如 L"C:\\MyApp\\"
std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        // 失败时回退到当前工作目录
        return L".\\";
    }
    std::wstring fullPath(path, length);
    size_t pos = fullPath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return fullPath.substr(0, pos + 1);   // 包含结尾的反斜杠
    }
    return L".\\";
}

WindowsSystem::WindowsSystem() {
    exePath_ProjectStructureViewer = GetExeDirectory() + L"ProjectStructureViewer.exe";
    exePath_LevelTreeList = GetExeDirectory() + L"LevelTreeList.exe";
    exePath_BlueprintViewer= GetExeDirectory() + L"BlueprintViewer.exe";
    exePath_TextBlock = GetExeDirectory() + L"TextBlock.exe";

    _EventBus::getInstance().subscribe_PrintText(
        [this](const std::string& text, const std::string& name, int X, int Y, int cx, int cy) {
            this->ToTextBlock(text, name, X, Y, cx, cy);
        });
    
    OpenProjectStructureViewer(exePath_ProjectStructureViewer.c_str());
    RefreshProjectStructureViewer();
    
    OpenLevelTreeList();

    CreateBPEditorIPC();
    
    // ---------- 按键绑定 ----------
    m_inputSystem.SetGlobalCapture(false);
    m_inputSystem.SetWindowHandle(GetConsoleWindow());

    m_inputCollector.AddBinding({ 13, Modifier::None, KeyCode::Enter, true });
}
WindowsSystem::WindowsSystem(std::string path) {
    size_t len = path.size() + 1;
    currentProjectDirectory = new char[len];
    strcpy_s(currentProjectDirectory, len, path.c_str());

    exePath_ProjectStructureViewer = GetExeDirectory() + L"ProjectStructureViewer.exe";
    exePath_LevelTreeList = GetExeDirectory() + L"LevelTreeList.exe";
    exePath_BlueprintViewer = GetExeDirectory() + L"BlueprintViewer.exe";
    exePath_TextBlock = GetExeDirectory() + L"TextBlock.exe";
    
    _EventBus::getInstance().subscribe_PrintText(
        [this](const std::string& text, const std::string& name, int X, int Y, int cx, int cy) {
            this->ToTextBlock(text, name, X, Y, cx, cy);
        });
    
    OpenProjectStructureViewer(exePath_ProjectStructureViewer.c_str());
    RefreshProjectStructureViewer();

    OpenLevelTreeList();

    CreateBPEditorIPC();
    
    // ---------- 按键绑定 ----------
    m_inputSystem.SetGlobalCapture(false);
    m_inputSystem.SetWindowHandle(GetConsoleWindow());

    m_inputCollector.AddBinding({ 13, Modifier::None, KeyCode::Enter, true });
}

WindowsSystem::~WindowsSystem() {
    // 先拷贝所有子进程的键，避免迭代器失效
    std::vector<std::string> keys;
    keys.reserve(childProcesses.size());
    for (const auto& pair : childProcesses) {
        keys.push_back(pair.first);
    }
    // 安全地逐个终止
    for (const auto& key : keys) {
        TerminateChildProcess(key, 1000);
    }
    if (m_hLevelTreeListPathUpdateEvent) {
        CloseHandle(m_hLevelTreeListPathUpdateEvent);
    }
    DestroyBPEditorIPC();
    ClearTextBlocks();
    delete[] currentProjectDirectory;
}

// 将宽字符串转为 UTF-8 编码的 std::string
std::string WideToUtf8(const std::wstring& wstr)
{
    if (wstr.empty()) return {};

    int size_needed = WideCharToMultiByte(
        CP_UTF8,                  // 目标编码 UTF-8
        0,                        // 转换标志：0 表示快速转换，WC_ERR_INVALID_CHARS 可检测非法字符
        wstr.c_str(),             // 源宽字符串
        static_cast<int>(wstr.length()), // 源字符数
        nullptr, 0, nullptr, nullptr);   // 第一次调用获取所需缓冲区大小

    std::string result(size_needed, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0,
        wstr.c_str(), static_cast<int>(wstr.length()),
        &result[0], size_needed,
        nullptr, nullptr);
    return result;
}

// ---------- 通用接口实现 ----------

std::string WindowsSystem::MakeGlobalName(const std::string& processKey, const std::string& suffix) {
    return "Global\\OFGal_Engine_" + processKey + "_" + suffix;
}

bool WindowsSystem::CreateSharedMemoryBlock(const std::string& processKey, const std::string& blockName, DWORD size, bool readOnly) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return false;
    ChildProcessInfo& info = it->second;

    // 如果块已存在，先清理
    auto blockIt = info.sharedMems.find(blockName);
    if (blockIt != info.sharedMems.end()) {
        SharedMemBlock& block = blockIt->second;
        if (block.pView) UnmapViewOfFile(block.pView);
        if (block.hSharedMem) CloseHandle(block.hSharedMem);
        info.sharedMems.erase(blockIt);
    }

    std::string globalName = MakeGlobalName(processKey, blockName);
    DWORD flProtect = readOnly ? PAGE_READONLY : PAGE_READWRITE;
    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, flProtect, 0, size, globalName.c_str());
    if (!hMap) {
        OutputDebugStringA(("CreateFileMapping failed: " + globalName + " error: " + std::to_string(GetLastError())).c_str());
        return false;
    }

    DWORD dwAccess = readOnly ? FILE_MAP_READ : FILE_MAP_WRITE;
    char* pView = static_cast<char*>(MapViewOfFile(hMap, dwAccess, 0, 0, size));
    if (!pView) {
        OutputDebugStringA("MapViewOfFile failed");
        CloseHandle(hMap);
        return false;
    }

    if (!readOnly) {
        memset(pView, 0, size); // 写入方清零
    }

    SharedMemBlock block;
    block.hSharedMem = hMap;
    block.pView = pView;
    block.size = size;
    block.isReadOnly = readOnly;
    info.sharedMems[blockName] = block;

    return true;
}

bool WindowsSystem::CreateProcessEvent(const std::string& processKey, const std::string& eventName, bool initialState) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return false;
    ChildProcessInfo& info = it->second;

    if (info.events.find(eventName) != info.events.end()) {
        // 已存在，直接返回成功
        return true;
    }

    std::string globalName = MakeGlobalName(processKey, eventName);
    HANDLE hEvent = CreateEventA(NULL, FALSE, initialState ? TRUE : FALSE, globalName.c_str());
    if (!hEvent) {
        OutputDebugStringA(("CreateEvent failed: " + globalName).c_str());
        return false;
    }

    info.events[eventName] = hEvent;
    return true;
}

bool WindowsSystem::LaunchChildProcessW(const ChildProcessConfig& config) {
    const std::string& key = config.processKey;
    if (childProcesses.find(key) != childProcesses.end()) {
        TerminateChildProcess(key, 1000);
    }

    // 先插入一个空的 ChildProcessInfo，以便后续函数能通过 find 找到并操作它
    ChildProcessInfo info;
    childProcesses[key] = info;
    ChildProcessInfo& storedInfo = childProcesses[key];

    // 1. 创建事件（现在 childProcesses 中已有 key）
    std::unordered_map<std::string, bool> eventsToCreate = config.eventsToCreate;
    if (eventsToCreate.find("Exit") == eventsToCreate.end()) {
        eventsToCreate["Exit"] = false;
    }
    for (const auto& ev : eventsToCreate) {
        if (!CreateProcessEvent(key, ev.first, ev.second)) {
            CleanupChildProcess(key);
            return false;
        }
    }

    // 2. 创建共享内存块
    for (const auto& blk : config.sharedMemBlocks) {
        if (!CreateSharedMemoryBlock(key, blk.first, blk.second, false)) {
            CleanupChildProcess(key);
            return false;
        }
    }

    // 3. 构建完整命令行（宽字符串）
    std::wstring fullCmdLine = L"\"" + config.exePath + L"\"";
    if (!config.commandLineArgs.empty()) {
        fullCmdLine += L" " + config.commandLineArgs;
    }

    // 为 CreateProcessW 准备可修改的缓冲区
    std::vector<wchar_t> cmdBuffer(fullCmdLine.begin(), fullCmdLine.end());
    cmdBuffer.push_back(L'\0');

    // 4. 设置启动信息（宽字符版本）
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };

    DWORD creationFlags = 0;
    if (config.createNewConsole) {
        creationFlags |= CREATE_NEW_CONSOLE;
    }
    // redirectStdIO 与 CREATE_NEW_CONSOLE 组合时无需额外标志，新控制台自动接管

    BOOL success = CreateProcessW(
        nullptr,                     // lpApplicationName
        cmdBuffer.data(),            // lpCommandLine（可修改的宽字符缓冲区）
        nullptr, nullptr, FALSE,
        creationFlags,
        nullptr, nullptr,
        &si, &pi
    );

    if (!success) {
        // 输出错误信息（宽字符转 ANSI 仅用于调试）
        OutputDebugStringA(("CreateProcessW failed, error: " + std::to_string(GetLastError())).c_str());
        CleanupChildProcess(key);
        return false;
    }

    storedInfo.hProcess = pi.hProcess;
    storedInfo.hThread = pi.hThread;
    storedInfo.processId = pi.dwProcessId;

    // 关闭线程句柄（保留以便后续 WaitForSingleObject）
    // 不在此处关闭，保存在 storedInfo 中

    return true;
}

bool WindowsSystem::WriteToSharedMemory(const std::string& processKey, const std::string& blockName, const void* data, size_t dataSize) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return false;
    auto& info = it->second;

    auto blockIt = info.sharedMems.find(blockName);
    if (blockIt == info.sharedMems.end()) return false;
    SharedMemBlock& block = blockIt->second;

    if (!block.pView || block.isReadOnly) return false;

    size_t copySize = (dataSize < block.size) ? dataSize : block.size;
    memcpy(block.pView, data, copySize);
    return true;
}

size_t WindowsSystem::ReadFromSharedMemory(const std::string& processKey, const std::string& blockName, void* outBuffer, size_t bufferSize) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return 0;
    auto& info = it->second;

    auto blockIt = info.sharedMems.find(blockName);
    if (blockIt == info.sharedMems.end()) return 0;
    SharedMemBlock& block = blockIt->second;

    if (!block.pView) return 0;

    size_t copySize = (bufferSize < block.size) ? bufferSize : block.size;
    memcpy(outBuffer, block.pView, copySize);
    return copySize;
}

bool WindowsSystem::SignalEvent(const std::string& processKey, const std::string& eventName) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return false;
    auto& info = it->second;

    auto evIt = info.events.find(eventName);
    if (evIt == info.events.end()) return false;
    return SetEvent(evIt->second) != 0;
}

bool WindowsSystem::ResetEvent(const std::string& processKey, const std::string& eventName) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return false;
    auto& info = it->second;

    auto evIt = info.events.find(eventName);
    if (evIt == info.events.end()) return false;
    return ::ResetEvent(evIt->second) != 0;
}

bool WindowsSystem::WaitForChildExit(const std::string& processKey, DWORD timeoutMs) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return true; // 已不存在
    HANDLE hProcess = it->second.hProcess;
    if (!hProcess) return true;
    DWORD ret = WaitForSingleObject(hProcess, timeoutMs);
    return (ret == WAIT_OBJECT_0);
}

bool WindowsSystem::TerminateChildProcess(const std::string& processKey, DWORD gracefulTimeoutMs) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return true;

    ChildProcessInfo& info = it->second;

    // 发送 Exit 事件（如果存在）
    auto evIt = info.events.find("Exit");
    if (evIt != info.events.end() && evIt->second) {
        SetEvent(evIt->second);
    }

    // 等待进程自行退出
    if (info.hProcess) {
        DWORD waitResult = WaitForSingleObject(info.hProcess, gracefulTimeoutMs);
        if (waitResult == WAIT_TIMEOUT) {
            TerminateProcess(info.hProcess, 0);
        }
    }

    CleanupChildProcess(processKey);
    return true;
}

bool WindowsSystem::IsChildRunning(const std::string& processKey) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return false;
    HANDLE hProcess = it->second.hProcess;
    if (!hProcess) return false;
    DWORD exitCode;
    if (GetExitCodeProcess(hProcess, &exitCode)) {
        return (exitCode == STILL_ACTIVE);
    }
    return false;
}

DWORD WindowsSystem::GetSharedMemorySize(const std::string& processKey, const std::string& blockName) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return 0;
    auto& info = it->second;
    auto blkIt = info.sharedMems.find(blockName);
    if (blkIt == info.sharedMems.end()) return 0;
    return blkIt->second.size;
}

void WindowsSystem::CleanupChildProcess(const std::string& processKey) {
    auto it = childProcesses.find(processKey);
    if (it == childProcesses.end()) return;
    ChildProcessInfo& info = it->second;

    // 关闭事件句柄
    for (auto& pair : info.events) {
        if (pair.second) CloseHandle(pair.second);
    }
    info.events.clear();

    // 卸载共享内存
    for (auto& pair : info.sharedMems) {
        SharedMemBlock& block = pair.second;
        if (block.pView) UnmapViewOfFile(block.pView);
        if (block.hSharedMem) CloseHandle(block.hSharedMem);
    }
    info.sharedMems.clear();

    // 关闭进程/线程句柄
    if (info.hThread) CloseHandle(info.hThread);
    if (info.hProcess) CloseHandle(info.hProcess);

    childProcesses.erase(it);
}

bool WindowsSystem::OpenProjectStructureViewer(const wchar_t* ViewerExePath, const wchar_t* ProjectRoot) {
    ChildProcessConfig config;
    config.processKey = "ProjectStructureViewer";
    config.exePath = ViewerExePath;
    if (ProjectRoot && wcslen(ProjectRoot) > 0) {
        config.commandLineArgs = std::wstring(L"\"") + ProjectRoot + L"\"";
    }
    config.createNewConsole = true;
    config.redirectStdIO = true;

    // 原有：ProjectStructureViewer 自身使用的共享内存和事件
    config.sharedMemBlocks["Path"] = MAX_PATH;
    config.eventsToCreate["Exit"] = false;
    config.eventsToCreate["Refresh"] = false;

    if (!LaunchChildProcessW(config)) {
        return false;
    }

    if (currentProjectDirectory) {
        WriteToSharedMemory("ProjectStructureViewer", "Path", currentProjectDirectory, strlen(currentProjectDirectory) + 1);
    }

    // === 新增：为 FolderViewer 创建 IPC 对象 ===
    const std::string key = "ProjectStructureViewer";
    auto it = childProcesses.find(key);
    if (it == childProcesses.end()) return false;
    ChildProcessInfo& info = it->second;

    // 定义三个通道的名称后缀
    const std::vector<std::pair<std::string, std::string>> channels = {
        {"OpenLevel", "OpenLevelPath"},
        {"OpenBlueprint", "OpenBlueprintPath"},
        {"OpenText", "OpenTextPath"}
    };

    for (const auto& ch : channels) {
        const std::string& eventSuffix = ch.first;
        const std::string& memSuffix = ch.second;

        // 构造 FolderViewer 期望的全局名称
        std::string globalEventName = "Global\\OFGal_Engine_ProjectStructureViewer_FolderViewer_" + eventSuffix;
        std::string globalMemName = "Global\\OFGal_Engine_ProjectStructureViewer_FolderViewer_" + memSuffix;

        // 创建事件（手动重置？此处使用自动重置事件，与 FolderViewer 中一致）
        HANDLE hEvent = CreateEventA(NULL, FALSE, FALSE, globalEventName.c_str());
        if (!hEvent) {
            OutputDebugStringA(("CreateEvent failed for FolderViewer: " + globalEventName).c_str());
            return false;
        }
        info.events[eventSuffix] = hEvent; // 存入 events 映射，以便 Run() 中访问

        // 创建共享内存（大小 4096 字节，足够存放路径）
        const DWORD memSize = 4096;
        HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, memSize, globalMemName.c_str());
        if (!hMap) {
            OutputDebugStringA(("CreateFileMapping failed for FolderViewer: " + globalMemName).c_str());
            CloseHandle(hEvent);
            return false;
        }

        char* pView = static_cast<char*>(MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, memSize));
        if (!pView) {
            OutputDebugStringA("MapViewOfFile failed for FolderViewer");
            CloseHandle(hMap);
            CloseHandle(hEvent);
            return false;
        }
        memset(pView, 0, memSize);

        SharedMemBlock block;
        block.hSharedMem = hMap;
        block.pView = pView;
        block.size = memSize;
        block.isReadOnly = false;
        info.sharedMems[memSuffix] = block;
    }

    return true;
}
bool WindowsSystem::OpenLevelTreeList() {
    ChildProcessConfig config;
    config.processKey = "LevelTreeList";
    config.exePath = exePath_LevelTreeList;
    // 如果需要传递命令行参数，可以设置 config.commandLineArgs
    config.createNewConsole = true;      // 必须分配新的控制台窗口
    config.redirectStdIO = true;         // 与 ProjectStructureViewer 保持一致

    // 可选：添加一个 Exit 事件，便于后续优雅关闭
    config.eventsToCreate["Exit"] = false;

    // 如果子进程需要共享内存，可以在此添加 config.sharedMemBlocks

    if (!LaunchChildProcessW(config)) {
        OutputDebugStringA("[WindowsSystem] Failed to launch LevelTreeList.exe\n");
        return false;
    }

    std::string levelTreeEventName = "Global\\OFGal_Engine_LevelTreeList_PathUpdate";
    m_hLevelTreeListPathUpdateEvent = CreateEventA(NULL, TRUE, FALSE, levelTreeEventName.c_str());
    if (!m_hLevelTreeListPathUpdateEvent)
        OutputDebugStringA(("CreateEvent failed for LevelTreeList: " + levelTreeEventName).c_str());

    OutputDebugStringA("[WindowsSystem] LevelTreeList.exe launched successfully\n");
    return true;
}

void WindowsSystem::CreateBPEditorIPC() {
    std::string globalEventName = "Global\\OFGal_Engine_BlueprintViewer_LoadBP";
    m_hBPEditorLoadEvent = CreateEventA(NULL, FALSE, FALSE, globalEventName.c_str());
    if (!m_hBPEditorLoadEvent) {
        OutputDebugStringA(("CreateEvent failed: " + globalEventName).c_str());
    }

    std::string globalMemName = "Global\\OFGal_Engine_BlueprintViewer_BlueprintPath";
    m_hBPEditorPathMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BPEDITOR_PATH_SIZE, globalMemName.c_str());
    if (!m_hBPEditorPathMap) {
        OutputDebugStringA(("CreateFileMapping failed: " + globalMemName).c_str());
    }
    else {
        m_pBPEditorPathView = static_cast<char*>(MapViewOfFile(m_hBPEditorPathMap, FILE_MAP_WRITE, 0, 0, BPEDITOR_PATH_SIZE));
        if (!m_pBPEditorPathView) {
            OutputDebugStringA("MapViewOfFile failed for BPEditorPath");
            CloseHandle(m_hBPEditorPathMap);
            m_hBPEditorPathMap = NULL;
        }
        else {
            memset(m_pBPEditorPathView, 0, BPEDITOR_PATH_SIZE);
        }
    }
}

void WindowsSystem::DestroyBPEditorIPC() {
    if (m_pBPEditorPathView) {
        UnmapViewOfFile(m_pBPEditorPathView);
        m_pBPEditorPathView = nullptr;
    }
    if (m_hBPEditorPathMap) {
        CloseHandle(m_hBPEditorPathMap);
        m_hBPEditorPathMap = NULL;
    }
    if (m_hBPEditorLoadEvent) {
        CloseHandle(m_hBPEditorLoadEvent);
        m_hBPEditorLoadEvent = NULL;
    }
}

void WindowsSystem::Start_BPEditor() {
    auto it = childProcesses.find("BlueprintViewer");
    if (it != childProcesses.end()) {
        CleanupChildProcess("BlueprintViewer");
    }

    std::wstring cmdLine = L"\"" + exePath_BlueprintViewer + L"\"";
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };
    DWORD creationFlags = CREATE_NEW_CONSOLE;

    if (!CreateProcessW(nullptr, cmdBuffer.data(), nullptr, nullptr, FALSE, creationFlags, nullptr, nullptr, &si, &pi)) {
        OutputDebugStringA("Failed to start BlueprintViewer");
        return;
    }

    ChildProcessInfo info;
    info.hProcess = pi.hProcess;
    info.hThread = pi.hThread;         // 保留句柄，勿关闭
    info.processId = pi.dwProcessId;
    childProcesses["BlueprintViewer"] = info;
}

void WindowsSystem::Notify_BPEditor(std::wstring BlueprintPath) {
    if (!m_pBPEditorPathView) return;

    // 写入宽字符路径
    size_t maxBytes = BPEDITOR_PATH_SIZE;
    size_t pathBytes = (BlueprintPath.length() + 1) * sizeof(wchar_t);
    if (pathBytes > maxBytes) pathBytes = maxBytes;
    memcpy(m_pBPEditorPathView, BlueprintPath.c_str(), pathBytes);

    // 确保终止符
    if (pathBytes >= sizeof(wchar_t)) {
        reinterpret_cast<wchar_t*>(m_pBPEditorPathView)[(pathBytes / sizeof(wchar_t)) - 1] = L'\0';
    }

    // 触发加载事件
    if (m_hBPEditorLoadEvent) {
        SetEvent(m_hBPEditorLoadEvent);
    }
}

void WindowsSystem::Run() {
    const std::string key = "ProjectStructureViewer";
    auto it = childProcesses.find(key);
    if (it == childProcesses.end()) return;

    ChildProcessInfo& info = it->second;

    // 获取三个事件句柄（FolderViewer 专用）
    HANDLE hOpenLevel = info.events["OpenLevel"];
    HANDLE hOpenBlueprint = info.events["OpenBlueprint"];
    HANDLE hOpenText = info.events["OpenText"];
    HANDLE hExit = info.events["Exit"]; // 原有 Exit 事件

    HANDLE events[4] = { hOpenLevel, hOpenBlueprint, hOpenText, hExit };
    const DWORD numEvents = 4;

    while (true) {
        DWORD result = WaitForMultipleObjects(numEvents, events, FALSE, 20);
        if (result == WAIT_OBJECT_0 + 3) // Exit 事件
            break;

        std::wstring* targetPath = nullptr;
        std::string blockName;

        if (result == WAIT_OBJECT_0) {
            targetPath = &m_lastOpenedLevelPath;
            blockName = "OpenLevelPath";
            if (m_hLevelTreeListPathUpdateEvent) {
                SetEvent(m_hLevelTreeListPathUpdateEvent);
            }
        }
        else if (result == WAIT_OBJECT_0 + 1) {
            targetPath = &m_lastOpenedBlueprintPath;
            blockName = "OpenBlueprintPath";

            auto blockIt = info.sharedMems.find(blockName);
            if (blockIt != info.sharedMems.end() && blockIt->second.pView) {
                const WCHAR* pWide = reinterpret_cast<const WCHAR*>(blockIt->second.pView);
                *targetPath = std::wstring(pWide);
                OutputDebugStringW((std::wstring(L"[OFGal_Engine] Opened file: ") + *targetPath + L"\n").c_str());
            }

            HWND hwndBP = FindWindowW(NULL, L"OFGal_Engine/BlueprintViewer");
            if (!hwndBP) {
                Start_BPEditor();
                Sleep(100);
            }
            Notify_BPEditor(m_lastOpenedBlueprintPath);
        }
        else if (result == WAIT_OBJECT_0 + 2) {
            targetPath = &m_lastOpenedTextPath;
            blockName = "OpenTextPath";
        }
        else {
            bool shouldRun = false;
            // ========== 输入轮询 ==========
            if (this->m_currentLevel) {
                m_inputCollector.update();

                std::vector<InputEvent> eventsCopy = m_inputSystem.getEvents();
                m_inputSystem.clearEvent();

                for (const auto& ev : eventsCopy) {
                    if (ev.type == InputType::KeyDown) {
                        switch (ev.key) {
                        case KeyCode::Enter:
                            shouldRun = true;
                            break;
                        default:
                            break;
                        }
                    }
                }

                if (shouldRun) {
                    OutputDebugStringW(L"[OFGal_Engine] Start Running GameVM...\n");
                    BlueprintScheduler vm;
                    vm.Start(this->m_currentLevel);
                    system("cls");
                }
            }
            else {
                continue;
            }

        }

        auto blockIt = info.sharedMems.find(blockName);
        if (blockIt != info.sharedMems.end() && blockIt->second.pView) {
            // 共享内存中存储的是 WCHAR 字符串
            const WCHAR* pWide = reinterpret_cast<const WCHAR*>(blockIt->second.pView);
            *targetPath = std::wstring(pWide);
            // 可选：输出调试信息或调用回调
            OutputDebugStringW((std::wstring(L"[OFGal_Engine] Opened file: ") + *targetPath + L"\n").c_str());

            if (result == WAIT_OBJECT_0) {
                //delete currentLevel;
                std::string pathStr = WideToUtf8(*targetPath);
                LevelData* loadedLevel = new LevelData(_EventBus::getInstance().publish_ReadLevelData(pathStr));
                if (loadedLevel) {
                    m_currentLevel = loadedLevel;
                    OutputDebugStringA("[OFGal_Engine] Level data loaded and set as current.\n");
                }
                else {
                    OutputDebugStringA("[OFGal_Engine] Failed to load level data.\n");
                }
            }
        }
    }
}

bool WindowsSystem::RefreshProjectStructureViewer() {
    const std::string key = "ProjectStructureViewer";
    if (!IsChildRunning(key)) return false;

    // 更新路径
    if (currentProjectDirectory) {
        WriteToSharedMemory(key, "Path", currentProjectDirectory, strlen(currentProjectDirectory) + 1);
    }
    // 触发 Refresh 事件
    return SignalEvent(key, "Refresh");
}

bool WindowsSystem::CloseProjectStructureViewer() {
    return TerminateChildProcess("ProjectStructureViewer", 1000);
}

// 辅助函数：将 UTF-8 字符串转换为宽字符串
static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) return std::wstring();
    std::wstring wide(len - 1, L'\0');   // 长度已包含结尾 null，resize 时不需额外空间
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], len);
    return wide;
}

void WindowsSystem::ToTextBlock(const std::string& text, const std::string& name,
    int X, int Y, int cx, int cy) {
    const size_t MAX_WCHARS = 300;                     // 最多 300 个 UTF‑16 字符
    const DWORD SHARED_MEM_SIZE = MAX_WCHARS * sizeof(wchar_t);

    // 检查该 name 是否已有对应的子进程
    auto itProc = m_hTextBlockProcess.find(name);
    if (itProc != m_hTextBlockProcess.end()) {
        // ===== 情况 1：进程已存在 =====
        auto itMem = m_hTextBlockSharedMems.find(name);
        auto itEv = m_hTextBlockEvents.find(name);
        if (itMem == m_hTextBlockSharedMems.end() || itEv == m_hTextBlockEvents.end()) {
            OutputDebugStringA("[TextBlock] Shared mem or event missing for existing process\n");
            return;
        }

        // 转换为 UTF‑16 并截断
        std::wstring wtext = Utf8ToWide(text);
        if (wtext.length() > MAX_WCHARS) {
            wtext.resize(MAX_WCHARS);
        }
        // 写入时需包含结尾 null，所以字节数为 (len+1)*sizeof(wchar_t)
        size_t bytesToWrite = (wtext.length() + 1) * sizeof(wchar_t);
        if (bytesToWrite > SHARED_MEM_SIZE) {
            bytesToWrite = SHARED_MEM_SIZE;            // 防御性保护
        }

        // 映射共享内存视图并写入
        void* pView = MapViewOfFile(itMem->second, FILE_MAP_WRITE, 0, 0, bytesToWrite);
        if (!pView) {
            OutputDebugStringA("[TextBlock] MapViewOfFile failed\n");
            return;
        }
        memcpy(pView, wtext.c_str(), bytesToWrite);
        UnmapViewOfFile(pView);

        // 激活事件，通知子进程有新文本
        SetEvent(itEv->second);

    }
    else {
        // ===== 情况 2：首次调用，需要启动子进程 =====
        // 将 name 转换为宽字符串以便构造命令行
        std::wstring wname = Utf8ToWide(name);
        std::wstring exePath = exePath_TextBlock;

        // 构建命令行：cmd /c "exePath" name X Y cx cy
        std::wstring cmdLine = L"cmd.exe /c \"" + exePath + L"\" " +
            wname + L" " +
            std::to_wstring(X) + L" " +
            std::to_wstring(Y) + L" " +
            std::to_wstring(cx) + L" " +
            std::to_wstring(cy);
        std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
        cmdBuffer.push_back(L'\0');

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = { 0 };
        DWORD creationFlags = CREATE_NEW_CONSOLE;       // 分配独立控制台窗口

        BOOL success = CreateProcessW(
            nullptr,               // 从命令行解析可执行文件
            cmdBuffer.data(),
            nullptr, nullptr,
            FALSE,
            creationFlags,
            nullptr, nullptr,
            &si, &pi
        );

        if (!success) {
            OutputDebugStringA(("[TextBlock] CreateProcessW failed, error: " +
                std::to_string(GetLastError())).c_str());
            return;
        }

        CloseHandle(pi.hThread);                        // 只需保留进程句柄
        m_hTextBlockProcess[name] = pi.hProcess;

        // 创建自动重置事件（初始无信号）
        std::wstring eventGlobalName = L"Global\\OFGal_Engine_TextBlock_" + wname + L"_PrintText";
        HANDLE hEvent = CreateEventW(nullptr, FALSE, FALSE, eventGlobalName.c_str());
        if (!hEvent) {
            OutputDebugStringA("[TextBlock] CreateEvent failed\n");
        }
        m_hTextBlockEvents[name] = hEvent;

        // 创建共享内存（大小足以容纳 300 个 UTF‑16 字符）
        std::wstring memGlobalName = L"Global\\OFGal_Engine_TextBlock_" + wname + L"_SharedMem";
        HANDLE hSharedMem = CreateFileMappingW(
            INVALID_HANDLE_VALUE, nullptr,
            PAGE_READWRITE,
            0, SHARED_MEM_SIZE,
            memGlobalName.c_str()
        );
        if (!hSharedMem) {
            OutputDebugStringA("[TextBlock] CreateFileMapping failed\n");
        }
        m_hTextBlockSharedMems[name] = hSharedMem;

        // 现在进程、事件、共享内存均已就绪，递归调用自身执行写入与通知
        ToTextBlock(text, name, X, Y, cx, cy);
    }
}
void WindowsSystem::ClearTextBlocks() {
    // 1. 强制终止所有 TextBlock 子进程并关闭进程句柄
    for (auto& pair : m_hTextBlockProcess) {
        HANDLE hProcess = pair.second;
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }
    m_hTextBlockProcess.clear();

    // 2. 关闭所有事件句柄
    for (auto& pair : m_hTextBlockEvents) {
        if (pair.second) {
            CloseHandle(pair.second);
        }
    }
    m_hTextBlockEvents.clear();

    // 3. 关闭所有共享内存句柄
    for (auto& pair : m_hTextBlockSharedMems) {
        if (pair.second) {
            CloseHandle(pair.second);
        }
    }
    m_hTextBlockSharedMems.clear();
}