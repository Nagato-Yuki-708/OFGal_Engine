// Copyright 2026 MrSeagull. All Rights Reserved.
#include "WindowsSystem.h"
bool WindowsSystem::OpenProjectStructureViewer(const char* ViewerExePath, const char* ProjectRoot) {
    const std::string procName = "ProjectStructureViewer";

    // 1. 创建事件（不变）
    HANDLE hExit = CreateEventA(NULL, FALSE, FALSE, "Global\\OFGal_Engine_ProjectStructureViewer_Exit");
    HANDLE hRefresh = CreateEventA(NULL, FALSE, FALSE, "Global\\OFGal_Engine_ProjectStructureViewer_Refresh");
    if (!hExit || !hRefresh) {
        OutputDebugStringA("创建事件失败，错误码: " + GetLastError());
        if (hExit) CloseHandle(hExit);
        if (hRefresh) CloseHandle(hRefresh);
        return false;
    }
    ProjectStructureViewer_ops["Exit"] = hExit;
    ProjectStructureViewer_ops["Refresh"] = hRefresh;

    // 2. 创建匿名管道（用于向子进程发送路径）
    HANDLE hPipeRead = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };  // 可继承
    if (!CreatePipe(&hPipeRead, &ProjectStructureViewer_ops["Write"], &sa, 0)) {
        OutputDebugStringA("创建管道失败，错误码: " + GetLastError());
        CleanUpHandles(procName);
        return false;
    }
    // 确保写端不被子进程继承（我们只希望子进程继承读端）
    SetHandleInformation(ProjectStructureViewer_ops["Write"], HANDLE_FLAG_INHERIT, 0);

    // 3. 设置 STARTUPINFO，将管道读端作为子进程的标准输入
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hPipeRead;          // 子进程从此读取路径
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);  // 仍使用新控制台的输出
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = { 0 };

    // 4. 构造命令行
    std::string cmdLine = "\"" + std::string(ViewerExePath) + "\"";
    if (ProjectRoot && strlen(ProjectRoot) > 0) {
        cmdLine += " \"";
        cmdLine += ProjectRoot;
        cmdLine += "\"";
    }

    // 5. 创建子进程（CREATE_NEW_CONSOLE 仍会创建新窗口，但其标准输入被重定向到管道）
    BOOL success = CreateProcessA(
        NULL,
        const_cast<LPSTR>(cmdLine.c_str()),
        NULL, NULL,
        TRUE,                // 必须为 TRUE 才能继承句柄
        CREATE_NEW_CONSOLE,
        NULL, NULL,
        &si, &pi
    );

    // 关闭子进程的读端（主进程不再需要）
    CloseHandle(hPipeRead);

    if (!success) {
        OutputDebugStringA("启动子进程失败，错误码: " + GetLastError());
        CloseHandle(ProjectStructureViewer_ops["Write"]);
        ProjectStructureViewer_ops["Write"] = NULL;
        CleanUpHandles(procName);
        return false;
    }

    // 6. 存储句柄
    ChildProcess[procName] = pi.hProcess;
    ChildThread[procName] = pi.hThread;

    return true;
}
bool WindowsSystem::RefreshProjectStructureViewer() {
    // 1. 先通过管道发送 currentProjectDirectory（确保数据先到达）
    auto itWrite = ProjectStructureViewer_ops.find("Write");
    if (itWrite != ProjectStructureViewer_ops.end() && itWrite->second && currentProjectDirectory) {
        DWORD bytesWritten;
        uint32_t len = static_cast<uint32_t>(strlen(currentProjectDirectory));
        if (!WriteFile(itWrite->second, &len, sizeof(len), &bytesWritten, NULL) || bytesWritten != sizeof(len)) {
            OutputDebugStringA("写入管道长度失败");
            return false;
        }
        if (!WriteFile(itWrite->second, currentProjectDirectory, len, &bytesWritten, NULL) || bytesWritten != len) {
            OutputDebugStringA("写入管道数据失败");
            return false;
        }
        FlushFileBuffers(itWrite->second);
    }

    // 2. 再触发刷新事件
    auto itRefresh = ProjectStructureViewer_ops.find("Refresh");
    if (itRefresh != ProjectStructureViewer_ops.end() && itRefresh->second) {
        SetEvent(itRefresh->second);
        return true;
    }
    return false;
}
bool WindowsSystem::CloseProjectStructureViewer() {
    const std::string procName = "ProjectStructureViewer";

    // 1. 发送退出事件
    auto itExit = ProjectStructureViewer_ops.find("Exit");
    if (itExit != ProjectStructureViewer_ops.end() && itExit->second) {
        SetEvent(itExit->second);
    }

    // 2. 等待子进程结束（超时后强制终止）
    auto procIt = ChildProcess.find(procName);
    if (procIt != ChildProcess.end() && procIt->second) {
        DWORD waitResult = WaitForSingleObject(procIt->second, 3000); // 等待3秒
        if (waitResult == WAIT_TIMEOUT) {
            TerminateProcess(procIt->second, 0);
        }
    }

    // 3. 清理所有相关句柄
    CleanUpHandles(procName);
    return true;
}

void WindowsSystem::CleanUpHandles(const std::string& processName) {
    // 清理事件句柄（ProjectStructureViewer 专用）
    if (processName == "ProjectStructureViewer") {
        for (auto& pair : ProjectStructureViewer_ops) {
            if (pair.second) {
                CloseHandle(pair.second);
                pair.second = NULL;
            }
        }
    }

    // 清理进程和线程句柄
    auto procIt = ChildProcess.find(processName);
    if (procIt != ChildProcess.end() && procIt->second) {
        CloseHandle(procIt->second);
        ChildProcess.erase(procIt);
    }

    auto threadIt = ChildThread.find(processName);
    if (threadIt != ChildThread.end() && threadIt->second) {
        CloseHandle(threadIt->second);
        ChildThread.erase(threadIt);
    }
}