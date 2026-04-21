// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#pragma once

#include <windows.h>
#include <string>
#include <chrono>
#include <thread>
#include <memory>

#include "SharedTypes.h"
#include "InputSystem.h"
#include "InputCollector.h"

class LevelTreeList {
public:
    LevelTreeList();
    ~LevelTreeList();

    // 初始化 IPC、输入系统等，成功返回 true
    bool Initialize();

    // 运行主循环（阻塞）
    void Run();

    // 停止主循环
    void Stop();

private:
    // 控制台配置
    void ConfigureConsole();
    void SetWindowSizeAndPosition();

    // IPC 初始化与清理
    bool OpenIPC();
    void CloseIPC();

    // 处理 OpenLevel 事件（从共享内存读取路径并加载 LevelData）
    void HandleOpenLevelEvent();

    // 处理输入事件
    void ProcessInputEvents();

    // 成员变量
    bool m_running;

    // IPC 句柄
    HANDLE m_hEvent;
    HANDLE m_hMapFile;
    LPVOID m_pView;

    // 输入系统
    InputSystem* m_inputSystem;                     // 通常指向全局实例
    std::unique_ptr<InputCollector> m_inputCollector;

    // 循环间隔
    std::chrono::milliseconds m_loopInterval;

    // 当前加载的 LevelData
    std::unique_ptr<LevelData> m_currentLevel;

    // 当前选中的对象
    ObjectData* m_selectedObject;
};