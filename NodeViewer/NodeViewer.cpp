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
    m_inputCollector.AddBinding({ 'E',        Modifier::None, KeyCode::E,      true });
    m_inputCollector.AddBinding({ 'Q',        Modifier::None, KeyCode::Q,      true });
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

std::string NodeViewer::TruncateText(const std::string& text, int maxWidth) const {
    if (maxWidth <= 0) return "";
    size_t vis = VisibleLength(text);
    if (vis <= static_cast<size_t>(maxWidth)) return text;

    int available = maxWidth - 3;   // 为省略号预留
    if (available <= 0) return "...";

    std::string result;
    for (size_t i = 1; i <= text.size(); ++i) {
        std::string sub = text.substr(0, i);
        if (VisibleLength(sub) > static_cast<size_t>(available)) {
            result = text.substr(0, i - 1);
            break;
        }
        if (i == text.size()) {
            result = text;
            break;
        }
    }
    return result + "...";
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
    std::string line(cols, '-');

    // ---------- 帮助信息 ----------
    std::ostringstream oss;
    oss << separator << "\n";
    oss << CYAN << "W / S" << RESET << " - Move selection 1 up / down\n";
    oss << CYAN << "Up / Down" << RESET << " - Move selection 2 up / down\n";
    oss << CYAN << "J" << RESET << " - Cut selection 1\n";
    oss << CYAN << "K" << RESET << " - Cut selection 2\n";
    oss << CYAN << "L" << RESET << " - Link selection 1 And selection 2\n";
    oss << CYAN << "Q" << RESET << " - Edit literal of selection 1\n";
    oss << CYAN << "E" << RESET << " - Edit literal of selection 2\n";
    oss << separator << "\n";
    std::cout << oss.str();

    // 灰色（无连接引脚用）
    static const char* GRAY = "\x1b[90m";

    // ---------- 查找节点 ----------
    const Node* node1 = nullptr;
    const Node* node2 = nullptr;
    for (const auto& n : currentBPData.nodes) {
        if (n.id == selectedNodeId1) node1 = &n;
        if (n.id == selectedNodeId2) node2 = &n;
    }

    // 判断引脚是否有连接
    auto hasConnection = [&](int nodeId, const std::string& pinName, const std::string& io) -> bool {
        if (io == "I") {
            for (const auto& link : currentBPData.links) {
                if (link.targetNode == nodeId && link.targetPin == pinName) return true;
            }
        }
        else if (io == "O") {
            for (const auto& link : currentBPData.links) {
                if (link.sourceNode == nodeId && link.sourcePin == pinName) return true;
            }
        }
        return false;
        };

    // 格式化字面值（含优化后的截断）
    auto formatLiteral = [&](const std::string& type, const std::string& raw, int maxVisible) -> std::string {
        if (type == "frame") return "";
        if (raw.empty()) return "";

        std::string result = raw;

        // 基础类型格式化
        if (type == "float") {
            try {
                float num = std::stof(raw);
                char buf[32];
                snprintf(buf, sizeof(buf), "%.1ff", num);
                result = buf;
            }
            catch (...) { /* 转换失败，保留原值 */ }
        }
        else if (type == "int") {
            // int 直接保留
        }
        else if (type == "bool") {
            if (raw == "true" || raw == "1") result = "true";
            else if (raw == "false" || raw == "0") result = "false";
        }
        else if (type == "string") {
            // 1. 截断换行
            size_t newline = result.find_first_of("\r\n");
            if (newline != std::string::npos) {
                result = result.substr(0, newline);
            }
            // 2. 按可见长度截断
            result = TruncateText(result, maxVisible);
        }
        return result;
        };

    // 打印节点详情的通用 lambda，接受引脚选中索引
    auto printNode = [&](const Node* node, const char* label, int selectedPinIdx) {
        if (!node) {
            std::cout << WHITE << "Node " << label << " not found." << RESET << "\n";
            return;
        }

        std::cout << WHITE << "\n" << separator << RESET << "\n";
        std::cout << CYAN << "No. " << node->id << " " << node->type << RESET << "\n";
        std::cout << WHITE << separator << RESET << "\n\n";

        for (size_t i = 0; i < node->pins.size(); ++i) {
            const auto& pin = node->pins[i];
            bool isSelected = (static_cast<int>(i) == selectedPinIdx);
            std::string arrow = (pin.io == "I") ? "O---> " : "<---O ";
            bool connected = hasConnection(node->id, pin.name, pin.io);
            const char* arrowColor = connected ? WHITE : GRAY;
            const char* nameColor = isSelected ? YELLOW : WHITE;  // 选中时黄色

            std::cout << arrowColor << arrow << RESET
                << nameColor << " " << pin.name << RESET
                << CYAN << "  " << pin.type << RESET << "\n";

            // 字面值输出
            if (pin.literal.has_value() && pin.type != "frame") {
                int usedVis = (int)(arrow.size() + VisibleLength(pin.name) + pin.type.size() + 4);
                int avail = cols - usedVis;
                if (avail < 3) avail = 3;
                std::string lit = formatLiteral(pin.type, pin.literal.value(), avail);
                if (!lit.empty())
                    std::cout << CYAN << "\nliteral: " << RESET << lit;
            }
            std::cout << "\n" << line << "\n\n";
        }
        };

    // 传入各个节点对应的选中引脚索引
    printNode(node1, "1", selectedPinId1);
    std::cout << std::endl;
    printNode(node2, "2", selectedPinId2);
}

void NodeViewer::MoveToPrev(int target) {
    // 根据 target 获取对应的选中引脚引用和节点 ID
    int& selectedPin = (target == 1) ? selectedPinId1 : selectedPinId2;
    int nodeId = (target == 1) ? selectedNodeId1 : selectedNodeId2;

    // 查找节点
    const Node* node = nullptr;
    for (const auto& n : currentBPData.nodes) {
        if (n.id == nodeId) {
            node = &n;
            break;
        }
    }
    if (!node || node->pins.empty()) {
        selectedPin = -1;
    }
    else {
        int maxIdx = static_cast<int>(node->pins.size()) - 1;
        if (selectedPin < 0 || selectedPin > maxIdx) {
            // 非法状态，重置到最后一个
            selectedPin = maxIdx;
        }
        else {
            selectedPin = (selectedPin - 1 + (maxIdx + 1)) % (maxIdx + 1);
            // 使用取模保证循环
        }
    }

    ClearScreen();
    BuildAndPrintAll();
    ScrollToTheTop();
}

void NodeViewer::MoveToNext(int target) {
    int& selectedPin = (target == 1) ? selectedPinId1 : selectedPinId2;
    int nodeId = (target == 1) ? selectedNodeId1 : selectedNodeId2;

    const Node* node = nullptr;
    for (const auto& n : currentBPData.nodes) {
        if (n.id == nodeId) {
            node = &n;
            break;
        }
    }
    if (!node || node->pins.empty()) {
        selectedPin = -1;
    }
    else {
        int maxIdx = static_cast<int>(node->pins.size()) - 1;
        if (selectedPin < 0 || selectedPin > maxIdx) {
            // 非法状态，重置到第一个
            selectedPin = 0;
        }
        else {
            selectedPin = (selectedPin + 1) % (maxIdx + 1);
        }
    }

    ClearScreen();
    BuildAndPrintAll();
    ScrollToTheTop();
}
bool NodeViewer::Cut(int target) {
    ClearScreen();
    FlushInputBuffer();

    // 获取当前选中的节点和引脚索引
    int nodeId = (target == 1) ? selectedNodeId1 : selectedNodeId2;
    int pinIdx = (target == 1) ? selectedPinId1 : selectedPinId2;

    // 查找节点
    const Node* node = nullptr;
    for (const auto& n : currentBPData.nodes) {
        if (n.id == nodeId) {
            node = &n;
            break;
        }
    }
    if (!node || pinIdx < 0 || pinIdx >= static_cast<int>(node->pins.size())) {
        std::cout << "No valid pin selected.\nPress Enter to continue...\n";
        std::cin.get();
        return false;
    }

    const Pin& pin = node->pins[pinIdx];
    std::vector<const ::Link*> relatedLinks;   // 保存指向相关连接的指针

    // 根据引脚方向收集连接
    if (pin.io == "I") {
        for (const auto& link : currentBPData.links) {
            if (link.targetNode == nodeId && link.targetPin == pin.name)
                relatedLinks.push_back(&link);
        }
    }
    else { // "O"
        for (const auto& link : currentBPData.links) {
            if (link.sourceNode == nodeId && link.sourcePin == pin.name)
                relatedLinks.push_back(&link);
        }
    }

    if (relatedLinks.empty()) {
        std::cout << "Pin \"" << pin.name << "\" is not connected to any other pin.\n";
        std::cout << "Press Enter to continue...\n";
        std::cin.get();
        return false;
    }

    // 循环选择要切断的连接
    while (true) {
        ClearScreen();
        std::cout << "Pin \"" << pin.name << "\" ("
            << ((pin.io == "I") ? "input" : "output")
            << ") is connected to:\n\n";

        for (size_t i = 0; i < relatedLinks.size(); ++i) {
            const ::Link& link = *relatedLinks[i];
            // 确定另一个端点
            int otherNodeId = (pin.io == "I") ? link.sourceNode : link.targetNode;
            std::string otherPinName = (pin.io == "I") ? link.sourcePin : link.targetPin;
            std::string otherType = "";
            for (const auto& n : currentBPData.nodes) {
                if (n.id == otherNodeId) {
                    otherType = n.type;
                    break;
                }
            }
            std::cout << "  " << i + 1 << " - Node " << otherNodeId << " ("
                << otherType << ") pin \"" << otherPinName << "\"\n";
        }

        std::cout << "\nEnter number to cut, or N to cancel: > ";

        std::string input;
        if (!std::getline(std::cin, input)) {
            return false;
        }
        if (input == "n" || input == "N" || input == "#esc#") {
            return false;
        }

        try {
            int choice = std::stoi(input);
            if (choice >= 1 && choice <= static_cast<int>(relatedLinks.size())) {
                const ::Link* toDelete = relatedLinks[choice - 1];
                // 从 currentBPData.links 中删除匹配项
                auto& links = currentBPData.links;
                links.erase(
                    std::remove_if(links.begin(), links.end(),
                        [&](const ::Link& l) {
                            return l.sourceNode == toDelete->sourceNode &&
                                l.sourcePin == toDelete->sourcePin &&
                                l.targetNode == toDelete->targetNode &&
                                l.targetPin == toDelete->targetPin;
                        }),
                    links.end());

                // 写入文件
                std::string path = WideToUTF8(currentBPPath);
                WriteBPData(path, currentBPData);
                return true;
            }
            else {
                std::cout << "Invalid number. Press Enter to try again.\n";
                std::cin.get();
            }
        }
        catch (...) {
            std::cout << "Invalid input. Press Enter to try again.\n";
            std::cin.get();
        }
    }
}
bool NodeViewer::Link() {
    ClearScreen();
    FlushInputBuffer();

    // 获取两个选中节点及其引脚索引
    int nodeId1 = selectedNodeId1;
    int nodeId2 = selectedNodeId2;
    int pinIdx1 = selectedPinId1;
    int pinIdx2 = selectedPinId2;

    // 查找节点
    const Node* node1 = nullptr;
    const Node* node2 = nullptr;
    for (const auto& n : currentBPData.nodes) {
        if (n.id == nodeId1) node1 = &n;
        if (n.id == nodeId2) node2 = &n;
    }
    if (!node1 || !node2 ||
        pinIdx1 < 0 || pinIdx1 >= (int)node1->pins.size() ||
        pinIdx2 < 0 || pinIdx2 >= (int)node2->pins.size()) {
        std::cout << "Invalid pin selection.\nPress Enter to continue...\n";
        std::cin.get();
        return false;
    }

    const Pin& pin1 = node1->pins[pinIdx1];
    const Pin& pin2 = node2->pins[pinIdx2];

    // 验证方向：必须一个是 I(输入) 一个是 O(输出)
    if (pin1.io == pin2.io) {
        std::cout << "Cannot connect: both pins are "
            << ((pin1.io == "I") ? "inputs" : "outputs") << ".\n";
        std::cout << "Press Enter to continue...\n";
        std::cin.get();
        return false;
    }

    // 确定源引脚（输出）和目标引脚（输入）
    const Pin* srcPin = (pin1.io == "O") ? &pin1 : &pin2;
    const Pin* dstPin = (pin1.io == "O") ? &pin2 : &pin1;
    int srcNodeId = (pin1.io == "O") ? nodeId1 : nodeId2;
    int dstNodeId = (pin1.io == "O") ? nodeId2 : nodeId1;

    // 类型兼容性检查
    bool srcIsExecOrFrame = (srcPin->type == "exec" || srcPin->type == "frame");
    bool dstIsExecOrFrame = (dstPin->type == "exec" || dstPin->type == "frame");

    if (srcIsExecOrFrame || dstIsExecOrFrame) {
        // exec 或 frame 只能连接同类型
        if (srcPin->type != dstPin->type) {
            std::cout << "Cannot connect: pin types are incompatible ("
                << srcPin->type << " and " << dstPin->type << ").\n"
                << "exec and frame pins can only connect to the same type.\n";
            std::cout << "Press Enter to continue...\n";
            std::cin.get();
            return false;
        }
    }
    // 否则数据类型（int, float, string, bool）可以互相连接，无需额外检查

    // 检查是否已存在完全相同的连接
    for (const auto& l : currentBPData.links) {
        if (l.sourceNode == srcNodeId && l.sourcePin == srcPin->name &&
            l.targetNode == dstNodeId && l.targetPin == dstPin->name) {
            std::cout << "This connection already exists.\nPress Enter to continue...\n";
            std::cin.get();
            return false;
        }
    }

    // 创建新连接
    ::Link newLink;
    newLink.sourceNode = srcNodeId;
    newLink.sourcePin = srcPin->name;
    newLink.targetNode = dstNodeId;
    newLink.targetPin = dstPin->name;
    currentBPData.links.push_back(newLink);

    // 保存到文件
    std::string path = WideToUTF8(currentBPPath);
    if (!WriteBPData(path, currentBPData)) {
        std::cout << "Failed to write blueprint file.\nPress Enter to continue...\n";
        std::cin.get();
        return false;
    }

    return true;   // 成功，调用者负责 SetEvent 和界面刷新
}
bool NodeViewer::Edit(int target) {
    ClearScreen();
    FlushInputBuffer();

    // 获取目标节点和引脚索引
    int nodeId = (target == 1) ? selectedNodeId1 : selectedNodeId2;
    int pinIdx = (target == 1) ? selectedPinId1 : selectedPinId2;

    // 查找节点
    const Node* nodeConst = nullptr;
    for (const auto& n : currentBPData.nodes) {
        if (n.id == nodeId) { nodeConst = &n; break; }
    }
    if (!nodeConst || pinIdx < 0 || pinIdx >= static_cast<int>(nodeConst->pins.size())) {
        std::cout << "Invalid pin selected.\nPress Enter to continue...\n";
        std::cin.get();
        return false;
    }

    // 获取可修改的引脚引用
    Node* node = nullptr;
    for (auto& n : currentBPData.nodes) {
        if (n.id == nodeId) { node = &n; break; }
    }
    if (!node) return false;   // 理论上不会发生
    Pin& pin = node->pins[pinIdx];

    // 1. 输出引脚不可编辑
    if (pin.io == "O") {
        std::cout << "Cannot edit literal of an output pin.\nPress Enter to continue...\n";
        std::cin.get();
        return false;
    }

    // 2. exec 和 frame 类型不可编辑
    if (pin.type == "exec" || pin.type == "frame") {
        std::cout << "Pins of type \"" << pin.type << "\" cannot have a literal.\nPress Enter to continue...\n";
        std::cin.get();
        return false;
    }

    // 输入循环
    while (true) {
        ClearScreen();
        std::cout << "Editing literal of pin \"" << pin.name << "\" (" << pin.type << ")\n";
        if (pin.literal.has_value())
            std::cout << "Current value: \"" << pin.literal.value() << "\"\n";
        else
            std::cout << "Current value: (empty)\n";
        std::cout << "\nEnter new value (or \"#esc#\" to cancel):\n> ";

        std::string input;
        if (!std::getline(std::cin, input)) {
            return false;   // 输入流异常
        }
        if (input == "#esc#") {
            return false;   // 用户取消
        }

        // 字面值有效性检查
        bool valid = false;
        if (pin.type == "int") {
            try {
                std::stoi(input);
                valid = true;
            }
            catch (...) {
                valid = false;
            }
        }
        else if (pin.type == "float") {
            // 格式要求：至少一位整数 + "." + 至少一位小数 + "f"
            if (input.empty() || input.back() != 'f') {
                valid = false;
            }
            else {
                std::string s = input.substr(0, input.size() - 1); // 去掉末尾 'f'
                size_t dotPos = s.find('.');
                if (dotPos == std::string::npos || dotPos == 0 || dotPos == s.size() - 1) {
                    valid = false;
                }
                else {
                    size_t start = (s[0] == '+' || s[0] == '-') ? 1 : 0;
                    if (dotPos <= start) {
                        valid = false;
                    }
                    else {
                        try {
                            std::stof(s);
                            valid = true;
                        }
                        catch (...) {
                            valid = false;
                        }
                    }
                }
            }
        }
        else if (pin.type == "string") {
            valid = true;   // 字符串允许任意内容（包括空）
        }
        else if (pin.type == "bool") {
            std::string lower = input;
            for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            valid = (lower == "true" || lower == "false");
        }
        else {
            valid = false;  // 未知类型（理论上走不到这里）
        }

        if (!valid) {
            std::cout << "\nInvalid literal for type \"" << pin.type << "\". Press Enter to try again...\n";
            std::cin.get();
            continue;
        }

        // 应用新值
        pin.literal = input;   // 直接存储字符串

        // 保存到文件
        std::string path = WideToUTF8(currentBPPath);
        if (!WriteBPData(path, currentBPData)) {
            std::cout << "Failed to write blueprint file.\nPress Enter to continue...\n";
            std::cin.get();
            return false;
        }
        return true;   // 修改成功，外部会 SetEvent 并清屏
    }
}

void NodeViewer::Run()
{
    // 事件等待数组
    HANDLE eventsToWait[2] = { hLoadBPEvent, hNodeMoveEvent };
    DWORD numEvents = 2;

    // 用于验证节点ID是否合法的lambda
    auto isValidNodeId = [&](int id) -> bool {
        for (const auto& n : currentBPData.nodes) {
            if (n.id == id) return true;
        }
        return false;
        };

    // 根据节点ID设置引脚索引（选中第一个引脚，若无引脚则-1）
    auto setDefaultPinIndex = [&](int nodeId) -> int {
        if (nodeId < 0) return -1;
        for (const auto& n : currentBPData.nodes) {
            if (n.id == nodeId) {
                return n.pins.empty() ? -1 : 0;
            }
        }
        return -1;
        };

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

                    // 从共享内存读取节点ID并校验
                    if (pNodeIdSharedMem) {
                        int id1 = pNodeIdSharedMem[0];
                        int id2 = pNodeIdSharedMem[1];

                        if (isValidNodeId(id1)) selectedNodeId1 = id1;
                        else if (!currentBPData.nodes.empty()) selectedNodeId1 = currentBPData.nodes[0].id;
                        else selectedNodeId1 = -1;

                        if (isValidNodeId(id2)) selectedNodeId2 = id2;
                        else if (!currentBPData.nodes.empty()) selectedNodeId2 = currentBPData.nodes[0].id;
                        else selectedNodeId2 = -1;
                    }
                    else {
                        // 没有共享内存时使用默认第一个节点
                        if (!currentBPData.nodes.empty()) {
                            selectedNodeId1 = currentBPData.nodes[0].id;
                            selectedNodeId2 = currentBPData.nodes[0].id;
                        }
                        else {
                            selectedNodeId1 = selectedNodeId2 = -1;
                        }
                    }

                    // 设置默认引脚选中（第一个引脚）
                    selectedPinId1 = setDefaultPinIndex(selectedNodeId1);
                    selectedPinId2 = setDefaultPinIndex(selectedNodeId2);

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

                // 校验有效性
                if (!isValidNodeId(selectedNodeId1) && !currentBPData.nodes.empty())
                    selectedNodeId1 = currentBPData.nodes[0].id;
                if (!isValidNodeId(selectedNodeId2) && !currentBPData.nodes.empty())
                    selectedNodeId2 = currentBPData.nodes[0].id;

                // 重置引脚选中为第一个引脚
                selectedPinId1 = setDefaultPinIndex(selectedNodeId1);
                selectedPinId2 = setDefaultPinIndex(selectedNodeId2);

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
                    isEditing = true;
                    if (Cut(1)) {
                        SetEvent(hNodeChangedEvent);
                        ClearScreen();
                    }
                    else {
                        ClearScreen();
                        BuildAndPrintAll();
                    }
                    isEditing = false;
                    break;
                case KeyCode::K:
                    isEditing = true;
                    if (Cut(2)) {
                        SetEvent(hNodeChangedEvent);
                        ClearScreen();
                    }
                    else {
                        ClearScreen();
                        BuildAndPrintAll();
                    }
                    isEditing = false;
                    break;
                case KeyCode::L:
                    isEditing = true;
                    if (Link()) {
                        SetEvent(hNodeChangedEvent);
                        ClearScreen();
                    }
                    else {
                        ClearScreen();
                        BuildAndPrintAll();
                    }
                    isEditing = false;
                    break;
                case KeyCode::Q:
                    isEditing = true;
                    if (Edit(1)) {
                        SetEvent(hNodeChangedEvent);
                    }
                    else {
                        ClearScreen();
                        BuildAndPrintAll();
                    }
                    isEditing = false;
                    break;
                case KeyCode::E:
                    isEditing = true;
                    if (Edit(2)) {
                        SetEvent(hNodeChangedEvent);
                        ClearScreen();
                    }
                    else {
                        ClearScreen();
                        BuildAndPrintAll();
                    }
                    isEditing = false;
                    break;
                default:
                    break;
                }
            }
        }
    }
}