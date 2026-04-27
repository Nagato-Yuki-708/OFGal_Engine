// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#include <iostream>
#include <windows.h> // 需要共享内存 API
#include "LevelViewer.h"

// 与 SharedTypes.h 中 Size2DInt 完全一致的内存布局
struct CanvasSize {
    int x;
    int y;
};

int main()
{
    // 打开由 RenderingSystem 创建的全局共享内存
    HANDLE hMapFile = OpenFileMapping(
        FILE_MAP_READ,          // 只读访问
        FALSE,                  // 不继承句柄
        L"Global\\OFGal_Engine_oriCanvasSize");

    if (hMapFile == NULL) {
        std::cerr << "Error: Cannot open shared memory 'Global\\OFGal_Engine_oriCanvasSize'. "
            << "Make sure the rendering system is running first." << std::endl;
        return 1;
    }

    // 映射视图
    CanvasSize* pCanvas = static_cast<CanvasSize*>(
        MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, sizeof(CanvasSize)));

    if (pCanvas == NULL) {
        std::cerr << "Error: Cannot map view of shared memory." << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }

    // 读取画布尺寸
    int length = pCanvas->x;
    int width = pCanvas->y;

    // 释放资源
    UnmapViewOfFile(pCanvas);
    CloseHandle(hMapFile);

    // 验证读取到的尺寸是否合理（至少需要大于0）
    if (length <= 0 || width <= 0) {
        std::cerr << "Error: Invalid canvas size read from shared memory ("
            << length << ", " << width << ")." << std::endl;
        return 1;
    }

    // 使用读取到的尺寸启动应用
    LevelViewer app(length, width);
    app.Run();

    return 0;
}