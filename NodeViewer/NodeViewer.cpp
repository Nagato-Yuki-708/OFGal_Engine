// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#include "NodeViewer.h"

NodeViewer::NodeViewer()
{
    SetConsoleTitleW(L"OFGal_Engine/NodeViewer");
    ConfigureConsole();
    SetWindowSizeAndPosition();

    // ---------- 打开 LoadBP 事件 ----------
    hLoadBPEvent = OpenEventW(
        EVENT_MODIFY_STATE | SYNCHRONIZE,
        FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_NodeViewer_LoadBP");
    if (!hLoadBPEvent) {
        DEBUG_W(L"[NodeViewer] OpenEvent LoadBP Failed, error=" << GetLastError() << L"\n");
    }

    // ---------- 打开 NodeChanged 事件 ----------
    hNodeChangedEvent = OpenEventW(
        EVENT_MODIFY_STATE | SYNCHRONIZE,
        FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_NodeViewer_NodeChanged");
    if (!hNodeChangedEvent) {
        DEBUG_W(L"[NodeViewer] OpenEvent NodeChanged Failed, error=" << GetLastError() << L"\n");
    }

    // ---------- 打开 NodeMove 事件 ----------
    hNodeMoveEvent = OpenEventW(
        EVENT_MODIFY_STATE | SYNCHRONIZE,
        FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_NodeViewer_NodeMove");
    if (!hNodeMoveEvent) {
        DEBUG_W(L"[NodeViewer] OpenEvent NodeMove Failed, error=" << GetLastError() << L"\n");
    }

    // ---------- 打开共享内存（BlueprintPath） ----------
    hFileMapping = OpenFileMappingW(
        FILE_MAP_READ,
        FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_BlueprintPath");
    if (hFileMapping) {
        pSharedMem = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
        if (!pSharedMem) {
            DEBUG_W(L"[NodeViewer] MapViewOfFile BlueprintPath Failed" << L"\n");
        }
    }
    else {
        pSharedMem = nullptr;
        DEBUG_W(L"[NodeViewer] OpenFileMapping BlueprintPath Failed, error=" << GetLastError() << L"\n");
    }

    // ---------- 打开共享内存（NodeId） ----------
    hNodeIdMapping = OpenFileMappingW(
        FILE_MAP_READ,
        FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_NodeViewer_NodeId");
    if (hNodeIdMapping) {
        pNodeIdSharedMem = static_cast<int*>(MapViewOfFile(hNodeIdMapping, FILE_MAP_READ, 0, 0, sizeof(int) * 2));
        if (!pNodeIdSharedMem) {
            DEBUG_W(L"[NodeViewer] MapViewOfFile NodeId Failed" << L"\n");
        }
    }
    else {
        pNodeIdSharedMem = nullptr;
        DEBUG_W(L"[NodeViewer] OpenFileMapping NodeId Failed, error=" << GetLastError() << L"\n");
    }

    // ---------- 按键绑定 ----------
    m_inputSystem.SetGlobalCapture(false);
    m_inputSystem.SetWindowHandle(GetConsoleWindow());

    m_inputCollector.AddBinding({ 'W',        Modifier::None, KeyCode::W,      true });
    m_inputCollector.AddBinding({ 'S',        Modifier::None, KeyCode::S,      true });
    m_inputCollector.AddBinding({ VK_UP,      Modifier::None, KeyCode::Up,     true });
    m_inputCollector.AddBinding({ VK_DOWN,    Modifier::None, KeyCode::Down,   true });
    m_inputCollector.AddBinding({ 'J',        Modifier::None, KeyCode::J,      true });
    m_inputCollector.AddBinding({ 'K',        Modifier::None, KeyCode::K,      true });
    m_inputCollector.AddBinding({ 'L',        Modifier::None, KeyCode::L,      true });
}

NodeViewer::~NodeViewer()
{
    if (pNodeIdSharedMem) UnmapViewOfFile(pNodeIdSharedMem);
    if (hNodeIdMapping)   CloseHandle(hNodeIdMapping);
    if (pSharedMem)       UnmapViewOfFile(pSharedMem);
    if (hFileMapping)     CloseHandle(hFileMapping);
    if (hLoadBPEvent)     CloseHandle(hLoadBPEvent);
    if (hNodeChangedEvent) CloseHandle(hNodeChangedEvent);
    if (hNodeMoveEvent)   CloseHandle(hNodeMoveEvent);
}

void NodeViewer::ConfigureConsole() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
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

void NodeViewer::ClearScreen() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;

    DWORD dwSize = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD dwWritten;
    COORD coord = { 0, 0 };

    FillConsoleOutputCharacterW(hOut, L' ', dwSize, coord, &dwWritten);
    FillConsoleOutputAttribute(hOut, csbi.wAttributes, dwSize, coord, &dwWritten);
    SetConsoleCursorPosition(hOut, coord);
}

void NodeViewer::SetWindowSizeAndPosition() {
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
            static_cast<int>(0.0f),
            static_cast<int>(520.0f * scaleX),
            static_cast<int>(1480.0f * scaleY),
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void NodeViewer::FlushInputBuffer() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(hIn);
}

void NodeViewer::ScrollToTheTop() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;
    SMALL_RECT window = csbi.srWindow;
    SHORT height = window.Bottom - window.Top + 1;
    window.Top = 0;
    window.Bottom = height - 1;
    SetConsoleWindowInfo(hOut, TRUE, &window);
}

std::string NodeViewer::WideToUTF8(const std::wstring& wstr) const {
    if (wstr.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

int NodeViewer::GetConsoleColumns() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return 80;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
}

size_t NodeViewer::VisibleLength(const std::string& s) {
    size_t len = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\x1b' && i + 1 < s.size() && s[i + 1] == '[') {
            while (i < s.size() && s[i] != 'm') ++i;
        }
        else {
            ++len;
        }
    }
    return len;
}

void NodeViewer::BuildAndPrintAll() {
    int cols = GetConsoleColumns();
    std::string separator(cols, '=');

    // ---------- 帮助信息 ----------
    std::ostringstream oss;
    oss << separator << "\n";
    oss << CYAN << "W / S" << RESET << " - Move selection 1 up / down\n";
    oss << CYAN << "Up / Down" << RESET << " - Move selection 2 up / down\n";
    oss << CYAN << "J" << RESET << " - Cut selection 1\n";
    oss << CYAN << "K" << RESET << " - Cut selection 2\n";
    oss << CYAN << "L" << RESET << " - Link selection 1 And selection 2\n";
    oss << separator << "\n";
    std::cout << oss.str();

    // ---------- 打印一号节点 ----------

    // ---------- 打印二号节点 ----------
}

void NodeViewer::MoveToPrev(int target) {

}
void NodeViewer::MoveToNext(int target) {

}
bool NodeViewer::Cut(int target) {

}
bool NodeViewer::Link() {

}

void NodeViewer::Run()
{
    // 事件等待数组
    HANDLE eventsToWait[2] = { hLoadBPEvent, hNodeMoveEvent };
    DWORD numEvents = 2;

    for (;;) {
        DWORD dwWait = WaitForMultipleObjects(numEvents, eventsToWait, FALSE, 20);

        // ---- 处理 LoadBP 事件 ----
        if (dwWait == WAIT_OBJECT_0 && hLoadBPEvent) {
            if (pSharedMem) {
                try {
                    const wchar_t* pPath = static_cast<const wchar_t*>(pSharedMem);
                    currentBPPath = pPath;
                    std::string filepath = WideToUTF8(currentBPPath);
                    currentBPData = ReadBPData(filepath);
                    selectedNodeId1 = 0;
                    selectedNodeId2 = 0;
                    ClearScreen();
                    BuildAndPrintAll();
                    ScrollToTheTop();
                }
                catch (const std::exception& e) {
                    ClearScreen();
                    std::cerr << "ERROR: Failed to load blueprint: " << e.what() << std::endl;
                    currentBPData = BlueprintData{};
                    currentBPPath.clear();
                }
                catch (...) {
                    ClearScreen();
                    std::cerr << "ERROR: Unknown error while loading blueprint." << std::endl;
                    currentBPData = BlueprintData{};
                    currentBPPath.clear();
                }
            }
            ResetEvent(hLoadBPEvent);
        }
        // ---- 处理 NodeMove 事件 ----
        else if (dwWait == WAIT_OBJECT_0 + 1 && hNodeMoveEvent) {
            if (pNodeIdSharedMem) {
                selectedNodeId1 = pNodeIdSharedMem[0];
                selectedNodeId2 = pNodeIdSharedMem[1];
                ClearScreen();
                BuildAndPrintAll();
                ScrollToTheTop();
            }
            ResetEvent(hNodeMoveEvent);
        }
        else if (dwWait == WAIT_FAILED) {
            break;
        }

        // ========== 输入轮询 ==========
        if (!isEditing) {
            m_inputCollector.update();
        }

        std::vector<InputEvent> eventsCopy = m_inputSystem.getEvents();
        m_inputSystem.clearEvent();

        for (const auto& ev : eventsCopy) {
            if (ev.type == InputType::KeyDown) {
                switch (ev.key) {
                case KeyCode::W:
                    MoveToPrev(1);
                    break;
                case KeyCode::Up:
                    MoveToPrev(2);
                    break;
                case KeyCode::S:
                    MoveToNext(1);
                    break;
                case KeyCode::Down:
                    MoveToNext(2);
                    break;
                case KeyCode::J:
                    if (Cut(1)) {

                    }
                    else {

                    }
                    break;
                case KeyCode::K:
                    if (Cut(2)) {

                    }
                    else {

                    }
                    break;
                case KeyCode::L:
                    if (Link()) {

                    }
                    else {

                    }
                    break;
                default:
                    break;
                }
            }
        }
    }
}