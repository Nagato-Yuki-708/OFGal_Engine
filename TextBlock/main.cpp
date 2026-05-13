// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#define NOMINMAX
#include <iostream>
#include <windows.h>
#include <string>
#include <algorithm>

void ConfigureConsole() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            dwMode |= ENABLE_WRAP_AT_EOL_OUTPUT;  // 强制自动换行，禁止水平滚动条
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

void ClearScreen() {
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

void SetWindowSizeAndPosition(float X, float Y, float cx, float cy) {
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
            static_cast<int>(X * scaleX),
            static_cast<int>(Y * scaleY),
            static_cast<int>(cx * scaleX),
            static_cast<int>(cy * scaleY),
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void ScrollToTheTop() {
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

int GetConsoleColumns() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return 80;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
        return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return 80;
}

// 计算宽字符串在指定列数下自动换行后所需的总行数
int CalculateTextLines(const std::wstring& text, int columns) {
    if (text.empty())
        return 1;

    int lines = 1;
    int col = 0;

    for (size_t i = 0; i < text.length(); ) {
        wchar_t ch = text[i];
        if (ch == L'\n') {
            lines++;
            col = 0;
            i++;
            continue;
        }

        // 判断当前字符是否为全角（在控制台中占2列）
        WORD charType;
        GetStringTypeW(CT_CTYPE3, &ch, 1, &charType);
        int width = (charType & C3_FULLWIDTH) ? 2 : 1;

        if (col + width > columns) {
            lines++;
            col = 0;
        }
        col += width;
        i++;
    }
    return lines;
}

// 根据文本调整缓冲区尺寸，确保无水平滚动条，高度至少为窗口可见行数
void AdjustBufferSize(const std::wstring& text) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;

    int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    int requiredLines = CalculateTextLines(text, cols);
    int newHeight = std::max(requiredLines, rows);

    // 设置缓冲区大小（宽度等于窗口列数，高度取计算值）
    COORD newSize;
    newSize.X = cols;
    newSize.Y = newHeight;
    SetConsoleScreenBufferSize(hOut, newSize);
    // 注意：因为 newHeight >= rows，所以不会因窗口区域超出而失败
}

int wmain(int argc, wchar_t* argv[]) {
    // 参数个数：程序名 + name + X + Y + cx + cy = 6
    if (argc < 6) {
        return -1;
    }

    std::wstring name = argv[1];
    SetConsoleTitleW(name.c_str());
    ConfigureConsole();

    // 将命令行传入的四个整数转换为 int 并保存
    int X = _wtoi(argv[2]);
    int Y = _wtoi(argv[3]);
    int cx = _wtoi(argv[4]);
    int cy = _wtoi(argv[5]);
    SetWindowSizeAndPosition((float)X, (float)Y, (float)cx, (float)cy);

    // 构造事件和共享内存的名称
    std::wstring eventName = L"Global\\OFGal_Engine_TextBlock_" + name + L"_PrintText";
    std::wstring sharedMemName = L"Global\\OFGal_Engine_TextBlock_" + name + L"_SharedMem";

    // 打开事件
    HANDLE hEvent = OpenEventW(SYNCHRONIZE, FALSE, eventName.c_str());
    if (hEvent == NULL) {
        return -1;
    }

    // 打开共享内存（文件映射）
    HANDLE hMapFile = OpenFileMappingW(FILE_MAP_READ, FALSE, sharedMemName.c_str());
    if (hMapFile == NULL) {
        CloseHandle(hEvent);
        return -1;
    }

    // 映射视图，大小为 300 个 wchar_t（UTF-16 编码）
    LPCWSTR pSharedMem = (LPCWSTR)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 300 * sizeof(WCHAR));
    if (pSharedMem == NULL) {
        CloseHandle(hMapFile);
        CloseHandle(hEvent);
        return -1;
    }

    // 循环监听事件
    while (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) {
        std::wstring text(pSharedMem);         // 复制一份以便安全使用
        AdjustBufferSize(text);                // 根据文本和窗口大小调整缓冲区
        system("cls");                         // 清屏（在新尺寸缓冲区上执行）
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),
            text.c_str(),
            (DWORD)text.length(),
            NULL, NULL);
        ScrollToTheTop();
    }

    // 清理
    UnmapViewOfFile(pSharedMem);
    CloseHandle(hMapFile);
    CloseHandle(hEvent);

    return 0;
}