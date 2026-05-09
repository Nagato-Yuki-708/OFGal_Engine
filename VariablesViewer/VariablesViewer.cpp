// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#include "VariablesViewer.h"

VariablesViewer::VariablesViewer() {
    SetConsoleTitleW(L"OFGal_Engine/VariablesViewer");
    ConfigureConsole();
    SetWindowSizeAndPosition();

    // ---------- 打开 LoadBP 事件 ----------
    hLoadBPEvent = OpenEventW(
        EVENT_MODIFY_STATE | SYNCHRONIZE,
        FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_VariablesViewer_LoadBP");
    if (!hLoadBPEvent) {
        DEBUG_W(L"[VariablesViewer] OpenEvent LoadBP Failed, error=" << GetLastError() << L"\n");
    }

    // ---------- 打开 VarChanged 事件 ----------
    hVarChangedEvent = OpenEventW(
        EVENT_MODIFY_STATE | SYNCHRONIZE,
        FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_VariablesViewer_VarChanged");
    if (!hVarChangedEvent) {
        DEBUG_W(L"[VariablesViewer] OpenEvent VarChanged Failed, error=" << GetLastError() << L"\n");
    }

    // ---------- 打开共享内存（BlueprintPath） ----------
    hFileMapping = OpenFileMappingW(
        FILE_MAP_READ,
        FALSE,
        L"Global\\OFGal_Engine_BlueprintViewer_BlueprintPath");
    if (hFileMapping) {
        pSharedMem = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
        if (!pSharedMem) {
            DEBUG_W(L"[VariablesViewer] MapViewOfFile Failed" << L"\n");
        }
    }
    else {
        pSharedMem = nullptr;
        DEBUG_W(L"[VariablesViewer] OpenFileMapping Failed, error=" << GetLastError() << L"\n");
    }

    // ---------- 按键绑定 ----------
    m_inputSystem.SetGlobalCapture(false);
    m_inputSystem.SetWindowHandle(GetConsoleWindow());

    m_inputCollector.AddBinding({ 'W',        Modifier::None, KeyCode::W,      true });
    m_inputCollector.AddBinding({ 'S',        Modifier::None, KeyCode::S,      true });
    m_inputCollector.AddBinding({ VK_UP,      Modifier::None, KeyCode::Up,     true });
    m_inputCollector.AddBinding({ VK_DOWN,    Modifier::None, KeyCode::Down,   true });
    m_inputCollector.AddBinding({ 'E',        Modifier::None, KeyCode::E,      true });
    m_inputCollector.AddBinding({ 'F',        Modifier::None, KeyCode::F,      true });
    m_inputCollector.AddBinding({ VK_DELETE,  Modifier::None, KeyCode::Delete, true });
}
VariablesViewer::~VariablesViewer() {
    if (pSharedMem)       UnmapViewOfFile(pSharedMem);
    if (hFileMapping)     CloseHandle(hFileMapping);
    if (hLoadBPEvent)     CloseHandle(hLoadBPEvent);
    if (hVarChangedEvent) CloseHandle(hVarChangedEvent);
}

void VariablesViewer::ConfigureConsole() {
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

void VariablesViewer::ClearScreen() {
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

void VariablesViewer::SetWindowSizeAndPosition() {
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
            static_cast<int>(1010.0f * scaleY),
            static_cast<int>(2040.0f * scaleX),
            static_cast<int>(470.0f * scaleY),
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void VariablesViewer::FlushInputBuffer() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(hIn);
}

std::string VariablesViewer::TruncateText(const std::string& text, int maxWidth) const {
    if (maxWidth <= 0) return "";
    size_t vis = VisibleLength(text);
    if (vis <= static_cast<size_t>(maxWidth)) return text;

    // 需要预留省略号 "..."
    int available = maxWidth - 3;
    if (available <= 0) return "...";

    // 逐字节尝试，找到合适的前缀长度（避免截断多字节字符，但此处假设单字节）
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

void VariablesViewer::BuildAndPrintAll() {
    int cols = GetConsoleColumns();
    std::string separator(cols, '=');

    // ---------- 帮助信息 ----------
    std::ostringstream oss;
    oss << separator << "\n";
    oss << CYAN << "W / S" << RESET << " - Move selection up / down\n";
    oss << CYAN << "Delete" << RESET << " - Remove selected variable\n";
    oss << CYAN << "F" << RESET << " - Add a new variable\n";
    oss << CYAN << "E" << RESET << " - Edit selected variable\n";
    oss << separator << "\n";
    std::cout << oss.str();

    // ---------- 变量列表 ----------
    auto& vars = currentBPData.variables;

    // 修正选中索引
    if (!vars.empty()) {
        if (selectedVarIndex < 0 || selectedVarIndex >= static_cast<int>(vars.size())) {
            selectedVarIndex = 0;
        }
    } else {
        selectedVarIndex = -1;
    }

    const int nameWidth = 20;
    const int typeWidth = 15;
    const std::string whiteLine = WHITE + std::string(cols, '-') + RESET + "\n";

    // ---------- 显示当前选中变量 ----------
    std::cout << "Now selected ";
    if (!vars.empty() && selectedVarIndex >= 0 && selectedVarIndex < static_cast<int>(vars.size())) {
        std::string selectedName = vars[selectedVarIndex].name;
        selectedName = TruncateText(selectedName, nameWidth);
        std::cout << ORANGE << selectedName << RESET;
    } else {
        std::cout << "(none)";
    }
    std::cout << "\n" << whiteLine;

    // ---------- 变量条目 ----------
    if (vars.empty()) {
        std::cout << "No variables defined.\n";
    } else {
        int contentMax = cols - (nameWidth + 3 + typeWidth + 3);
        if (contentMax < 1) contentMax = 1;

        for (size_t i = 0; i < vars.size(); ++i) {
            const auto& var = vars[i];
            bool selected = (static_cast<int>(i) == selectedVarIndex);

            // 名字：截断 + 颜色 + 填充
            std::string dispName = TruncateText(var.name, nameWidth);
            std::string nameField = (selected ? ORANGE : YELLOW) + dispName + RESET;
            int nameVis = static_cast<int>(VisibleLength(nameField));
            if (nameVis < nameWidth) nameField += std::string(nameWidth - nameVis, ' ');

            // 类型：颜色 + 填充
            std::string typeField = CYAN + var.type + RESET;
            int typeVis = static_cast<int>(VisibleLength(typeField));
            if (typeVis < typeWidth) typeField += std::string(typeWidth - typeVis, ' ');

            // 内容：按类型格式化
            std::string content;
            if (!var.value.empty()) {
                const std::string& v = var.value;
                if (var.type == "int") {
                    content = v;
                } else if (var.type == "float") {
                    try {
                        float num = std::stof(v);
                        std::ostringstream fss;
                        fss << std::fixed << std::setprecision(1) << num << "f";
                        content = fss.str();
                    } catch (...) {
                        content = v;
                    }
                } else if (var.type == "string") {
                    std::string temp = v;
                    size_t newlinePos = temp.find_first_of("\r\n");
                    if (newlinePos != std::string::npos) {
                        temp = temp.substr(0, newlinePos) + "...";
                    } else {
                        int visLen = static_cast<int>(VisibleLength(temp));
                        if (visLen + 3 > contentMax) {
                            int keep = contentMax - 3;
                            if (keep < 0) keep = 0;
                            temp = temp.substr(0, keep) + "...";
                        }
                    }
                    content = temp;
                } else if (var.type == "bool") {
                    if (v == "true" || v == "1") content = "true";
                    else if (v == "false" || v == "0") content = "false";
                    else content = v;
                } else if (var.type == "frame") {
                    content = "";   // 不显示
                } else {
                    content = v;    // 未知类型原样显示
                }
            }

            // 输出行
            std::cout << nameField
                      << WHITE << " | " << RESET
                      << typeField
                      << WHITE << " | " << RESET
                      << content << "\n";
        }
    }
    std::cout << whiteLine;
}

void VariablesViewer::ScrollToTheTop() {
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

std::string VariablesViewer::WideToUTF8(const std::wstring& wstr) const {
    if (wstr.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

int VariablesViewer::GetConsoleColumns() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return 80;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
}

size_t VariablesViewer::VisibleLength(const std::string& s) {
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

bool VariablesViewer::SetConsoleQuickEdit(bool enable) {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE) return false;
    DWORD mode;
    if (!GetConsoleMode(hIn, &mode)) return false;
    if (enable)
        mode |= ENABLE_QUICK_EDIT_MODE;
    else
        mode &= ~ENABLE_QUICK_EDIT_MODE;
    mode |= ENABLE_EXTENDED_FLAGS;
    return SetConsoleMode(hIn, mode);
}

// 变量命名规则：首字符为字母或下划线，其余为字母、数字、下划线
bool VariablesViewer::IsValidVariableName(const std::string& name) const {
    if (name.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_')
        return false;
    for (size_t i = 1; i < name.size(); ++i) {
        char c = name[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            return false;
    }
    return true;
}

// 字面值有效性检查（仅用于编辑）
bool VariablesViewer::IsLiteralValid(const std::string& type, const std::string& literal) const {
    if (type == "int") {
        if (literal.empty()) return false;
        try {
            std::stoi(literal);
            return true;
        }
        catch (...) {
            return false;
        }
    }
    else if (type == "float") {
        if (literal.empty()) return false;
        // 必须包含小数点、至少一位小数、以f结尾： [+-]?[0-9]+\.[0-9]+f
        // 这里使用简单解析
        std::string s = literal;
        // 去掉末尾f
        if (s.back() != 'f') return false;
        s.pop_back();
        auto dotPos = s.find('.');
        if (dotPos == std::string::npos || dotPos == 0 || dotPos == s.size() - 1)
            return false;
        // 允许前导符号
        size_t start = (s[0] == '+' || s[0] == '-') ? 1 : 0;
        if (dotPos <= start) return false; // 小数点前无数字
        try {
            std::stof(s);
            return true;
        }
        catch (...) {
            return false;
        }
    }
    else if (type == "string") {
        // 字符串允许任何内容（包括空）
        return true;
    }
    else if (type == "bool") {
        std::string lower = literal;
        for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return (lower == "true" || lower == "false");
    }
    else if (type == "frame") {
        // frame 的字面值必须为空
        return literal.empty();
    }
    return false; // 未知类型
}

void VariablesViewer::Run() {
    HANDLE eventsToWait[1] = { hLoadBPEvent };
    DWORD numEvents = 1;
    if (!hLoadBPEvent) numEvents = 0;   // 极端情况，但至少保证 numEvents 为 0

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
                    ClearScreen();
                    BuildAndPrintAll();      // 刷新变量列表
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
                case KeyCode::Up:
                    MoveToPrev();
                    break;
                case KeyCode::S:
                case KeyCode::Down:
                    MoveToNext();
                    break;
                case KeyCode::F:
                    isEditing = true;
                    if (AddVar()) {
                        SetEvent(hVarChangedEvent);
                        ClearScreen();
                    }
                    else {
                        ClearScreen();
                        BuildAndPrintAll();
                    }
                    break;
                    isEditing = false;
                case KeyCode::E:
                    isEditing = true;
                    if (EditVar()) {
                        SetEvent(hVarChangedEvent);
                        ClearScreen();
                    }
                    else {
                        ClearScreen();
                        BuildAndPrintAll();
                    }
                    isEditing = false;
                    break;
                case KeyCode::Delete:
                    isEditing = true;
                    if (DeleteVar()) {
                        SetEvent(hVarChangedEvent);
                        ClearScreen();
                    }
                    else {
                        ClearScreen();
                        BuildAndPrintAll();
                    }
                    isEditing = false;
                    break;
                default: break;
                }
            }
        }
    }
}

void VariablesViewer::MoveToPrev() {
    if (currentBPData.variables.empty()) {
        selectedVarIndex = -1;
        return;
    }
    if (selectedVarIndex <= 0) {
        // 从第一个向上则循环到最后一个
        selectedVarIndex = static_cast<int>(currentBPData.variables.size()) - 1;
    }
    else {
        --selectedVarIndex;
    }
    // 移动后立即刷新界面
    ClearScreen();
    BuildAndPrintAll();
    ScrollToTheTop();
}

void VariablesViewer::MoveToNext() {
    if (currentBPData.variables.empty()) {
        selectedVarIndex = -1;
        return;
    }
    if (selectedVarIndex < 0) {
        // 无选中时默认跳转到第一个
        selectedVarIndex = 0;
    }
    else {
        selectedVarIndex = (selectedVarIndex + 1) % static_cast<int>(currentBPData.variables.size());
    }
    ClearScreen();
    BuildAndPrintAll();
    ScrollToTheTop();
}
bool VariablesViewer::AddVar() {
    ClearScreen();
    FlushInputBuffer();

    // ---- 类型选择 ----
    const std::vector<std::string> types = { "int", "float", "string", "bool", "frame" };
    int chosenType = -1;
    while (true) {
        ClearScreen();
        std::cout << "Select variable type:\n\n";
        for (size_t i = 0; i < types.size(); ++i) {
            std::cout << "  " << i + 1 << " - " << types[i] << "\n";
        }
        std::cout << "\nEnter number or \"#esc#\" to cancel: > ";
        std::string line;
        if (!std::getline(std::cin, line))
            return false;
        if (line == "#esc#") return false;
        try {
            int idx = std::stoi(line) - 1;
            if (idx >= 0 && idx < static_cast<int>(types.size())) {
                chosenType = idx;
                break;
            }
        }
        catch (...) {}
        std::cout << "\nInvalid choice. Press Enter to retry...\n";
        std::cin.get();
    }

    std::string varType = types[chosenType];

    // ---- 变量名输入 ----
    std::string varName;
    while (true) {
        ClearScreen();
        std::cout << "Enter variable name (letters, digits, underscores;\n"
            "must start with a letter or underscore).\n"
            "Enter \"#esc#\" to cancel.\n\n"
            "Name: > ";
        std::string line;
        if (!std::getline(std::cin, line))
            return false;
        if (line == "#esc#") return false;

        if (!IsValidVariableName(line)) {
            std::cout << "\nInvalid variable name format.\n"
                "Press Enter to try again...\n";
            std::cin.get();
            continue;
        }

        // 检查重名
        bool duplicate = false;
        for (const auto& v : currentBPData.variables) {
            if (v.name == line) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            std::cout << "\nA variable with this name already exists.\n"
                "Press Enter to try again...\n";
            std::cin.get();
            continue;
        }

        varName = line;
        break;
    }

    // ---- 创建变量（value 留空）----
    Variable newVar;
    newVar.name = varName;
    newVar.type = varType;
    newVar.value = "";  // 默认为空

    // ---- 保存并更新索引 ----
    currentBPData.variables.push_back(newVar);
    selectedVarIndex = static_cast<int>(currentBPData.variables.size()) - 1;

    std::string path = WideToUTF8(currentBPPath);
    WriteBPData(path, currentBPData);

    return true;
}
bool VariablesViewer::EditVar() {
    ClearScreen();
    FlushInputBuffer();

    // ---- 有效性检查 ----
    if (currentBPData.variables.empty() ||
        selectedVarIndex < 0 ||
        selectedVarIndex >= static_cast<int>(currentBPData.variables.size()))
    {
        std::cout << "No variable selected or list is empty.\n";
        std::cout << "Press Enter to continue...\n";
        std::cin.get();
        return false;
    }

    const Variable& target = currentBPData.variables[selectedVarIndex];

    // 若类型为 frame，自动清空并结束
    if (target.type == "frame") {
        currentBPData.variables[selectedVarIndex].value = "";
        std::string path = WideToUTF8(currentBPPath);
        WriteBPData(path, currentBPData);
        return true;
    }

    // ---- 输入新字面值 ----
    std::string newValue;
    while (true) {
        ClearScreen();
        std::cout << "Edit variable value\n\n";
        std::cout << "  Name : " << target.name << "\n";
        std::cout << "  Type : " << target.type << "\n";
        std::cout << "  Current value: \"" << target.value << "\"\n\n";
        std::cout << "Enter new value (or \"#esc#\" to cancel):\n> \n";

        std::string line;
        SetConsoleQuickEdit(true);
        if (!std::getline(std::cin, line))
        {
            SetConsoleQuickEdit(false);
            return false;
        }
        SetConsoleQuickEdit(false);
        if (line == "#esc#")
            return false;

        if (!IsLiteralValid(target.type, line)) {
            std::cout << "\nInvalid literal for type '" << target.type << "'.\n"
                "Press Enter to try again...\n";
            std::cin.get();
            continue;
        }

        newValue = line;
        // bool 统一小写存储
        if (target.type == "bool") {
            for (auto& c : newValue) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        break;
    }

    // ---- 更新并保存 ----
    currentBPData.variables[selectedVarIndex].value = newValue;
    std::string path = WideToUTF8(currentBPPath);
    WriteBPData(path, currentBPData);

    return true;
}
bool VariablesViewer::DeleteVar() {
    ClearScreen();
    FlushInputBuffer();

    // ---------- 1. 有效性检查 ----------
    auto& vars = currentBPData.variables;
    if (vars.empty() || selectedVarIndex < 0 || selectedVarIndex >= static_cast<int>(vars.size())) {
        std::cout << "No variable selected or list is empty.\n";
        std::cout << "Press Enter to continue...\n";
        std::cin.get();
        return false;
    }

    const Variable& target = vars[selectedVarIndex];

    // ---------- 2. 确认循环 ----------
    while (true) {
        ClearScreen();
        std::cout << "Are you sure you want to delete the following variable?\n\n";
        std::cout << "  Name : " << target.name << "\n";
        std::cout << "  Type : " << target.type << "\n";
        std::cout << "  Value: " << target.value << "\n\n";
        std::cout << "Confirm? (Y/N) Enter \"#esc#\" to cancel.\n> ";

        std::string confirm;
        if (!std::getline(std::cin, confirm)) {
            return false;  // 输入流异常
        }

        if (confirm == "#esc#" || confirm == "n" || confirm == "N") {
            return false;
        }

        if (confirm == "y" || confirm == "Y") {
            // ---------- 3. 执行删除 ----------
            BlueprintData temp = currentBPData;
            temp.variables.erase(temp.variables.begin() + selectedVarIndex);

            // 写回文件
            std::string pathStr = WideToUTF8(currentBPPath);
            WriteBPData(pathStr, temp);

            // ---------- 4. 修正选中索引 ----------
            if (temp.variables.empty()) {
                selectedVarIndex = -1;
            }
            else if (selectedVarIndex >= static_cast<int>(temp.variables.size())) {
                selectedVarIndex = static_cast<int>(temp.variables.size()) - 1;
            }

            return true;   // 成功删除，外部会 SetEvent + ClearScreen
        }

        // 无效输入，提示并重试
        std::cout << "\nInvalid input. Type Y or N, or #esc# to cancel.\n";
        std::cout << "Press Enter to continue...\n";
        std::cin.get();
    }
}