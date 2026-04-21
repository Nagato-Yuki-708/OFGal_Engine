// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#include "LevelTreeList.h"
#include "Json_LevelData_ReadWrite.h"
#include <iostream>
#include <string>
#include <codecvt>
#include <locale>

// 全局输入系统实例（定义在 InputSystem.cpp）
extern InputSystem g_inputSystem;

// 辅助函数：将宽字符串转换为 UTF-8 字符串
static std::string WstrToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

LevelTreeList::LevelTreeList()
    : m_running(false)
    , m_hEvent(NULL)
    , m_hMapFile(NULL)
    , m_pView(nullptr)
    , m_inputSystem(&g_inputSystem)
    , m_loopInterval(std::chrono::milliseconds(20))
{
}

LevelTreeList::~LevelTreeList() {
    CloseIPC();
}

void LevelTreeList::ConfigureConsole() {
    // 启用虚拟终端处理
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }

    // 禁用快速编辑模式
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hIn, &dwMode)) {
            dwMode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_INSERT_MODE);
            dwMode |= ENABLE_EXTENDED_FLAGS;
            SetConsoleMode(hIn, dwMode);
        }
    }
}

void LevelTreeList::SetWindowSizeAndPosition() {
    HWND hwndConsole = GetConsoleWindow();
    if (hwndConsole) {
        HWND MAX_Window = FindWindow(NULL, L"OFGal_Engine");
        float scaleX = 1920.0f / 2560.0f;
        float scaleY = 1080.0f / 1600.0f;
        if (MAX_Window) {
            RECT rect;
            if (GetWindowRect(MAX_Window, &rect)) {
                int width = rect.right - rect.left;
                int height = rect.bottom - rect.top;
                scaleX = (float)width / 2560.0f;
                scaleY = (float)height / 1600.0f;
            }
        }
        SetWindowPos(hwndConsole, nullptr,
            static_cast<int>(2040.0f * scaleX),
            static_cast<int>(150.0f * scaleY),
            static_cast<int>(520.0f * scaleX),
            static_cast<int>(500.0f * scaleY),
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

bool LevelTreeList::OpenIPC() {
    const std::wstring eventName = L"Global\\OFGal_Engine_LevelTreeList_PathUpdate";

    const std::wstring sharedMemName =
        L"Global\\OFGal_Engine_ProjectStructureViewer_"
        L"FolderViewer_OpenLevelPath";

    // 1. 打开事件
    m_hEvent = OpenEventW(EVENT_ALL_ACCESS, FALSE, eventName.c_str());
    if (m_hEvent == NULL) {
        DWORD err = GetLastError();
        WCHAR buf[512];
        swprintf_s(buf, L"[LevelTreeList] Failed to open event: %s, error: %lu\n", eventName.c_str(), err);
        OutputDebugStringW(buf);
        if (err == ERROR_FILE_NOT_FOUND) {
            OutputDebugStringW(L"[LevelTreeList] Event object not found. Ensure main program is running.\n");
        }
        return false;
    }
    WCHAR buf[256];
    swprintf_s(buf, L"[LevelTreeList] Event opened successfully. Handle: %p\n", m_hEvent);
    OutputDebugStringW(buf);

    // 2. 打开共享内存
    m_hMapFile = OpenFileMappingW(FILE_MAP_READ, FALSE, sharedMemName.c_str());
    if (m_hMapFile == NULL) {
        DWORD err = GetLastError();
        WCHAR buf[512];
        swprintf_s(buf, L"[LevelTreeList] Failed to open shared memory: %s, error: %lu\n", sharedMemName.c_str(), err);
        OutputDebugStringW(buf);
        CloseHandle(m_hEvent);
        m_hEvent = NULL;
        return false;
    }
    swprintf_s(buf, L"[LevelTreeList] Shared memory opened successfully. Handle: %p\n", m_hMapFile);
    OutputDebugStringW(buf);

    // 3. 映射视图
    m_pView = MapViewOfFile(m_hMapFile, FILE_MAP_READ, 0, 0, 0);
    if (m_pView == NULL) {
        WCHAR buf[128];
        swprintf_s(buf, L"[LevelTreeList] Failed to map view, error: %lu\n", GetLastError());
        OutputDebugStringW(buf);
        CloseHandle(m_hMapFile);
        m_hMapFile = NULL;
        CloseHandle(m_hEvent);
        m_hEvent = NULL;
        return false;
    }
    swprintf_s(buf, L"[LevelTreeList] Shared memory view mapped. Address: %p\n", m_pView);
    OutputDebugStringW(buf);

    return true;
}

void LevelTreeList::CloseIPC() {
    if (m_pView) {
        UnmapViewOfFile(m_pView);
        m_pView = nullptr;
    }
    if (m_hMapFile) {
        CloseHandle(m_hMapFile);
        m_hMapFile = NULL;
    }
    if (m_hEvent) {
        CloseHandle(m_hEvent);
        m_hEvent = NULL;
    }
}

bool LevelTreeList::Initialize() {
    // 配置控制台
    ConfigureConsole();
    SetConsoleTitleW(L"OFGal_Engine/LevelTreeList");
    SetWindowSizeAndPosition();

    OutputDebugStringW(L"[LevelTreeList] Started. Initializing IPC...\n");

    if (!OpenIPC()) {
        system("pause");
        return false;
    }

    // 初始化输入收集器
    m_inputCollector = std::make_unique<InputCollector>(m_inputSystem);
    m_inputCollector->AddBinding({ 'A', Modifier::None, KeyCode::A, true });
    m_inputCollector->AddBinding({ 'D', Modifier::None, KeyCode::D, true });
    m_inputCollector->AddBinding({ 'F', Modifier::None, KeyCode::F, true });

    OutputDebugStringW(L"[LevelTreeList] Input bindings configured. Entering main loop...\n");

    m_running = true;
    return true;
}

void LevelTreeList::HandleOpenLevelEvent() {
    OutputDebugStringW(L"[LevelTreeList] OpenLevel event triggered!\n");

    const WCHAR* path = static_cast<const WCHAR*>(m_pView);
    if (path == nullptr || wcslen(path) == 0) {
        OutputDebugStringW(L"[LevelTreeList] ERROR: Shared memory path is empty or null.\n");
        return;
    }

    std::wstring openedLevelPath(path);
    std::wstring msg = L"[LevelTreeList] Path from shared memory: " + openedLevelPath + L"\n";
    OutputDebugStringW(msg.c_str());

    // 转换为 UTF-8 并读取 .level 文件
    std::string utf8Path = WstrToUtf8(openedLevelPath);
    try {
        LevelData level = ReadLevelData(utf8Path);
        m_currentLevel = std::make_unique<LevelData>(std::move(level));
        std::string countMsg = "Level loaded successfully. Object count: " + std::to_string(m_currentLevel->objects.size()) + "\n";
        OutputDebugStringA(countMsg.c_str());
    }
    catch (const std::exception& e) {
        std::string errMsg = "Failed to read level file: " + std::string(e.what()) + "\n";
        OutputDebugStringA(errMsg.c_str());
    }

    if (m_hEvent) {
        ResetEvent(m_hEvent);
    }
}

void LevelTreeList::ProcessInputEvents() {
    const auto& events = m_inputSystem->getEvents();
    for (const auto& ev : events) {
        if (ev.type == InputType::KeyDown) {
            switch (ev.key) {
            case KeyCode::A:
                system("cls");
                std::cout << "A/D: go back/next object" << std::endl;
                std::cout << "F: more ops" << std::endl;
                OutputDebugStringW(L"up\n");
                break;
            case KeyCode::D:
                system("cls");
                std::cout << "A/D: go back/next object" << std::endl;
                std::cout << "F: more ops" << std::endl;
                OutputDebugStringW(L"down\n");
                break;
            case KeyCode::F:
                system("cls");
                std::cout << "A/D: go back/next object" << std::endl;
                std::cout << "F: more ops" << std::endl;
                OutputDebugStringW(L"F\n");
                break;
            default:
                break;
            }
        }
    }
}

void LevelTreeList::Run() {
    OutputDebugStringW(L"[LevelTreeList] Entering main loop.\n");

    // 检查事件句柄
    if (m_hEvent == NULL || m_hEvent == INVALID_HANDLE_VALUE) {
        OutputDebugStringW(L"[LevelTreeList] ERROR: Event handle is invalid!\n");
        return;
    }

    // 初始状态检查（调试用）
    DWORD initialWait = WaitForSingleObject(m_hEvent, 0);
    WCHAR buf[128];
    swprintf_s(buf, L"[LevelTreeList] Initial event state: %lu (0=signaled, 258=timeout, 0xFFFFFFFF=error)\n", initialWait);
    OutputDebugStringW(buf);

    while (m_running) {
        // 1. 检查 IPC 事件（非阻塞）
        DWORD waitResult = WaitForSingleObject(m_hEvent, 0);
        if (waitResult == WAIT_OBJECT_0) {
            OutputDebugStringW(L"[LevelTreeList] Event signaled!\n");
            HandleOpenLevelEvent();
        }
        else if (waitResult == WAIT_TIMEOUT) {
            // 正常超时，不输出（避免刷屏）
        }
        else {
            WCHAR errBuf[128];
            swprintf_s(errBuf, L"[LevelTreeList] WaitForSingleObject error: %lu, handle: %p\n", GetLastError(), m_hEvent);
            OutputDebugStringW(errBuf);
            m_running = false;
            break;
        }

        // 2. 更新输入收集器
        m_inputCollector->update();

        // 3. 处理输入事件
        ProcessInputEvents();

        // 4. 清除本帧事件
        m_inputSystem->clearEvent();

        // 5. 休眠 20ms
        std::this_thread::sleep_for(m_loopInterval);
    }

    OutputDebugStringW(L"[LevelTreeList] Exited cleanly.\n");
}

void LevelTreeList::Stop() {
    m_running = false;
}