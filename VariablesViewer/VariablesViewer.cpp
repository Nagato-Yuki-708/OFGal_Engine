// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#include "VariablesViewer.h"

VariablesViewer::VariablesViewer() {
    SetConsoleTitleW(L"OFGal_Engine/VariablesViewer");
    ConfigureConsole();
    SetWindowSizeAndPosition();

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

void VariablesViewer::BuildAndPrintAll() {
    int cols = GetConsoleColumns();
    std::string separator(cols, '=');
    std::ostringstream oss;

    oss << CYAN << "=== Help ===" << RESET << "\n";
    oss << separator << "\n";

    oss << CYAN << "W / S" << RESET << " - Move selection up / down\n";
    oss << CYAN << "Delete" << RESET << " - Remove selected variable\n";
    oss << CYAN << "F" << RESET << " - Add a new variable\n";
    oss << CYAN << "E" << RESET << " - Edit selected variable\n";

    oss << separator << "\n";
    std::cout << oss.str();

    // TODO:...
    /*
    ------------------------------------------------------
    {Name in CYAN} | {Type in YELLOW} | {content in RESET}
    ------------------------------------------------------
    */
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

void VariablesViewer::Run() {

}