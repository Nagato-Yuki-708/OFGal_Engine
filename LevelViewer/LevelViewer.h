// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#pragma once
#include "SharedTypes.h"
#include "RenderingSystem.h"
#include "Json_LevelData_ReadWrite.h"
#include <iostream>

class LevelViewer {
private:
    bool shouldRender = false;

    std::wstring m_currentLevelPath;
    LevelData m_currentLevel;

    // 共享内存句柄与视图
    HANDLE m_hSharedMem;
    void* m_pSharedMemView;

    // 事件句柄
    HANDLE m_hEventLevelChanged;
    HANDLE m_hEventRenderPreview;

    // 存储从命令行传入的长和宽
    Size2DInt oriCanvasSize;

public:
    void Run();

    LevelViewer(int length, int width);
    ~LevelViewer();

    std::string WStringToString(const std::wstring& wstr);
    void ClearScreen();
};