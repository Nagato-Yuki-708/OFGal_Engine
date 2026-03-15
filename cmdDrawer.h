// Copyright 2026 MrSeagull. All Rights Reserved.
#pragma once
// 禁止 Windows.h 定义 min/max 宏，避免与 std::min 冲突
#define NOMINMAX
#include <windows.h>
#include "BMP_Reader.h"  // 用于 BMP_Data 类型

class CmdDrawer {
public:
	CmdDrawer();
	~CmdDrawer();

	// 初始化控制台（设置字体、窗口、缓冲区、VT 模式等）
	bool initialize();

	// 绘制 BMP 图片到控制台
	void draw(const BMP_Data& bmp);

	// 手动清理（可选，析构函数会自动调用）
	void shutdown();

private:
	HANDLE hConsole;
	HANDLE hStdin;
	WORD originalAttributes;
	DWORD originalInputMode;
	bool initialized;

	// 辅助函数：启用 VT 模式
	bool enableVTMode();
};