// Copyright 2026 MrSeagull. All Rights Reserved.
#include "cmdDrawer.h"
#include <iostream>
#include <algorithm>
#include <string>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

// ---------- 内部辅助结构 ----------
namespace {
	// 快速整数转字符串表（0-255）
	struct FastIntTable {
		char str[256][4];
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

	HANDLE g_hConsole = INVALID_HANDLE_VALUE;
	HANDLE g_hStdin = INVALID_HANDLE_VALUE;
	WORD g_originalAttributes = 0;
	DWORD g_originalInputMode = 0;
	bool g_initialized = false;
	Size2DInt g_maxCanvasSize = { 0, 0 };

	bool enableVTMode() {
		DWORD dwMode = 0;
		if (!GetConsoleMode(g_hConsole, &dwMode)) return false;
		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		return SetConsoleMode(g_hConsole, dwMode);
	}

	bool isWindowsTerminal() {
		HWND hwnd = GetConsoleWindow();
		if (!hwnd) return false;
		DWORD pid = 0;
		GetWindowThreadProcessId(hwnd, &pid);
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (!hProcess) return false;
		wchar_t path[MAX_PATH];
		if (GetModuleFileNameExW(hProcess, NULL, path, MAX_PATH)) {
			std::wstring procPath(path);
			size_t pos = procPath.find_last_of(L"\\");
			if (pos != std::wstring::npos) {
				std::wstring fileName = procPath.substr(pos + 1);
				if (_wcsicmp(fileName.c_str(), L"WindowsTerminal.exe") == 0) {
					CloseHandle(hProcess);
					return true;
				}
			}
		}
		CloseHandle(hProcess);
		return false;
	}
}

bool initializeConsoleDrawer() {
	if (g_initialized) return true;
	if (isWindowsTerminal()) return false;

	g_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	g_hStdin = GetStdHandle(STD_INPUT_HANDLE);
	if (g_hConsole == INVALID_HANDLE_VALUE || g_hStdin == INVALID_HANDLE_VALUE) {
		std::string msg = "获取控制台句柄失败。错误码：" + std::to_string(GetLastError());
		OutputDebugStringA(msg.c_str());
		return false;
	}

	SetConsoleTitleW(L"OFGal_Engine/LevelViewer");

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(g_hConsole, &csbi)) {
		OutputDebugStringA("获取控制台信息失败");
		return false;
	}
	g_originalAttributes = csbi.wAttributes;

	if (!GetConsoleMode(g_hStdin, &g_originalInputMode)) {
		OutputDebugStringA("获取输入模式失败");
		return false;
	}

	// 设置极小字体（1×1 像素），配合后续像素级绘图
	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	if (GetCurrentConsoleFontEx(g_hConsole, FALSE, &cfi)) {
		cfi.dwFontSize.X = 1;
		cfi.dwFontSize.Y = 1;
		cfi.FontFamily = FF_DONTCARE;
		cfi.FontWeight = FW_NORMAL;
		wcscpy_s(cfi.FaceName, L"Consolas");
		SetCurrentConsoleFontEx(g_hConsole, FALSE, &cfi);
	}

	HWND hwnd = GetConsoleWindow();
	if (hwnd) {
		// 去掉最大化按钮和可调整边框
		LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
		style &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
		SetWindowLongPtr(hwnd, GWL_STYLE, style);

		// 计算相对于主窗口的缩放比例
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

		// 目标窗口尺寸（像素）
		int targetWidth = static_cast<int>(1610.0f * scaleX);
		int targetHeight = static_cast<int>(1010.0f * scaleY);

		SetWindowPos(hwnd, nullptr,
			static_cast<int>(430.0f * scaleX),
			static_cast<int>(0.0f),
			targetWidth,
			targetHeight,
			SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

		// 等待窗口更新，确保新尺寸生效
		Sleep(100);

		// ===== 修正点：避免因旧缓冲区过小导致 srWindow 读取受限 =====
		// 字体已设为 1×1，因此字符尺寸 ≈ 1 像素，缓冲区行列数可直接取目标像素尺寸
		SHORT desiredCols = static_cast<SHORT>(targetWidth);
		SHORT desiredRows = static_cast<SHORT>(targetHeight);

		// 将屏幕缓冲区显式扩大到足以容纳新窗口
		COORD newBufferSize = { desiredCols, desiredRows };
		SetConsoleScreenBufferSize(g_hConsole, newBufferSize);

		// 现在获取的 csbi 才是真实视口信息
		GetConsoleScreenBufferInfo(g_hConsole, &csbi);
	}
	else {
		// 如果没有窗口句柄，回退到直接查询
		GetConsoleScreenBufferInfo(g_hConsole, &csbi);
	}

	SHORT windowWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	SHORT windowHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	COORD finalBufferSize = { windowWidth, windowHeight };
	SetConsoleScreenBufferSize(g_hConsole, finalBufferSize);

	g_maxCanvasSize.x = windowWidth;
	g_maxCanvasSize.y = windowHeight;

	if (!enableVTMode()) {
		OutputDebugStringA("启用 VT 模式失败");
		return false;
	}

	DWORD newMode = g_originalInputMode;
	newMode |= ENABLE_EXTENDED_FLAGS;
	newMode &= ~ENABLE_QUICK_EDIT_MODE;
	SetConsoleMode(g_hStdin, newMode);

	g_initialized = true;
	return true;
}

Size2DInt getMaxCanvasSize() {
	return g_maxCanvasSize;
}

void drawFrame(const Frame& frame) {
	if (!g_initialized) {
		OutputDebugStringA("绘图器未初始化，请先调用 initializeConsoleDrawer()");
		return;
	}

	int drawWidth = frame.width;
	int drawHeight = frame.height;

	SetConsoleCursorPosition(g_hConsole, COORD{ 0, 0 });

	std::vector<char> lineBuffer(drawWidth * 20 + 2);
	char* lineBufferPtr = lineBuffer.data();

	for (int y = 0; y < drawHeight; ++y) {
		char* p = lineBufferPtr;
		int lastR = -1, lastG = -1, lastB = -1;

		for (int x = 0; x < drawWidth; ++x) {
			const auto& pixel = frame.pixels[y * drawWidth + x];

			if (pixel.red != lastR || pixel.green != lastG || pixel.blue != lastB) {
				*p++ = '\x1b';
				*p++ = '[';
				*p++ = '4';
				*p++ = '8';
				*p++ = ';';
				*p++ = '2';
				*p++ = ';';

				memcpy(p, g_intTable.str[pixel.red], g_intTable.len[pixel.red]);
				p += g_intTable.len[pixel.red];
				*p++ = ';';

				memcpy(p, g_intTable.str[pixel.green], g_intTable.len[pixel.green]);
				p += g_intTable.len[pixel.green];
				*p++ = ';';

				memcpy(p, g_intTable.str[pixel.blue], g_intTable.len[pixel.blue]);
				p += g_intTable.len[pixel.blue];

				*p++ = 'm';

				lastR = pixel.red;
				lastG = pixel.green;
				lastB = pixel.blue;
			}
			*p++ = ' ';
		}
		*p++ = '\n';

		DWORD written;
		WriteFile(g_hConsole, lineBufferPtr, static_cast<DWORD>(p - lineBufferPtr), &written, nullptr);
	}

	printf("\x1b[0m");
	SetConsoleTextAttribute(g_hConsole, g_originalAttributes);
}

void shutdownConsoleDrawer() {
	if (g_initialized) {
		SetConsoleTextAttribute(g_hConsole, g_originalAttributes);
		SetConsoleMode(g_hStdin, g_originalInputMode);
		g_initialized = false;
	}
}