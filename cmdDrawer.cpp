// Copyright 2026 MrSeagull. All Rights Reserved.
#include "cmdDrawer.h"
#include <iostream>
#include <vector>
#include <algorithm>

// 快速整数转字符串表（0-255）
namespace {
	struct FastIntTable {
		char str[256][4];  // 最多3位数字+'\0'
		int len[256];
		FastIntTable() {
			for (int i = 0; i < 256; ++i) {
				if (i < 10) {
					str[i][0] = '0' + i;
					str[i][1] = '\0';
					len[i] = 1;
				}
				else if (i < 100) {
					str[i][0] = '0' + i / 10;
					str[i][1] = '0' + i % 10;
					str[i][2] = '\0';
					len[i] = 2;
				}
				else {
					str[i][0] = '0' + i / 100;
					str[i][1] = '0' + (i / 10) % 10;
					str[i][2] = '0' + i % 10;
					str[i][3] = '\0';
					len[i] = 3;
				}
			}
		}
	};
	const FastIntTable g_intTable;
}

CmdDrawer::CmdDrawer()
	: hConsole(GetStdHandle(STD_OUTPUT_HANDLE))
	, hStdin(GetStdHandle(STD_INPUT_HANDLE))
	, originalAttributes(0)
	, originalInputMode(0)
	, initialized(false) {
}

CmdDrawer::~CmdDrawer() {
	shutdown();
}

bool CmdDrawer::enableVTMode() {
	DWORD dwMode = 0;
	if (!GetConsoleMode(hConsole, &dwMode)) return false;
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	return SetConsoleMode(hConsole, dwMode);
}

bool CmdDrawer::initialize() {
	if (initialized) return true;

	// 检查句柄有效性
	if (hConsole == INVALID_HANDLE_VALUE || hStdin == INVALID_HANDLE_VALUE) {
		OutputDebugStringA("获取控制台句柄失败。错误码：" + GetLastError());
		return false;
	}

	// 保存原始控制台属性
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
		OutputDebugStringA("获取控制台信息失败");
		return false;
	}
	originalAttributes = csbi.wAttributes;

	// 保存原始输入模式（用于恢复快速编辑模式）
	if (!GetConsoleMode(hStdin, &originalInputMode)) {
		OutputDebugStringA("获取输入模式失败");
		return false;
	}

	// 设置字体为 Consolas，大小 1x1
	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	if (GetCurrentConsoleFontEx(hConsole, FALSE, &cfi)) {
		cfi.dwFontSize.X = 1;
		cfi.dwFontSize.Y = 1;
		cfi.FontFamily = FF_DONTCARE;
		cfi.FontWeight = FW_NORMAL;
		wcscpy_s(cfi.FaceName, L"Consolas");
		SetCurrentConsoleFontEx(hConsole, FALSE, &cfi);
	}

	// 最大化窗口
	HWND hwnd = GetConsoleWindow();
	if (hwnd) ShowWindow(hwnd, SW_MAXIMIZE);
	Sleep(100); // 等待窗口调整

	// 获取窗口尺寸并设置缓冲区等于窗口大小
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	SHORT windowWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	SHORT windowHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	COORD newBufferSize = { windowWidth, windowHeight };
	SetConsoleScreenBufferSize(hConsole, newBufferSize);

	// 启用 VT 模式
	if (!enableVTMode()) {
		OutputDebugStringA("启用 VT 模式失败");
		return false;
	}

	// 禁用快速编辑模式（避免鼠标点击暂停程序）
	DWORD newMode = originalInputMode;
	newMode |= ENABLE_EXTENDED_FLAGS;
	newMode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(hStdin, newMode);

	initialized = true;
	return true;
}

void CmdDrawer::draw(const BMP_Data& bmp) {
	if (!initialized) {
		OutputDebugStringA("绘图器未初始化，请先调用 initialize()");
		return;
	}

	// 获取当前控制台缓冲区尺寸
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	COORD bufferSize = csbi.dwSize;

	// 计算可绘制的最大区域
	int drawWidth = std::min(bmp.width, static_cast<int>(bufferSize.X));
	int drawHeight = std::min(bmp.height, static_cast<int>(bufferSize.Y));

	// 光标置顶
	SetConsoleCursorPosition(hConsole, COORD{ 0, 0 });

	// 行缓冲区（最坏情况：每个像素颜色不同，每像素约20字节 + 换行符）
	std::vector<char> lineBuffer(drawWidth * 20 + 2);
	char* lineBufferPtr = lineBuffer.data();

	for (int y = 0; y < drawHeight; ++y) {
		char* p = lineBufferPtr;
		int lastR = -1, lastG = -1, lastB = -1;

		for (int x = 0; x < drawWidth; ++x) {
			const auto& pixel = bmp.pixels[y * bmp.width + x];

			// 如果颜色变化，输出新的背景色转义序列
			if (pixel.red != lastR || pixel.green != lastG || pixel.blue != lastB) {
				*p++ = '\x1b';
				*p++ = '[';
				*p++ = '4';
				*p++ = '8';
				*p++ = ';';
				*p++ = '2';
				*p++ = ';';

				// 红色
				memcpy(p, g_intTable.str[pixel.red], g_intTable.len[pixel.red]);
				p += g_intTable.len[pixel.red];
				*p++ = ';';

				// 绿色
				memcpy(p, g_intTable.str[pixel.green], g_intTable.len[pixel.green]);
				p += g_intTable.len[pixel.green];
				*p++ = ';';

				// 蓝色
				memcpy(p, g_intTable.str[pixel.blue], g_intTable.len[pixel.blue]);
				p += g_intTable.len[pixel.blue];

				*p++ = 'm';

				lastR = pixel.red;
				lastG = pixel.green;
				lastB = pixel.blue;
			}
			*p++ = ' ';  // 输出空格作为“像素”
		}
		*p++ = '\n';     // 换行

		// 写入一行到控制台
		DWORD written;
		WriteFile(hConsole, lineBufferPtr, static_cast<DWORD>(p - lineBufferPtr), &written, nullptr);
	}

	// 恢复默认颜色
	printf("\x1b[0m");
	// 同时恢复原始控制台属性（虽然 VT 已经重置颜色，但保留以确保完全恢复）
	SetConsoleTextAttribute(hConsole, originalAttributes);
}

void CmdDrawer::shutdown() {
	if (initialized) {
		// 恢复原始控制台属性
		SetConsoleTextAttribute(hConsole, originalAttributes);
		// 恢复原始输入模式（如快速编辑）
		SetConsoleMode(hStdin, originalInputMode);
		// 可选：恢复原始字体？一般不需要，退出后控制台会重置
		initialized = false;
	}
}