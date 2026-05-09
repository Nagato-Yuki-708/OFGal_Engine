// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#include "BlueprintViewer.h"

BlueprintViewer::BlueprintViewer() {
    SetConsoleTitleW(L"OFGal_Engine/BlueprintViewer");
    ConfigureConsole();
    SetWindowSizeAndPosition();

    // ---------- 初始化同步对象 ----------
    hLoadBPEvent = OpenEventW(
        EVENT_MODIFY_STATE | SYNCHRONIZE,
        FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_LoadBP");
    if (!hLoadBPEvent) {
        DWORD err = GetLastError();
        DEBUG_W(L"[BlueprintViewer] OpenEvent LoadBP Failed, error=" << err << L"\n");
    }

    hFileMapping = OpenFileMappingW(
        FILE_MAP_READ,
        FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_BlueprintPath");
    if (hFileMapping) {
        pSharedMem = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
        if (!pSharedMem) {
            DEBUG_W(L"[BlueprintViewer] MapViewOfFile Failed" << L"\n");
        }
    }
    else {
        pSharedMem = nullptr;
        DEBUG_W(L"[BlueprintViewer] OpenFileMapping Failed, error=" << GetLastError() << L"\n");
    }

    hNodeViewerEvent = CreateEventW(
        NULL, FALSE, FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_NodeViewer_LoadBP");
    if (!hNodeViewerEvent) {
        DEBUG_W(L"[BlueprintViewer] CreateEvent NodeViewer Failed" << L"\n");
    }

    hVariablesViewerEvent = CreateEventW(
        NULL, FALSE, FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_VariablesViewer_LoadBP");
    if (!hVariablesViewerEvent) {
        DEBUG_W(L"[BlueprintViewer] CreateEvent VariablesViewer Failed" << L"\n");
    }

    hNodeChangedEvent = CreateEventW(
        NULL, FALSE, FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_NodeViewer_NodeChanged");
    if (!hNodeChangedEvent) {
        DEBUG_W(L"[BlueprintViewer] CreateEvent NodeChanged Failed" << L"\n");
    }

    hVarChangedEvent = CreateEventW(
        NULL, FALSE, FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_VariablesViewer_VarChanged");
    if (!hVarChangedEvent) {
        DEBUG_W(L"[BlueprintViewer] CreateEvent VarChanged Failed" << L"\n");
    }

    // ---------- 创建 NodeViewer 移动通知用事件与共享内存 ----------
    hNodeMoveEvent = CreateEventW(
        NULL, FALSE, FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_NodeViewer_NodeMove");
    if (!hNodeMoveEvent) {
        DEBUG_W(L"[BlueprintViewer] CreateEvent NodeMove Failed, error=" << GetLastError() << L"\n");
    }

    hNodeIdMapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(int) * 2,
        L"Global\\OFGal_Engine_BlueprintViewer_NodeViewer_NodeId");
    if (hNodeIdMapping) {
        pNodeIdSharedMem = static_cast<int*>(
            MapViewOfFile(hNodeIdMapping, FILE_MAP_WRITE, 0, 0, sizeof(int) * 2));
        if (!pNodeIdSharedMem) {
            DEBUG_W(L"[BlueprintViewer] MapViewOfFile NodeId Failed, error=" << GetLastError() << L"\n");
        }
        // 初始化值为当前选中节点（可能为 -1）
        pNodeIdSharedMem[0] = selectedNodeId1;
        pNodeIdSharedMem[1] = selectedNodeId2;
    }
    else {
        pNodeIdSharedMem = nullptr;
        DEBUG_W(L"[BlueprintViewer] CreateFileMapping NodeId Failed, error=" << GetLastError() << L"\n");
    }

    // ---------- 按键绑定 ----------
    m_inputSystem.SetGlobalCapture(false);
    m_inputSystem.SetWindowHandle(GetConsoleWindow());

    m_inputCollector.AddBinding({ 'W',        Modifier::None, KeyCode::W,      true });
    m_inputCollector.AddBinding({ 'S',        Modifier::None, KeyCode::S,      true });
    m_inputCollector.AddBinding({ VK_UP,      Modifier::None, KeyCode::Up,     true });
    m_inputCollector.AddBinding({ VK_DOWN,    Modifier::None, KeyCode::Down,   true });
    m_inputCollector.AddBinding({ 'A',        Modifier::None, KeyCode::A,      true });
    m_inputCollector.AddBinding({ 'D',        Modifier::None, KeyCode::D,      true });
    m_inputCollector.AddBinding({ 'F',        Modifier::None, KeyCode::F,      true });
    m_inputCollector.AddBinding({ VK_DELETE,  Modifier::None, KeyCode::Delete, true });

    // ---------- 启动子进程 ----------
    if (!FindWindowW(NULL, L"OFGal_Engine/NodeViewer"))
    {
        LaunchChildProcess(exePath_NodeViewer);
    }
    else
    {
        if (!hNodeChangedEvent)
        {
            SetEvent(hNodeViewerEvent);
        }
    }
    if(!FindWindowW(NULL, L"OFGal_Engine/VariablesViewer"))
    {
        LaunchChildProcess(exePath_VariablesViewer);
    }
    else
    {
        if (!hVarChangedEvent)
        {
            SetEvent(hVariablesViewerEvent);
        }
    }
}

BlueprintViewer::~BlueprintViewer() {
    if (pSharedMem)      UnmapViewOfFile(pSharedMem);
    if (hFileMapping)    CloseHandle(hFileMapping);
    if (hLoadBPEvent)    CloseHandle(hLoadBPEvent);
    if (hNodeViewerEvent) CloseHandle(hNodeViewerEvent);
    if (hVariablesViewerEvent) CloseHandle(hVariablesViewerEvent);
    if (hNodeChangedEvent) CloseHandle(hNodeChangedEvent);
    if (hVarChangedEvent) CloseHandle(hVarChangedEvent);
    if (pNodeIdSharedMem) UnmapViewOfFile(pNodeIdSharedMem);
    if (hNodeIdMapping)   CloseHandle(hNodeIdMapping);
    if (hNodeMoveEvent)   CloseHandle(hNodeMoveEvent);

    for (HANDLE h : childProcesses) {
        if (h) CloseHandle(h);
    }
}

bool BlueprintViewer::LaunchChildProcess(const std::wstring& exePath) {
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = {};

    BOOL success = CreateProcessW(
        exePath.c_str(),
        NULL,
        NULL, NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL, NULL,
        &si, &pi);

    if (success) {
        childProcesses.push_back(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    else {
        DEBUG_W(L"[BlueprintViewer] LaunchChildProcess Failed for " << exePath << L", error=" << GetLastError() << L"\n");
        return false;
    }
}

void BlueprintViewer::AddColorSpan(RenderBlock& block, int row, int startVisCol, int endVisCol, const char* color) {
    if (!color) return;
    if (row >= (int)block.spans.size())
        block.spans.resize(row + 1);
    block.spans[row].push_back({ startVisCol, endVisCol, color });
}

void BlueprintViewer::MergeChildSpans(RenderBlock& parent, const RenderBlock& child,
    int rowOffset, int colOffset) {
    for (size_t r = 0; r < child.spans.size(); ++r) {
        int targetRow = rowOffset + (int)r;
        if (targetRow >= (int)parent.spans.size())
            parent.spans.resize(targetRow + 1);
        for (const auto& sp : child.spans[r]) {
            ColorSpan shifted = sp;
            shifted.startCol += colOffset;
            shifted.endCol += colOffset;
            parent.spans[targetRow].push_back(shifted);
        }
    }
}

const char* BlueprintViewer::GetTopBorderColor(int nodeId, int sel1, int sel2) {
    if (nodeId == sel1 && nodeId == sel2) return CYAN;
    if (nodeId == sel1) return CYAN;
    if (nodeId == sel2) return ORANGE;
    return nullptr;
}

const char* BlueprintViewer::GetBottomBorderColor(int nodeId, int sel1, int sel2) {
    if (nodeId == sel1 && nodeId == sel2) return ORANGE;
    if (nodeId == sel1) return CYAN;
    if (nodeId == sel2) return ORANGE;
    return nullptr;
}

void BlueprintViewer::Run() {
    HANDLE eventsToWait[3] = { hLoadBPEvent, hNodeChangedEvent, hVarChangedEvent };
    DWORD numEvents = 3;
    if (!hLoadBPEvent) {
        eventsToWait[0] = hNodeChangedEvent;
        eventsToWait[1] = hVarChangedEvent;
        numEvents = 2;
    }

    for (;;) {
        DWORD dwWait = WaitForMultipleObjects(numEvents, eventsToWait, FALSE, 20);

        // 处理事件
        if (dwWait == WAIT_OBJECT_0) {
            // LoadBP 事件
            if (pSharedMem) {
                try {
                    const wchar_t* pPath = static_cast<const wchar_t*>(pSharedMem);
                    currentBPPath = pPath;
                    std::string filepath = WideToUTF8(currentBPPath);
                    currentBPData = ReadBPData(filepath);

                    auto entryIds = GetEntryNodeIds();
                    if (!entryIds.empty()) {
                        currentEntryIndex = 0;
                        currentEntryNodeId = entryIds[0];
                    }
                    else {
                        currentEntryIndex = 0;
                        currentEntryNodeId = -1;
                    }

                    // 重置选中节点（将在 BuildAndPrintCurrentFlow 中更新）
                    selectedNodeId1 = currentEntryNodeId;
                    selectedNodeId2 = currentEntryNodeId;

                    AdjustBufferSize();

                    if (hNodeViewerEvent) SetEvent(hNodeViewerEvent);
                    if (hVariablesViewerEvent) SetEvent(hVariablesViewerEvent);

                    RenderAll();
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
            if (hLoadBPEvent)
                ResetEvent(hLoadBPEvent);
        }
        else if (dwWait == WAIT_OBJECT_0 + 1 ||
            (numEvents == 2 && eventsToWait[0] == hNodeChangedEvent && dwWait == WAIT_OBJECT_0)) {
            DEBUG_W(L"[BlueprintViewer] NodeChanged event signaled\n");
            SetEvent(hLoadBPEvent);
        }
        else if (dwWait == WAIT_OBJECT_0 + 2 ||
            (numEvents == 2 && eventsToWait[1] == hVarChangedEvent && dwWait == WAIT_OBJECT_0)) {
            DEBUG_W(L"[BlueprintViewer] VarChanged event signaled\n");
            SetEvent(hLoadBPEvent);
        }
        else if (dwWait == WAIT_FAILED) {
            break;
        }

        // 输入监听
        if (!isEditing) {
            m_inputCollector.update();
        }

        std::vector<InputEvent> eventsCopy = m_inputSystem.getEvents();
        m_inputSystem.clearEvent();

        for (const auto& ev : eventsCopy) {
            if (ev.type == InputType::KeyDown) {
                switch (ev.key) {
                case KeyCode::W:   
                    MoveSelection1Up(); 
                    if (pNodeIdSharedMem) {
                        pNodeIdSharedMem[0] = selectedNodeId1;
                        pNodeIdSharedMem[1] = selectedNodeId2;
                    }
                    if (hNodeMoveEvent) SetEvent(hNodeMoveEvent);
                    break;
                case KeyCode::S:   
                    MoveSelection1Down(); 
                    if (pNodeIdSharedMem) {
                        pNodeIdSharedMem[0] = selectedNodeId1;
                        pNodeIdSharedMem[1] = selectedNodeId2;
                    }
                    if (hNodeMoveEvent) SetEvent(hNodeMoveEvent);
                    break;
                case KeyCode::Up:  
                    MoveSelection2Up(); 
                    if (pNodeIdSharedMem) {
                        pNodeIdSharedMem[0] = selectedNodeId1;
                        pNodeIdSharedMem[1] = selectedNodeId2;
                    }
                    if (hNodeMoveEvent) SetEvent(hNodeMoveEvent);
                    break;
                case KeyCode::Down:
                    MoveSelection2Down(); 
                    if (pNodeIdSharedMem) {
                        pNodeIdSharedMem[0] = selectedNodeId1;
                        pNodeIdSharedMem[1] = selectedNodeId2;
                    }
                    if (hNodeMoveEvent) SetEvent(hNodeMoveEvent);
                    break;
                case KeyCode::A:   
                    MoveToPrevFlow(); 
                    if (pNodeIdSharedMem) {
                        pNodeIdSharedMem[0] = selectedNodeId1;
                        pNodeIdSharedMem[1] = selectedNodeId2;
                    }
                    if (hNodeMoveEvent) SetEvent(hNodeMoveEvent);
                    break;
                case KeyCode::D:   
                    MoveToNextFlow(); 
                    if (pNodeIdSharedMem) {
                        pNodeIdSharedMem[0] = selectedNodeId1;
                        pNodeIdSharedMem[1] = selectedNodeId2;
                    }
                    if (hNodeMoveEvent) SetEvent(hNodeMoveEvent);
                    break;
                case KeyCode::F:
                    isEditing = true;
                    if(Edit())
                    {
                        SetEvent(hLoadBPEvent);
                        SetEvent(hNodeViewerEvent);
                        SetEvent(hVariablesViewerEvent);
                    }
                    else{
                        ClearScreen();
                        RenderAll();
                    }
                    isEditing = false;
                    break;
                case KeyCode::Delete:
                    isEditing = true;
                    if(OnDelete())
                    {
                        pNodeIdSharedMem[0] = selectedNodeId1;
                        pNodeIdSharedMem[1] = selectedNodeId2;
                        SetEvent(hLoadBPEvent);
                        SetEvent(hNodeViewerEvent);
                        SetEvent(hVariablesViewerEvent);
                    }
                    else {
                        ClearScreen();
                        RenderAll();
                    }
                    isEditing = false;
                    break;
                default: break;
                }
            }
        }
    }
}

std::string BlueprintViewer::WideToUTF8(const std::wstring& wstr) const {
    if (wstr.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

void BlueprintViewer::ConfigureConsole() {
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

void BlueprintViewer::ClearScreen() {
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

void BlueprintViewer::FlushInputBuffer() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(hIn);
}

int BlueprintViewer::GetConsoleColumns() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return 80;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
}

int BlueprintViewer::GetMaxNodeId() const {
    int maxId = -1;
    for (const auto& node : currentBPData.nodes) {
        if (node.id > maxId) {
            maxId = node.id;
        }
    }
    return maxId;
}

void BlueprintViewer::SetWindowSizeAndPosition() {
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
            static_cast<int>(0.0f),
            static_cast<int>(0.0f),
            static_cast<int>(2040.0f * scaleX),
            static_cast<int>(1010.0f * scaleY),
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void BlueprintViewer::BuildAndPrintHelpText() {
    int cols = GetConsoleColumns();
    std::string separator(cols, '=');
    std::ostringstream oss;

    oss << CYAN << "=== Help ===" << RESET << "\n";
    oss << separator << "\n";

    oss << CYAN << "W" << RESET << " - Move selection 1 up\n";
    oss << CYAN << "S" << RESET << " - Move selection 1 down\n";
    oss << CYAN << "Up" << RESET << " - Move selection 2 up\n";
    oss << CYAN << "Down" << RESET << " - Move selection 2 down\n";
    oss << CYAN << "Delete" << RESET << " - Remove selected node 1 and all its descendants\n";
    oss << CYAN << "F" << RESET << " - Add a new Node\n";
    oss << CYAN << "A" << RESET << " - Previous execution flow\n";
    oss << CYAN << "D" << RESET << " - Next execution flow\n";

    oss << separator << "\n";
    std::cout << oss.str();
}

void BlueprintViewer::MoveSelection1Up() {
    if (m_flowNodeOrder.empty()) return;
    auto it = std::find(m_flowNodeOrder.begin(), m_flowNodeOrder.end(), selectedNodeId1);
    if (it == m_flowNodeOrder.end()) {
        selectedNodeId1 = m_flowNodeOrder.front();
    }
    else {
        if (it == m_flowNodeOrder.begin())
            it = m_flowNodeOrder.end() - 1;
        else
            --it;
        selectedNodeId1 = *it;
    }
    RenderAll();
}
void BlueprintViewer::MoveSelection1Down() {
    if (m_flowNodeOrder.empty()) return;
    auto it = std::find(m_flowNodeOrder.begin(), m_flowNodeOrder.end(), selectedNodeId1);
    if (it == m_flowNodeOrder.end()) {
        selectedNodeId1 = m_flowNodeOrder.front();
    }
    else {
        ++it;
        if (it == m_flowNodeOrder.end())
            it = m_flowNodeOrder.begin();
        selectedNodeId1 = *it;
    }
    RenderAll();
}
void BlueprintViewer::MoveSelection2Up() {
    if (m_flowNodeOrder.empty()) return;
    auto it = std::find(m_flowNodeOrder.begin(), m_flowNodeOrder.end(), selectedNodeId2);
    if (it == m_flowNodeOrder.end()) {
        selectedNodeId2 = m_flowNodeOrder.front();
    }
    else {
        if (it == m_flowNodeOrder.begin())
            it = m_flowNodeOrder.end() - 1;
        else
            --it;
        selectedNodeId2 = *it;
    }
    RenderAll();
}
void BlueprintViewer::MoveSelection2Down() {
    if (m_flowNodeOrder.empty()) return;
    auto it = std::find(m_flowNodeOrder.begin(), m_flowNodeOrder.end(), selectedNodeId2);
    if (it == m_flowNodeOrder.end()) {
        selectedNodeId2 = m_flowNodeOrder.front();
    }
    else {
        ++it;
        if (it == m_flowNodeOrder.end())
            it = m_flowNodeOrder.begin();
        selectedNodeId2 = *it;
    }
    RenderAll();
}
void BlueprintViewer::MoveToPrevFlow() {
    auto entryIds = GetEntryNodeIds();
    if (entryIds.empty()) return;
    if (currentEntryIndex <= 0)
        currentEntryIndex = static_cast<int>(entryIds.size()) - 1;
    else
        --currentEntryIndex;
    currentEntryNodeId = entryIds[currentEntryIndex];
    RenderAll();
}

void BlueprintViewer::MoveToNextFlow() {
    auto entryIds = GetEntryNodeIds();
    if (entryIds.empty()) return;
    if (currentEntryIndex >= static_cast<int>(entryIds.size()) - 1)
        currentEntryIndex = 0;
    else
        ++currentEntryIndex;
    currentEntryNodeId = entryIds[currentEntryIndex];
    RenderAll();
}
bool BlueprintViewer::OnDelete() {
    ClearScreen();
    FlushInputBuffer();

    // ---------- 1. 检验选中是否有效 ----------
    auto nodeExists = [&](int id) {
        for (const auto& nd : currentBPData.nodes) {
            if (nd.id == id) return true;
        }
        return false;
        };

    if (!nodeExists(selectedNodeId1) || !nodeExists(selectedNodeId2)) {
        std::cout << "Invalid selection. One or both selected nodes do not exist.\n";
        std::cout << "Press Enter to continue...\n";
        std::cin.get();   // 等待回车
        return false;
    }

    // ---------- 2. 选择操作对象 ----------
    int targetNodeId = -1;
    while (true) {
        ClearScreen();
        std::cout << "Which selection to delete?\n";
        std::cout << "1 - Node ID " << selectedNodeId1 << " (Selection 1)\n";
        std::cout << "2 - Node ID " << selectedNodeId2 << " (Selection 2)\n";
        std::cout << "Enter \"#esc#\" to cancel.\n> ";

        std::string line;
        if (!std::getline(std::cin, line)) {
            // 输入流异常，取消操作
            return false;
        }
        if (line == "#esc#") {
            return false;
        }
        if (line == "1") {
            targetNodeId = selectedNodeId1;
            break;
        }
        else if (line == "2") {
            targetNodeId = selectedNodeId2;
            break;
        }
        // 其他输入忽略，循环重试
    }

    // 获取目标节点信息
    const Node* targetNode = nullptr;
    for (const auto& nd : currentBPData.nodes) {
        if (nd.id == targetNodeId) {
            targetNode = &nd;
            break;
        }
    }
    if (!targetNode) {
        return false;
    }

    // ---------- 3. 确认删除 ----------
    ClearScreen();
    std::cout << "Are you sure you want to delete the following node and ALL its successors?\n\n";
    std::cout << "  Node ID   : " << targetNode->id << "\n";
    std::cout << "  Node Type : " << targetNode->type << "\n\n";
    std::cout << "Confirm? (Y/N)  Enter \"#esc#\" to cancel.\n> ";

    std::string confirm;
    while (true) {
        if (!std::getline(std::cin, confirm)) {
            return false;
        }
        if (confirm == "#esc#" || confirm == "n" || confirm == "N") {
            return false;
        }
        if (confirm == "y" || confirm == "Y") {
            break;
        }
        std::cout << "Invalid input. Type Y/N or #esc# to cancel.\n> ";
    }

    // ---------- 4. 执行删除（在副本上操作） ----------
    auto getDescendants = [&](int startId) -> std::vector<int> {
        std::unordered_set<int> visited;
        std::function<void(int)> dfs = [&](int id) {
            if (visited.count(id)) return;
            visited.insert(id);
            const Node* node = nullptr;
            for (const auto& n : currentBPData.nodes) {
                if (n.id == id) { node = &n; break; }
            }
            if (!node) return;
            for (const auto& pin : node->pins) {
                if (pin.io == "O" && pin.type == "exec") {
                    for (const auto& link : currentBPData.links) {
                        if (link.sourceNode == id && link.sourcePin == pin.name) {
                            dfs(link.targetNode);
                        }
                    }
                }
            }
            };
        dfs(startId);
        return std::vector<int>(visited.begin(), visited.end());
        };

    std::vector<int> idsToDelete = getDescendants(targetNodeId);

    BlueprintData temp = currentBPData;

    // 删除节点
    temp.nodes.erase(
        std::remove_if(temp.nodes.begin(), temp.nodes.end(),
            [&](const Node& n) {
                return std::find(idsToDelete.begin(), idsToDelete.end(), n.id) != idsToDelete.end();
            }),
        temp.nodes.end());

    // 删除涉及的连接
    temp.links.erase(
        std::remove_if(temp.links.begin(), temp.links.end(),
            [&](const Link& l) {
                return std::find(idsToDelete.begin(), idsToDelete.end(), l.sourceNode) != idsToDelete.end()
                    || std::find(idsToDelete.begin(), idsToDelete.end(), l.targetNode) != idsToDelete.end();
            }),
        temp.links.end());

    // 删除事件
    temp.events.erase(
        std::remove_if(temp.events.begin(), temp.events.end(),
            [&](const Event& e) {
                return std::find(idsToDelete.begin(), idsToDelete.end(), e.id) != idsToDelete.end();
            }),
        temp.events.end());

    // 写回文件（需要 WideToUTF8 转换）
    std::string pathStr = WideToUTF8(currentBPPath);
    WriteBPData(pathStr, temp);

    return true;
}
bool BlueprintViewer::Edit() {
    ClearScreen();
    FlushInputBuffer();

    // ======================== 辅助 Lambda ========================
    auto readLine = [&]() -> std::string {
        std::string line;
        if (!std::getline(std::cin, line)) return "#esc#";
        return line;
        };
    auto isEsc = [&](const std::string& s) { return s == "#esc#"; };
    auto printBold = [](const std::string& text) {
        std::cout << CYAN << text << RESET;
        };

    auto getMaxId = [&](const std::vector<Node>& nodes) -> int {
        int m = -1;
        for (const auto& n : nodes)
            if (n.id > m) m = n.id;
        return m;
        };

    auto getTemplatePins = [&](const std::string& type, const std::string& dataType = "") -> std::vector<Pin> {
        std::vector<Pin> pins;
        if (type == "Add" || type == "Sub" || type == "Mul" || type == "Div") {
            std::string dt = dataType.empty() ? "int" : dataType;
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"A","I",dt,std::nullopt},
                {"B","I",dt,std::nullopt},
                {"OEXEC","O","exec",std::nullopt},
                {"result","O",dt,std::nullopt}
            };
        }
        else if (type == "GreaterThan" || type == "LessThan" || type == "GreaterThanOrEqualTo" ||
            type == "LessThanOrEqualTo" || type == "EqualTo" || type == "NotEqualTo") {
            std::string dt = dataType.empty() ? "int" : dataType;
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"A","I",dt,std::nullopt},
                {"B","I",dt,std::nullopt},
                {"OEXEC","O","exec",std::nullopt},
                {"result","O","bool",std::nullopt}
            };
        }
        else if (type == "if") {
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"shouldRunA","I","bool",std::nullopt},
                {"OEXEC_A","O","exec",std::nullopt},
                {"OEXEC_B","O","exec",std::nullopt}
            };
        }
        else if (type == "while") {
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"shouldRunLoop","I","bool",std::nullopt},
                {"OEXEC","O","exec",std::nullopt},
                {"OEXEC_Loop","O","exec",std::nullopt}
            };
        }
        else if (type == "break") {
            pins = { {"IEXEC","I","exec",std::nullopt} };
        }
        else if (type == "continue") {
            pins = { {"IEXEC","I","exec",std::nullopt} };
        }
        else if (type == "GetVariable") {
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"VarToGet","I","string",std::nullopt},
                {"OEXEC","O","exec",std::nullopt},
                {"VarCopy","O",dataType.empty() ? "int" : dataType,std::nullopt}
            };
        }
        else if (type == "SetVariable") {
            std::string dt = dataType.empty() ? "int" : dataType;
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"VarToSet","I","string",std::nullopt},
                {"NewValue","I",dt,std::nullopt},
                {"OEXEC","O","exec",std::nullopt},
                {"VarCopy","O",dt,std::nullopt}
            };
        }
        else if (type == "PrintText") {
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"Text","I","string",std::nullopt},
                {"OEXEC","O","exec",std::nullopt}
            };
        }
        else if (type == "Render") {
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"Frame","O","frame",std::nullopt},
                {"OEXEC","O","exec",std::nullopt}
            };
        }
        else if (type == "FrameProcess") {
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"ProcessOp","I","string",std::nullopt},
                {"FrameToProcess","I","frame",std::nullopt},
                {"OEXEC","O","exec",std::nullopt},
                {"Frame","O","frame",std::nullopt}
            };
        }
        else if (type == "ShowtheFrame") {
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"Frame","I","frame",std::nullopt},
                {"OEXEC","O","exec",std::nullopt}
            };
        }
        else if (type == "PlaySound") {
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"Path","I","string",std::nullopt},
                {"shouldLoop","I","bool",std::nullopt},
                {"Volume","I","float",std::nullopt},
                {"OEXEC","O","exec",std::nullopt}
            };
        }
        else if (type == "StopSound") {
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"Path","I","string",std::nullopt},
                {"OEXEC","O","exec",std::nullopt}
            };
        }
        else if (type == "SetTransform") {
            pins = {
                {"IEXEC","I","exec",std::nullopt},
                {"Location_x","I","float",std::nullopt},
                {"Location_y","I","float",std::nullopt},
                {"Location_z","I","int",std::nullopt},
                {"Rotation","I","float",std::nullopt},
                {"Scale_x","I","float",std::nullopt},
                {"Scale_y","I","float",std::nullopt},
                {"OEXEC","O","exec",std::nullopt}
            };
        }
        else if (type == "Exit") {
            pins = { {"IEXEC","I","exec",std::nullopt} };
        }
        else if (type == "BeginPlay") {
            pins = { {"OEXEC","O","exec",std::nullopt} };
        }
        else if (type == "Play_per_N_ms") {
            pins = {
                {"Time","I","int",std::nullopt},
                {"OEXEC","O","exec",std::nullopt}
            };
        }
        else if (type == "Play_when_N_push_down") {
            pins = {
                {"Btn","I","string",std::nullopt},
                {"OEXEC","O","exec",std::nullopt}
            };
        }
        else if (type == "Play_when_triggered") {
            pins = { {"OEXEC","O","exec",std::nullopt} };
        }
        return pins;
        };

    auto getExecOutputs = [](const Node& node) -> std::vector<std::string> {
        std::vector<std::string> outs;
        for (const auto& p : node.pins)
            if (p.io == "O" && p.type == "exec")
                outs.push_back(p.name);
        return outs;
        };

    auto printNodeSummary = [&](const Node& node) {
        std::cout << "  Node ID   : " << node.id << "\n";
        std::cout << "  Node Type : " << node.type << "\n";
        std::cout << "  Pins      :\n";
        for (const auto& p : node.pins) {
            std::cout << "    " << p.name << " (" << p.io << ", " << p.type << ")";
            if (p.literal.has_value())
                std::cout << " = \"" << p.literal.value() << "\"";
            std::cout << "\n";
        }
        if (!node.properties.empty()) {
            std::cout << "  Properties:\n";
            for (const auto& kv : node.properties)
                std::cout << "    " << kv.first << " : " << kv.second << "\n";
        }
        };

    // ======================== 0. 判断蓝图是否为空 ========================
    BlueprintData temp = currentBPData;
    bool isEmpty = temp.nodes.empty();

    // ======================== 1. 选择操作模式 ========================
    enum class EditMode { EntryNode, InsertNormal };
    EditMode mode;

    if (isEmpty) {
        ClearScreen();
        std::cout << "Blueprint is empty. You must create an entry node first.\n";
        std::cout << "Press Enter to continue...";
        std::cin.get();
        mode = EditMode::EntryNode;
    }
    else {
        while (true) {
            ClearScreen();
            printBold("Choose operation:\n\n");
            std::cout << "  [1]  Create a new entry node (BeginPlay, Play_per_N_ms, ...)\n";
            std::cout << "  [2]  Insert a normal node after a selected node\n";
            std::cout << "\nEnter the number or \"#esc#\" to cancel: > ";
            std::string line = readLine();
            if (isEsc(line)) return false;
            if (!line.empty()) {
                char ch = (char)tolower((unsigned char)line[0]);
                if (ch == '1') { mode = EditMode::EntryNode; break; }
                else if (ch == '2') { mode = EditMode::InsertNormal; break; }
                else {
                    std::cout << "\nYou must input number \"1\" or \"2\" to continue!Or \"#esc#\" to cancel." << std::endl;
                    system("pause");
                }
            }
        }
    }

    int targetNodeId = -1;
    const Node* targetNode = nullptr;

    // ======================== 2. 插入普通节点时选择目标 ========================
    if (mode == EditMode::InsertNormal) {
        while (true) {
            ClearScreen();
            printBold("Insert a new node after selection.\n\n");
            std::cout << "  1 - Node ID " << selectedNodeId1 << " (Selection 1)\n";
            std::cout << "  2 - Node ID " << selectedNodeId2 << " (Selection 2)\n";
            std::cout << "\nEnter number or \"#esc#\" to cancel: > ";
            std::string line = readLine();
            if (isEsc(line)) return false;
            if (line == "1") { targetNodeId = selectedNodeId1; break; }
            else if (line == "2") { targetNodeId = selectedNodeId2; break; }
        }
        for (const auto& n : temp.nodes)
            if (n.id == targetNodeId) { targetNode = &n; break; }
        if (!targetNode) {
            std::cout << "Target node not found. Press Enter...\n";
            std::cin.get();
            return false;
        }
    }

    // ======================== 3. 选择节点类型 ========================
    static const std::vector<std::string> nodeTypes = {
        "Add","Sub","Mul","Div",
        "GreaterThan","LessThan","GreaterThanOrEqualTo","LessThanOrEqualTo","EqualTo","NotEqualTo",
        "if","while","break","continue",
        "GetVariable","SetVariable","PrintText",
        "Render","FrameProcess","ShowtheFrame",
        "PlaySound","StopSound","SetTransform","Exit",
        "BeginPlay","Play_per_N_ms","Play_when_N_push_down","Play_when_triggered"
    };

    int chosenIdx = -1;
    while (true) {
        ClearScreen();
        printBold("Select node type to insert:\n\n");
        int half = (int)nodeTypes.size() / 2 + (int)nodeTypes.size() % 2;
        for (int i = 0; i < half; ++i) {
            int left = i, right = i + half;
            std::ostringstream leftStr, rightStr;
            leftStr << " " << std::setw(2) << (left + 1) << ". " << nodeTypes[left];
            if (right < (int)nodeTypes.size())
                rightStr << " " << std::setw(2) << (right + 1) << ". " << nodeTypes[right];
            std::cout << leftStr.str() << std::string(30 - leftStr.str().size(), ' ') << rightStr.str() << "\n";
        }
        std::cout << "\nEnter number or \"#esc#\": > ";
        std::string line = readLine();
        if (isEsc(line)) return false;
        try {
            int idx = std::stoi(line) - 1;
            if (idx >= 0 && idx < (int)nodeTypes.size()) {
                chosenIdx = idx;
                break;
            }
        }
        catch (...) {}
    }
    std::string newNodeType = nodeTypes[chosenIdx];

    // ---------- 禁止在普通插入模式下使用入口节点 ----------
    bool isEntryNode = (newNodeType == "BeginPlay" || newNodeType == "Play_per_N_ms" ||
        newNodeType == "Play_when_N_push_down" || newNodeType == "Play_when_triggered");
    if (mode == EditMode::InsertNormal && isEntryNode) {
        ClearScreen();
        std::cout << "Entry nodes cannot be inserted into the middle of an execution flow.\n";
        std::cout << "Use entry node creation mode instead.\nPress Enter...\n";
        std::cin.get();
        return false;
    }

    // 入口模式兼容性检查
    if (mode == EditMode::EntryNode && !isEntryNode) {
        std::cout << "Error: Non-entry node type selected in entry mode. Press Enter...\n";
        std::cin.get();
        return false;
    }

    // ======================== 4. 新建节点并收集信息 ========================
    Node newNode;
    newNode.id = getMaxId(temp.nodes) + 1;
    newNode.type = newNodeType;

    std::string dataType;
    int varIndex = -1;

    // --------------------- 数据类型选择 ---------------------
    if (newNodeType == "Add" || newNodeType == "Sub" || newNodeType == "Mul" || newNodeType == "Div") {
        ClearScreen();
        printBold("Select data type for " + newNodeType + ":\n\n");
        std::vector<std::string> dtypes = { "int","float","string","bool" };
        for (size_t i = 0; i < dtypes.size(); ++i)
            std::cout << "  " << i + 1 << " - " << dtypes[i] << "\n";
        std::cout << "> ";
        std::string line = readLine();
        if (isEsc(line)) return false;
        try {
            int di = std::stoi(line) - 1;
            if (di >= 0 && di < 4) dataType = dtypes[di];
        }
        catch (...) {}
        if (dataType.empty()) { std::cout << "Invalid. Press Enter...\n"; std::cin.get(); return false; }
    }
    else if (newNodeType == "GreaterThan" || newNodeType == "LessThan" || newNodeType == "GreaterThanOrEqualTo" ||
        newNodeType == "LessThanOrEqualTo" || newNodeType == "EqualTo" || newNodeType == "NotEqualTo") {
        ClearScreen();
        printBold("Select data type for comparison:\n\n");
        std::vector<std::string> dtypes = { "int","float" };
        for (size_t i = 0; i < dtypes.size(); ++i)
            std::cout << "  " << i + 1 << " - " << dtypes[i] << "\n";
        std::cout << "> ";
        std::string line = readLine();
        if (isEsc(line)) return false;
        try {
            int di = std::stoi(line) - 1;
            if (di >= 0 && di < 2) dataType = dtypes[di];
        }
        catch (...) {}
        if (dataType.empty()) { std::cout << "Invalid. Press Enter...\n"; std::cin.get(); return false; }
    }
    else if (newNodeType == "GetVariable" || newNodeType == "SetVariable") {
        if (temp.variables.empty()) {
            ClearScreen();
            std::cout << "No variables defined in this blueprint.\n";
            std::cout << "Cannot create " << newNodeType << " node.\nPress Enter...\n";
            std::cin.get();
            return false;
        }
        while (true) {
            ClearScreen();
            printBold("Select an existing variable for " + newNodeType + ":\n\n");
            for (size_t i = 0; i < temp.variables.size(); ++i)
                std::cout << " " << i + 1 << " - " << temp.variables[i].name
                << " (" << temp.variables[i].type << ")\n";
            std::cout << "\nEnter number or \"#esc#\": > ";
            std::string line = readLine();
            if (isEsc(line)) return false;
            try {
                int idx = std::stoi(line) - 1;
                if (idx >= 0 && idx < (int)temp.variables.size()) {
                    varIndex = idx;
                    dataType = temp.variables[varIndex].type;
                    break;
                }
            }
            catch (...) {}
        }
    }

    newNode.pins = getTemplatePins(newNodeType, dataType);

    // 自动设置变量引用字面值
    if (newNodeType == "GetVariable" || newNodeType == "SetVariable") {
        std::string varName = temp.variables[varIndex].name;
        for (auto& pin : newNode.pins) {
            if (newNodeType == "GetVariable" && pin.name == "VarToGet")
                pin.literal = varName;
            else if (newNodeType == "SetVariable" && pin.name == "VarToSet")
                pin.literal = varName;
        }
    }

    // --------------------- 字面值输入 ---------------------
    for (auto& pin : newNode.pins) {
        if (pin.io != "I" || pin.type == "exec" || pin.type == "frame")
            continue;
        if ((newNodeType == "GetVariable" && pin.name == "VarToGet") ||
            (newNodeType == "SetVariable" && pin.name == "VarToSet"))
            continue;

        bool required = false;
        if (newNodeType == "Play_per_N_ms" && pin.name == "Time")
            required = true;
        else if (newNodeType == "Play_when_N_push_down" && pin.name == "Btn")
            required = true;
        else if (newNodeType == "FrameProcess" && pin.name == "ProcessOp")
            required = true;

        while (true) {
            ClearScreen();
            std::cout << "Pin: " << pin.name << " (type: " << pin.type << ")";
            if (required) std::cout << " [REQUIRED]\n";
            if (newNodeType == "FrameProcess") {
                for (std::string op : AllFrameProcessOps)
                    std::cout << " " << op << "; ";
            }else if (newNodeType == "Play_when_N_push_down") {
                for (std::string StdBtn : std::views::keys(StdkeysMap)) {
                    static int BtnPrintCount = 0;
                    std::cout << " " << StdBtn << "; ";
                    ++BtnPrintCount;
                    if (BtnPrintCount == 9) {
                        std::cout << "\n";
                        BtnPrintCount = 0;
                    }
                }
            }else if (newNodeType == "Play_per_N_ms") {
                std::cout << "Required an integer representing time in milliseconds\n";
            }
            std::cout << "\nEnter literal value or press Enter to skip (\"#esc#\" to cancel):\n> ";
            std::string val = readLine();
            if (isEsc(val)) return false;
            if (!val.empty()) {
                bool valid = false;
                if (newNodeType == "FrameProcess") {
                    for (std::string op : AllFrameProcessOps) {
                        if (val == op)
                            valid = true;
                    }
                    if (valid)
                        pin.literal = val;
                    else {
                        std::cout << "Invalid Frame Process Op. Try again.\n";
                        system("pause");
                        continue;
                    }
                }
                else if (newNodeType == "Play_when_N_push_down") {
                    for (std::string StdBtn : std::views::keys(StdkeysMap)) {
                        if (val == StdBtn) {
                            valid = true;
                            break;
                        }
                    }
                    if (valid)
                        pin.literal = val;
                    else {
                        std::cout << "Invalid Button. Try again.\n";
                        system("pause");
                        continue;
                    }
                }
                else if (newNodeType == "Play_per_N_ms") {
                    if (val[0] != '0')
                    {
                        valid = true;
                        for (char c : val) {
                            if (!std::isdigit(static_cast<unsigned char>(c))) 
                            {
                                valid = false;
                                break;
                            }
                        }
                    }
                    if (valid) {
                        pin.literal = val;
                    }
                    else {
                        std::cout << "Invalid time. Try again.\n";
                        system("pause");
                        continue;
                    }
                }
                else {
                    pin.literal = val;
                }
                break;
            }
            else {
                if (required) {
                    std::cout << "This pin requires a literal. Try again.\n";
                    std::cin.get();
                    continue;
                }
                pin.literal = std::nullopt;
                break;
            }
        }
    }

    // --------------------- Render 高级属性 ---------------------
    if (newNodeType == "Render") {
        while (true) {
            ClearScreen();
            std::cout << "Configure advanced settings for Render?\n\n";
            std::cout << "Skip? (Y/N) Enter #esc# to cancel.\n> ";
            std::string ans = readLine();
            if (isEsc(ans)) return false;
            auto toLower = [](std::string s) {
                for (auto& c : s) c = (char)tolower((unsigned char)c);
                return s;
                };
            ans = toLower(ans);
            if (ans == "n" || ans == "no") {
                struct PropEntry {
                    std::string key;
                    std::string desc;
                    std::vector<std::string> validValues;
                };
                std::vector<PropEntry> renderProps = {
                    {"MSAA", "Anti-aliasing sample count (1,2,3,4)", {"1","2","3","4"}},
                    {"samplingMethod", "Texture sampling method (NEAREST, BILINEAR, BICUBIC, ANISOTROPIC)",
                     {"NEAREST","BILINEAR","BICUBIC","ANISOTROPIC"}},
                    {"anisoLevel", "Anisotropic filter level (1-16, only for ANISOTROPIC)", {}}
                };
                std::set<int> chosenProps;
                while (true) {
                    ClearScreen();
                    std::cout << "Available advanced settings for Render:\n";
                    for (size_t i = 0; i < renderProps.size(); ++i)
                        std::cout << " " << i + 1 << ". " << renderProps[i].key << " - " << renderProps[i].desc << "\n";
                    std::cout << "\nEnter comma-separated numbers (e.g. 1,3) or 'all' to add all, or press Enter to skip: > ";
                    std::string sel = readLine();
                    if (isEsc(sel)) return false;
                    if (sel.empty()) break;
                    sel = toLower(sel);
                    if (sel == "all") {
                        for (int i = 0; i < (int)renderProps.size(); ++i) chosenProps.insert(i);
                        break;
                    }
                    std::istringstream iss(sel);
                    std::string token;
                    bool valid = true;
                    while (std::getline(iss, token, ',')) {
                        try {
                            int num = std::stoi(token) - 1;
                            if (num >= 0 && num < (int)renderProps.size())
                                chosenProps.insert(num);
                            else { valid = false; break; }
                        }
                        catch (...) { valid = false; break; }
                    }
                    if (valid && !chosenProps.empty()) break;
                    std::cout << "Invalid selection. Try again.\n";
                    std::cin.get();
                }
                for (int idx : chosenProps) {
                    const PropEntry& entry = renderProps[idx];
                    while (true) {
                        ClearScreen();
                        std::cout << "Setting: " << entry.key << "\n";
                        std::cout << entry.desc << "\n";
                        if (!entry.validValues.empty()) {
                            std::cout << "Valid values: ";
                            for (size_t i = 0; i < entry.validValues.size(); ++i) {
                                if (i) std::cout << ", ";
                                std::cout << entry.validValues[i];
                            }
                            std::cout << "\n";
                        }
                        std::cout << "Enter value (or press Enter to skip this setting): > ";
                        std::string val = readLine();
                        if (isEsc(val)) return false;
                        if (val.empty()) break;
                        if (!entry.validValues.empty()) {
                            bool match = false;
                            for (const auto& v : entry.validValues) {
                                if (val == v) { match = true; break; }
                            }
                            if (!match) {
                                std::cout << "Invalid value. Must be one of the listed options.\n";
                                std::cin.get();
                                continue;
                            }
                        }
                        else {
                            try {
                                int lvl = std::stoi(val);
                                if (lvl < 1 || lvl > 16) {
                                    std::cout << "Value must be between 1 and 16.\n";
                                    std::cin.get();
                                    continue;
                                }
                            }
                            catch (...) {
                                std::cout << "Invalid integer.\n";
                                std::cin.get();
                                continue;
                            }
                        }
                        newNode.properties[entry.key] = val;
                        break;
                    }
                }
                break;
            }
            else if (ans == "y" || ans == "yes") {
                break;
            }
        }
    }
    // --------------------- FrameProcess 高级属性 ---------------------
    else if (newNodeType == "FrameProcess") {
        struct PostEffect {
            std::string key;
            std::string desc;
            std::string defaultVal;
            std::vector<std::string> paramTypes;
            std::vector<std::string> paramDescs;
        };
        std::vector<PostEffect> effects = {
            {"Bloom", "Bloom effect: threshold intensity blurRadius sigma",
             "220.0f,0.8f,4,-1.0f",
             {"float","float","int","float"},
             {"threshold(0-255)","intensity(0-2)","blurRadius(px)","sigma"}},
            {"FXAA", "FXAA anti-aliasing: edgeThreshold edgeThresholdMin spanMax reduceMul reduceMin",
             "0.166f,0.05f,8.0f,0.125f,0.0078f",
             {"float","float","float","float","float"},
             {"edgeThreshold","edgeThresholdMin","spanMax","reduceMul","reduceMin"}},
            {"SMAA", "SMAA anti-aliasing: edgeThreshold maxSearchSteps enableDiag",
             "0.05f,4,true",
             {"float","int","bool"},
             {"edgeThreshold","maxSearchSteps","enableDiag(true/false)"}},
            {"LensDistortion", "Lens distortion: strength centerX centerY",
             "0.0f,0.5f,0.5f",
             {"float","float","float"},
             {"strength(-0.3~0.3)","centerX","centerY"}},
            {"ChromaticAberration", "Chromatic aberration: strength mode centerX centerY",
             "2.0f,0,0.5f,0.5f",
             {"float","int","float","float"},
             {"strength(px)","mode(0=radial,1=horizontal)","centerX","centerY"}},
            {"Blur", "Gaussian blur: radius sigma direction",
             "3.0f,-1.0f,0",
             {"float","float","int"},
             {"radius(px)","sigma","direction(0=2D,1=H,2=V)"}},
            {"Sharpen", "Unsharp mask: strength radius sigma",
             "0.5f,2,-1.0f",
             {"float","int","float"},
             {"strength","radius","sigma"}},
            {"FilmGrain", "Film grain: intensity grainSize dynamic frameId",
             "0.05f,1,true,0",
             {"float","int","bool","int"},
             {"intensity","grainSize","dynamic(true/false)","frameId"}},
            {"Vignette", "Vignette: intensity innerRadius outerRadius centerX centerY exponent",
             "0.3f,0.6f,1.0f,0.5f,0.5f,1.0f",
             {"float","float","float","float","float","float"},
             {"intensity","innerRadius","outerRadius","centerX","centerY","exponent"}},
            {"ColorCorrection", "Color correction: brightness contrast saturation wR wG wB hueShift",
             "0.0f,1.0f,1.0f,1.0f,1.0f,1.0f,0.0f",
             {"float","float","float","float","float","float","float"},
             {"brightness","contrast","saturation","whiteR","whiteG","whiteB","hueShift(deg)"}},
            {"ColorGrading", "Color grading: style intensity customR customG customB",
             "0,0.8f,1.0f,1.0f,1.0f",
             {"int","float","float","float","float"},
             {"style(0-5)","intensity","customR","customG","customB"}}
        };

        while (true) {
            ClearScreen();
            std::cout << "Configure post‑processing settings for FrameProcess?\n\n";
            std::cout << "Skip? (Y/N) If skipped, no effects will be added. Enter #esc# to cancel.\n> ";
            std::string ans = readLine();
            if (isEsc(ans)) return false;
            auto toLower = [](std::string s) {
                for (auto& c : s) c = (char)tolower((unsigned char)c);
                return s;
                };
            ans = toLower(ans);
            if (ans == "n" || ans == "no") {
                std::set<int> chosenEffects;
                while (true) {
                    ClearScreen();
                    std::cout << "Available post-processing effects:\n";
                    for (size_t i = 0; i < effects.size(); ++i)
                        std::cout << " " << i + 1 << ". " << effects[i].key << " - " << effects[i].desc << "\n";
                    std::cout << "\nEnter comma-separated numbers or 'all' to add all, or press Enter to skip: > ";
                    std::string sel = readLine();
                    if (isEsc(sel)) return false;
                    if (sel.empty()) break;
                    sel = toLower(sel);
                    if (sel == "all") {
                        for (int i = 0; i < (int)effects.size(); ++i) chosenEffects.insert(i);
                        break;
                    }
                    std::istringstream iss(sel);
                    std::string token;
                    bool valid = true;
                    while (std::getline(iss, token, ',')) {
                        try {
                            int num = std::stoi(token) - 1;
                            if (num >= 0 && num < (int)effects.size())
                                chosenEffects.insert(num);
                            else { valid = false; break; }
                        }
                        catch (...) { valid = false; break; }
                    }
                    if (valid && !chosenEffects.empty()) break;
                    std::cout << "Invalid selection. Try again.\n";
                    std::cin.get();
                }
                // 修正后的逻辑：跳过时不设置任何属性，只设置用户选择的效果
                for (int idx : chosenEffects) {
                    const PostEffect& fx = effects[idx];
                    while (true) {
                        ClearScreen();
                        std::cout << "Effect: " << fx.key << "\n";
                        std::cout << "Parameters: ";
                        for (size_t i = 0; i < fx.paramTypes.size(); ++i) {
                            if (i) std::cout << ", ";
                            std::cout << fx.paramDescs[i] << "(" << fx.paramTypes[i] << ")";
                        }
                        std::cout << "\n";
                        std::cout << "Default: " << fx.defaultVal << "\n";
                        std::cout << "Enter comma-separated values (floats must end with 'f', e.g. 0.5f),";
                        std::cout << " or press Enter to use default: > ";
                        std::string input = readLine();
                        if (isEsc(input)) return false;
                        if (input.empty()) {
                            newNode.properties[fx.key] = fx.defaultVal;
                            break;
                        }
                        std::vector<std::string> parts;
                        std::istringstream iss(input);
                        std::string part;
                        while (std::getline(iss, part, ',')) {
                            size_t start = part.find_first_not_of(" \t");
                            size_t end = part.find_last_not_of(" \t");
                            if (start == std::string::npos)
                                parts.push_back("");
                            else
                                parts.push_back(part.substr(start, end - start + 1));
                        }
                        if (parts.size() != fx.paramTypes.size()) {
                            std::cout << "Expected " << fx.paramTypes.size() << " values, got " << parts.size() << ". Try again.\n";
                            std::cin.get();
                            continue;
                        }
                        bool ok = true;
                        for (size_t pi = 0; pi < parts.size(); ++pi) {
                            const std::string& p = parts[pi];
                            const std::string& type = fx.paramTypes[pi];
                            if (type == "float") {
                                if (p.size() < 2 || p.back() != 'f') {
                                    ok = false;
                                    std::cout << "Parameter " << (pi + 1) << " must be a float ending with 'f'.\n";
                                    break;
                                }
                                try { std::stof(p.substr(0, p.size() - 1)); }
                                catch (...) {
                                    ok = false;
                                    std::cout << "Parameter " << (pi + 1) << " is not a valid float.\n";
                                    break;
                                }
                            }
                            else if (type == "int") {
                                try { std::stoi(p); }
                                catch (...) {
                                    ok = false;
                                    std::cout << "Parameter " << (pi + 1) << " must be an integer.\n";
                                    break;
                                }
                            }
                            else if (type == "bool") {
                                std::string low = toLower(p);
                                if (low != "true" && low != "false") {
                                    ok = false;
                                    std::cout << "Parameter " << (pi + 1) << " must be 'true' or 'false'.\n";
                                    break;
                                }
                            }
                        }
                        if (!ok) { std::cin.get(); continue; }
                        std::string valueStr;
                        for (size_t pi = 0; pi < parts.size(); ++pi) {
                            if (pi) valueStr += ",";
                            valueStr += parts[pi];
                        }
                        newNode.properties[fx.key] = valueStr;
                        break;
                    }
                }
                break;
            }
            else if (ans == "y" || ans == "yes") {
                // 用户选择跳过，不设置任何效果属性
                break;
            }
        }
    }

    // ======================== 入口节点重复检查 ========================
    if (isEntryNode) {
        bool duplicate = false;
        if (newNodeType == "BeginPlay") {
            for (const auto& node : temp.nodes) {
                if (node.type == "BeginPlay") {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                ClearScreen();
                std::cout << "Error: A BeginPlay entry node already exists.\n";
                std::cout << "Only one BeginPlay is allowed.\nPress Enter...\n";
                std::cin.get();
                return false;
            }
        }
        else if (newNodeType == "Play_when_triggered") {
            for (const auto& node : temp.nodes) {
                if (node.type == "Play_when_triggered") {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                ClearScreen();
                std::cout << "Error: A Play_when_triggered entry node already exists.\n";
                std::cout << "Only one Play_when_triggered is allowed.\nPress Enter...\n";
                std::cin.get();
                return false;
            }
        }
        else if (newNodeType == "Play_per_N_ms") {
            // 检查是否有相同 Time 字面值
            std::string newTimeLit;
            for (const auto& pin : newNode.pins) {
                if (pin.name == "Time" && pin.literal.has_value()) {
                    newTimeLit = pin.literal.value();
                    break;
                }
            }
            for (const auto& node : temp.nodes) {
                if (node.type == "Play_per_N_ms") {
                    for (const auto& pin : node.pins) {
                        if (pin.name == "Time" && pin.literal.has_value() && pin.literal.value() == newTimeLit) {
                            duplicate = true;
                            break;
                        }
                    }
                }
                if (duplicate) break;
            }
            if (duplicate) {
                ClearScreen();
                std::cout << "Error: A Play_per_N_ms entry node with Time = " << newTimeLit << " already exists.\n";
                std::cout << "Please choose a different interval.\nPress Enter...\n";
                std::cin.get();
                return false;
            }
        }
        else if (newNodeType == "Play_when_N_push_down") {
            // 检查是否有相同 Btn 字面值
            std::string newBtnLit;
            for (const auto& pin : newNode.pins) {
                if (pin.name == "Btn" && pin.literal.has_value()) {
                    newBtnLit = pin.literal.value();
                    break;
                }
            }
            for (const auto& node : temp.nodes) {
                if (node.type == "Play_when_N_push_down") {
                    for (const auto& pin : node.pins) {
                        if (pin.name == "Btn" && pin.literal.has_value() && pin.literal.value() == newBtnLit) {
                            duplicate = true;
                            break;
                        }
                    }
                }
                if (duplicate) break;
            }
            if (duplicate) {
                ClearScreen();
                std::cout << "Error: A Play_when_N_push_down entry node with Btn = " << newBtnLit << " already exists.\n";
                std::cout << "Please choose a different button.\nPress Enter...\n";
                std::cin.get();
                return false;
            }
        }
    }

    // ======================== 5. 执行流连接 ========================
    if (isEntryNode) {
        ClearScreen();
        printBold("New entry node:\n\n");
        printNodeSummary(newNode);
        std::cout << "\nAdd this entry node? (Y/N) > ";
        std::string confirm = readLine();
        if (isEsc(confirm) || (confirm != "Y" && confirm != "y")) return false;
        temp.nodes.push_back(newNode);
        WriteBPData(WideToUTF8(currentBPPath), temp);
        return true;
    }

    // ---------- 普通插入 ----------
    auto execOutputs = getExecOutputs(*targetNode);
    if (execOutputs.empty()) {
        ClearScreen();
        std::cout << "Target node has no exec output. Cannot insert.\nPress Enter...\n";
        std::cin.get();
        return false;
    }

    std::string chosenSrcPin;
    if (execOutputs.size() == 1) {
        chosenSrcPin = execOutputs[0];
    }
    else {
        while (true) {
            ClearScreen();
            printBold("Choose which exec output of node " + std::to_string(targetNodeId) + " to break:\n\n");
            for (size_t i = 0; i < execOutputs.size(); ++i)
                std::cout << "  " << i + 1 << " - " << execOutputs[i] << "\n";
            std::cout << "> ";
            std::string line = readLine();
            if (isEsc(line)) return false;
            try {
                int idx = std::stoi(line) - 1;
                if (idx >= 0 && idx < (int)execOutputs.size()) {
                    chosenSrcPin = execOutputs[idx];
                    break;
                }
            }
            catch (...) {}
        }
    }

    // 收集该引脚所有下游
    std::vector<int> originalNextIds;
    for (auto it = temp.links.begin(); it != temp.links.end(); ) {
        if (it->sourceNode == targetNodeId && it->sourcePin == chosenSrcPin) {
            originalNextIds.push_back(it->targetNode);
            it = temp.links.erase(it);
        }
        else {
            ++it;
        }
    }

    // 多后继保护
    if (originalNextIds.size() > 1) {
        ClearScreen();
        std::cout << "ERROR: Target pin has " << originalNextIds.size() << " downstream nodes.\n";
        std::cout << "Inserting a node here would break the execution flow.\n";
        std::cout << "Please disconnect some links before inserting.\n";
        std::cout << "Press Enter to continue...\n";
        std::cin.get();
        return false;
    }

    int originalNext = originalNextIds.empty() ? -1 : originalNextIds[0];
    bool hasSuccessor = (originalNext != -1);

    auto newNodeExecOuts = getExecOutputs(newNode);
    std::string chosenNewOut;

    if (hasSuccessor && newNodeExecOuts.empty()) {
        ClearScreen();
        std::cout << "New node has no exec output pin.\n";
        std::cout << "The connection to node " << originalNext << " will be lost.\n";
        std::cout << "Continue? (Y/N) > ";
        std::string confirm = readLine();
        if (isEsc(confirm) || (confirm != "Y" && confirm != "y")) return false;
    }
    else if (hasSuccessor && newNodeExecOuts.size() == 1) {
        chosenNewOut = newNodeExecOuts[0];
    }
    else if (hasSuccessor && newNodeExecOuts.size() > 1) {
        while (true) {
            ClearScreen();
            printBold("Select new node's exec output to connect to the rest of the flow:\n\n");
            for (size_t i = 0; i < newNodeExecOuts.size(); ++i)
                std::cout << "  " << i + 1 << " - " << newNodeExecOuts[i] << "\n";
            std::cout << "> ";
            std::string line = readLine();
            if (isEsc(line)) return false;
            try {
                int idx = std::stoi(line) - 1;
                if (idx >= 0 && idx < (int)newNodeExecOuts.size()) {
                    chosenNewOut = newNodeExecOuts[idx];
                    break;
                }
            }
            catch (...) {}
        }
    }

    Link link1;
    link1.sourceNode = targetNodeId;
    link1.sourcePin = chosenSrcPin;
    link1.targetNode = newNode.id;
    link1.targetPin = "IEXEC";
    temp.links.push_back(link1);

    if (hasSuccessor && !chosenNewOut.empty()) {
        Link link2;
        link2.sourceNode = newNode.id;
        link2.sourcePin = chosenNewOut;
        link2.targetNode = originalNext;
        link2.targetPin = "IEXEC";
        temp.links.push_back(link2);
    }

    // ======================== 6. 确认并保存 ========================
    ClearScreen();
    printBold("Inserting new node:\n\n");
    printNodeSummary(newNode);
    std::cout << "\nBreaks link from node " << targetNodeId << " (" << targetNode->type
        << ") pin " << chosenSrcPin;
    if (originalNext != -1)
        std::cout << " to node " << originalNext;
    else
        std::cout << " (no successor)";
    std::cout << "\nNew connection: " << targetNodeId << " -> " << newNode.id;
    if (!chosenNewOut.empty())
        std::cout << " -> " << originalNext;
    std::cout << "\n\nConfirm? (Y/N) > ";
    std::string confirm = readLine();
    if (isEsc(confirm) || (confirm != "Y" && confirm != "y")) return false;

    temp.nodes.push_back(newNode);
    WriteBPData(WideToUTF8(currentBPPath), temp);
    return true;
}

std::vector<int> BlueprintViewer::GetEntryNodeIds() const {
    std::vector<int> ids;
    for (const auto& node : currentBPData.nodes) {
        if (node.type == "BeginPlay" ||
            node.type == "Play_per_N_ms" ||
            node.type == "Play_when_N_push_down" ||
            node.type == "Play_when_triggered") {
            ids.push_back(node.id);
        }
    }
    return ids;
}

std::vector<std::pair<int, std::string>> BlueprintViewer::GetEntryNodes() const {
    std::vector<std::pair<int, std::string>> entries;
    for (const auto& node : currentBPData.nodes) {
        if (node.type == "BeginPlay" ||
            node.type == "Play_per_N_ms" ||
            node.type == "Play_when_N_push_down" ||
            node.type == "Play_when_triggered") {
            entries.emplace_back(node.id, node.type);
        }
    }
    return entries;
}

std::unique_ptr<BlueprintViewer::ExecTreeNode> BlueprintViewer::BuildExecTree(int startNodeId) const {
    std::unordered_set<int> visited;
    std::function<std::unique_ptr<ExecTreeNode>(int)> build = [&](int nodeId) -> std::unique_ptr<ExecTreeNode> {
        if (visited.count(nodeId)) return nullptr;
        visited.insert(nodeId);

        const Node* node = nullptr;
        for (const auto& n : currentBPData.nodes) {
            if (n.id == nodeId) { node = &n; break; }
        }
        if (!node) return nullptr;

        auto treeNode = std::make_unique<ExecTreeNode>();
        treeNode->nodeId = node->id;
        treeNode->nodeType = node->type;

        for (const auto& pin : node->pins) {
            if (pin.io == "O" && pin.type == "exec") {
                for (const auto& link : currentBPData.links) {
                    if (link.sourceNode == nodeId && link.sourcePin == pin.name) {
                        auto child = build(link.targetNode);
                        if (child) {
                            std::string label = pin.name;
                            if (label == "OEXEC_A") label = "True";
                            else if (label == "OEXEC_B") label = "False";
                            else if (label == "OEXEC_Loop") label = "Loop";
                            else if (label == "OEXEC") label = "";
                            treeNode->branches.emplace_back(label, std::move(child));
                        }
                    }
                }
            }
        }
        return treeNode;
        };
    return build(startNodeId);
}

void BlueprintViewer::CollectNodeOrder(const ExecTreeNode* node, std::vector<int>& order) const {
    if (!node) return;
    order.push_back(node->nodeId);
    for (const auto& branch : node->branches) {
        CollectNodeOrder(branch.second.get(), order);
    }
}

// 工具：去除ANSI转义序列后的可见字符长度
size_t BlueprintViewer::VisibleLength(const std::string& s) {
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

BlueprintViewer::RenderBlock BlueprintViewer::RenderExecTree(
    const ExecTreeNode* node,
    std::unordered_set<int>& visited,
    int depth,
    int selectedId1,
    int selectedId2) const
{
    RenderBlock result;
    constexpr int MAX_DEPTH = 20;
    if (!node || depth > MAX_DEPTH || visited.count(node->nodeId)) {
        result.lines = { "[...]" };
        result.centerCol = 2;
        return result;
    }
    visited.insert(node->nodeId);

    // ---- 纯文本节点框 ----
    std::string top_raw = "+" + std::string(boxWidth - 2, '-') + "+";
    std::string middle = "| " + node->nodeType +
        std::string(maxNodeNameLength - node->nodeType.size(), ' ') + " |";
    std::string bottom_raw = "+" + std::string(boxWidth - 2, '-') + "+";

    std::vector<std::string> nodeLines = { top_raw, middle, bottom_raw };
    int center = boxWidth / 2;

    // ---- 记录当前节点框的着色区间（仅边框，不包含 middle） ----
    int nodeBaseRow = (int)result.lines.size(); // 将在合并后调整，暂存
    const char* topColor = GetTopBorderColor(node->nodeId, selectedId1, selectedId2);
    const char* bottomColor = GetBottomBorderColor(node->nodeId, selectedId1, selectedId2);
    // 先临时保存，在最终组装时加入

    if (node->branches.empty()) {
        result.lines = nodeLines;
        result.centerCol = center;
        // 着色
        if (topColor)    AddColorSpan(result, 0, 0, boxWidth, topColor);
        if (bottomColor) AddColorSpan(result, 2, 0, boxWidth, bottomColor);
        return result;
    }

    // 2. 递归子分支（纯文本，带 spans）
    std::vector<RenderBlock> childBlocks;
    for (auto& branch : node->branches) {
        childBlocks.push_back(RenderExecTree(branch.second.get(), visited,
            depth + 1, selectedId1, selectedId2));
    }

    if (node->branches.size() == 1) {
        // ===== 单分支 =====
        const auto& child = childBlocks[0];
        int requiredCenter = std::max(center, child.centerCol);
        int parentLeftPad = requiredCenter - center;
        int tempWidth = parentLeftPad + boxWidth;
        int childWidth = (int)child.lines[0].size();   // 纯文本长度 = 可见宽度
        int totalWidth = std::max(tempWidth, childWidth);

        int newCenter = parentLeftPad + center;
        int childLeftPad = newCenter - child.centerCol;
        int childRightPad = totalWidth - (childLeftPad + childWidth);

        // 父节点行填充
        for (auto& line : nodeLines) {
            int len = (int)line.size();
            line = std::string(parentLeftPad, ' ') + line
                + std::string(totalWidth - parentLeftPad - len, ' ');
        }
        result.lines = nodeLines;
        result.centerCol = newCenter;

        // 父节点着色（注意行索引 0,1,2）
        if (topColor)    AddColorSpan(result, 0, parentLeftPad, parentLeftPad + boxWidth, topColor);
        if (bottomColor) AddColorSpan(result, 2, parentLeftPad, parentLeftPad + boxWidth, bottomColor);

        // 连接线
        result.lines.push_back(std::string(totalWidth, ' '));
        result.lines.back()[newCenter] = '|';
        result.lines.push_back(std::string(totalWidth, ' '));
        result.lines.back()[newCenter] = 'v';

        // 子块行（纯文本拼接）
        size_t baseRow = result.lines.size();
        for (auto& line : child.lines) {
            result.lines.push_back(std::string(childLeftPad, ' ') + line +
                std::string(childRightPad, ' '));
        }
        // 合并子块 spans（偏移列 childLeftPad，行偏移 baseRow）
        MergeChildSpans(result, child, (int)baseRow, childLeftPad);
    }
    else {
        // ===== 多分支 =====
        int totalWidth = boxWidth;
        std::vector<int> offsets;
        int gap = 3;
        for (auto& blk : childBlocks) {
            offsets.push_back(totalWidth);
            totalWidth += (int)blk.lines[0].size() + gap;
        }
        if (!offsets.empty()) totalWidth -= gap;

        int parentStart = (totalWidth - boxWidth) / 2;
        // 父节点行（填充）
        {
            std::string topPad(parentStart, ' ');
            result.lines.push_back(topPad + top_raw + std::string(totalWidth - parentStart - boxWidth, ' '));
            result.lines.push_back(topPad + middle + std::string(totalWidth - parentStart - (int)middle.size(), ' '));
            result.lines.push_back(topPad + bottom_raw + std::string(totalWidth - parentStart - boxWidth, ' '));
        }
        int parentCenter = parentStart + center;

        // 父节点着色
        if (topColor)    AddColorSpan(result, 0, parentStart, parentStart + boxWidth, topColor);
        if (bottomColor) AddColorSpan(result, 2, parentStart, parentStart + boxWidth, bottomColor);

        // 连接线
        std::string conn1(totalWidth, ' '); conn1[parentCenter] = '|';
        result.lines.push_back(conn1);
        std::string conn2(totalWidth, ' ');
        for (size_t i = 0; i < childBlocks.size(); ++i) {
            int childCenter = offsets[i] + childBlocks[i].centerCol;
            int left = std::min(parentCenter, childCenter);
            int right = std::max(parentCenter, childCenter);
            for (int x = left; x <= right; ++x) {
                if (x == childCenter)
                    conn2[x] = 'v';
                else if (conn2[x] == ' ')
                    conn2[x] = '-';
            }
        }
        result.lines.push_back(conn2);

        // 子块拼接（纯文本）
        size_t maxChildLines = 0;
        for (auto& blk : childBlocks) maxChildLines = std::max(maxChildLines, blk.lines.size());
        int baseRow = (int)result.lines.size();

        for (size_t row = 0; row < maxChildLines; ++row) {
            std::string line(totalWidth, ' ');
            for (size_t i = 0; i < childBlocks.size(); ++i) {
                if (row < childBlocks[i].lines.size()) {
                    const std::string& childLine = childBlocks[i].lines[row];
                    int visLen = (int)childLine.size();
                    if (offsets[i] + visLen <= totalWidth)
                        line.replace(offsets[i], visLen, childLine);
                }
            }
            result.lines.push_back(line);
        }

        // 合并每个子块的 spans（列偏移为 offsets[i]，行偏移为 baseRow）
        for (size_t i = 0; i < childBlocks.size(); ++i) {
            MergeChildSpans(result, childBlocks[i], baseRow, offsets[i]);
        }
        result.centerCol = parentCenter;
    }

    return result;
}

// ---- 修改后的 PrintRenderBlock ----
void BlueprintViewer::PrintRenderBlock(const RenderBlock& block) {
    for (size_t row = 0; row < block.lines.size(); ++row) {
        const std::string& line = block.lines[row];
        // 获取该行的着色区间，按起始列排序
        std::vector<ColorSpan> rowSpans;
        if (row < block.spans.size())
            rowSpans = block.spans[row];
        std::sort(rowSpans.begin(), rowSpans.end(),
            [](const ColorSpan& a, const ColorSpan& b) { return a.startCol < b.startCol; });

        size_t col = 0;
        for (const auto& sp : rowSpans) {
            // 输出着色区间之前的普通文本
            if (col < (size_t)sp.startCol) {
                std::cout << line.substr(col, sp.startCol - col);
                col = sp.startCol;
            }
            // 输出着色文本
            std::cout << sp.color;
            std::cout << line.substr(col, sp.endCol - col);
            std::cout << RESET;
            col = sp.endCol;
        }
        // 输出剩余部分
        if (col < line.size())
            std::cout << line.substr(col);
        std::cout << '\n';
    }
}

void BlueprintViewer::BuildAndPrintCurrentFlow() {
    auto entryIds = GetEntryNodeIds();
    if (entryIds.empty()) {
        std::cout << "The Blueprint is empty. Please Add your first Entry Node.\n";
        m_flowNodeOrder.clear();
        return;
    }
    if (currentEntryIndex < 0 || currentEntryIndex >= static_cast<int>(entryIds.size()))
        currentEntryIndex = 0;
    currentEntryNodeId = entryIds[currentEntryIndex];

    auto tree = BuildExecTree(currentEntryNodeId);
    if (!tree) {
        std::cout << "Failed to build execution flow.\n";
        m_flowNodeOrder.clear();
        return;
    }

    // 收集节点顺序
    m_flowNodeOrder.clear();
    CollectNodeOrder(tree.get(), m_flowNodeOrder);

    // 如果当前选中节点不在顺序中，重置到入口节点
    if (!m_flowNodeOrder.empty()) {
        if (std::find(m_flowNodeOrder.begin(), m_flowNodeOrder.end(), selectedNodeId1) == m_flowNodeOrder.end())
            selectedNodeId1 = currentEntryNodeId;
        if (std::find(m_flowNodeOrder.begin(), m_flowNodeOrder.end(), selectedNodeId2) == m_flowNodeOrder.end())
            selectedNodeId2 = currentEntryNodeId;
    }
    else {
        selectedNodeId1 = -1;
        selectedNodeId2 = -1;
    }

    std::unordered_set<int> visited;
    auto block = RenderExecTree(tree.get(), visited, 0, selectedNodeId1, selectedNodeId2);
    PrintRenderBlock(block);
}

void BlueprintViewer::AdjustBufferSize() {
    int requiredWidth = 0;
    int requiredHeight = 0;
    auto entryIds = GetEntryNodeIds();
    if (!entryIds.empty()) {
        for (int entryId : entryIds) {
            auto tree = BuildExecTree(entryId);
            if (!tree) continue;
            std::unordered_set<int> visited;
            auto block = RenderExecTree(tree.get(), visited, 0, selectedNodeId1, selectedNodeId2);
            int w = 0;
            for (const auto& line : block.lines) {
                int visLen = (int)VisibleLength(line);
                if (visLen > w) w = visLen;
            }
            int h = (int)block.lines.size();
            requiredWidth = std::max(requiredWidth, w);
            requiredHeight = std::max(requiredHeight, h);
        }
    }
    else {
        requiredWidth = 80;
        requiredHeight = 10;
    }

    requiredWidth += 20;
    requiredHeight += 20;

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;

    int windowWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int windowHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    int newWidth = std::max(requiredWidth, windowWidth + 20);
    int newHeight = std::max(requiredHeight, windowHeight + 20);

    COORD bufferSize;
    bufferSize.X = static_cast<SHORT>(newWidth);
    bufferSize.Y = static_cast<SHORT>(newHeight);
    SetConsoleScreenBufferSize(hOut, bufferSize);
}

void BlueprintViewer::ScrollToTheTop() {
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

void BlueprintViewer::PrintEntryNodes() {
    auto entryNodes = GetEntryNodes();
    int cols = GetConsoleColumns();
    std::string separator(cols, '=');
    std::cout << separator << "\n";

    if (entryNodes.empty()) {
        std::cout << "No entry nodes.\n";
        std::cout << separator << "\n";
        return;
    }

    struct EntryItem {
        std::string plain;
        std::string styled;
    };
    std::vector<EntryItem> items;
    for (size_t i = 0; i < entryNodes.size(); ++i) {
        int id = entryNodes[i].first;
        const std::string& type = entryNodes[i].second;
        std::string plain = "No." + std::to_string(id) + " " + type;
        std::string styled;
        if (static_cast<int>(i) == currentEntryIndex) {
            styled = CYAN + plain + RESET;
        }
        else {
            styled = plain;
        }
        items.push_back({ plain, styled });
    }

    std::ostringstream out;
    int currentLineLen = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        int neededLen = (currentLineLen > 0 ? 1 : 0) + static_cast<int>(item.plain.length());
        if (currentLineLen > 0 && currentLineLen + neededLen > cols) {
            out << "\n";
            currentLineLen = 0;
            neededLen = static_cast<int>(item.plain.length());
        }
        if (currentLineLen > 0) {
            out << " ";
            currentLineLen++;
        }
        out << item.styled;
        currentLineLen += item.plain.length();
    }
    out << "\n";
    std::cout << out.str();
    std::cout << separator << "\n";
}

void BlueprintViewer::RenderAll() {
    ClearScreen();
    BuildAndPrintHelpText();
    PrintEntryNodes();
    int cols = GetConsoleColumns();
    std::string splitLine(cols, '-');
    std::cout << splitLine << "\n";
    BuildAndPrintCurrentFlow();

    ScrollToTheTop();
}