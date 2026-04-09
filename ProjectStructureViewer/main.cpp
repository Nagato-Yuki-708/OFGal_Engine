// Copyright 2026 MrSeagull. All Rights Reserved.
#include <iostream>
#include <windows.h>
#include "ProjectStructureGetter.h"
#include "SharedTypes.h"

// 设置控制台窗口位置，大小
void SetupConsoleWindow() {
    HWND hwndConsole = GetConsoleWindow();
    if (hwndConsole) {
        // 设置窗口位置为左上角，大小为 400x400 像素
        SetWindowPos(hwndConsole, nullptr, 0, 0, 400, 400, SWP_NOZORDER | SWP_NOACTIVATE);

        // 获取控制台输出句柄
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            CONSOLE_FONT_INFO fontInfo;
            if (GetCurrentConsoleFont(hConsole, FALSE, &fontInfo)) {
                // 根据字体大小计算缓冲区字符行列数
                COORD bufferSize;
                bufferSize.X = static_cast<SHORT>(400 / fontInfo.dwFontSize.X);
                bufferSize.Y = static_cast<SHORT>(400 / fontInfo.dwFontSize.Y);
                SetConsoleScreenBufferSize(hConsole, bufferSize);
            }
            else {
                // 获取字体失败时，使用默认大小
                COORD defaultSize = { 80, 25 };
                SetConsoleScreenBufferSize(hConsole, defaultSize);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    ProjectStructure* pProjectStructure = new ProjectStructure;

    SetupConsoleWindow();

    const char* projectRoot = nullptr;
    if (argc > 1) {
        projectRoot = argv[1];
        std::cout << "项目根目录: " << projectRoot << std::endl;
    }

    // 打开事件...
    HANDLE hExitEvent = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE,
        "Global\\OFGal_Engine_ProjectStructureViewer_Exit");
    HANDLE hUpdateEvent = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE,
        "Global\\OFGal_Engine_ProjectStructureViewer_Refresh");
    if (!hExitEvent || !hUpdateEvent) {
        std::cerr << "打开事件失败，错误码: " << GetLastError() << std::endl;
        return -1;
    }

    std::vector<HANDLE> events = { hExitEvent, hUpdateEvent };
    bool keepRunning = true;

    std::cout << "子进程已启动，等待事件..." << std::endl;

    while (keepRunning) {
        DWORD waitResult = WaitForMultipleObjects(
            static_cast<DWORD>(events.size()),
            events.data(),
            FALSE,
            INFINITE
        );

        if (waitResult == WAIT_OBJECT_0) {
            std::cout << "收到退出信号，子进程退出。" << std::endl;
            keepRunning = false;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            std::cout << "收到更新信号，读取管道数据..." << std::endl;

            // ---------- 读取主进程发来的路径 ----------
            HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
            uint32_t len = 0;
            DWORD bytesRead = 0;
            if (ReadFile(hStdin, &len, sizeof(len), &bytesRead, NULL) &&
                bytesRead == sizeof(len) && len > 0) {
                std::vector<char> buffer(len + 1, '\0');
                if (ReadFile(hStdin, buffer.data(), len, &bytesRead, NULL) &&
                    bytesRead == len) {
                    std::string newPath(buffer.data());
                    std::cout << "收到新路径: " << newPath << std::endl;
                    // TODO: 后续可根据 newPath 刷新显示
                }
                else {
                    std::cerr << "读取路径内容失败" << std::endl;
                }
            }
            else {
                std::cerr << "读取路径长度失败或长度为0" << std::endl;
            }
            // ------------------------------------------------
        }
        else if (waitResult == WAIT_FAILED) {
            std::cerr << "WaitForMultipleObjects 失败，错误码: " << GetLastError() << std::endl;
            break;
        }
        else {
            std::cerr << "未知的等待结果" << std::endl;
            break;
        }
    }

    CloseHandle(hExitEvent);
    CloseHandle(hUpdateEvent);
    delete pProjectStructure;
    return 0;
}