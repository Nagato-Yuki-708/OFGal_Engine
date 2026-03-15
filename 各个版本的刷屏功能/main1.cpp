#include <windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include "BMP_Reader.h"

// 启用虚拟终端序列
bool EnableVTMode(HANDLE hOut) {
	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode))
		return false;
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	return SetConsoleMode(hOut, dwMode);
}

// 快速整数转字符串表 (0-255)
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

static FastIntTable g_intTable;

int main() {
	// 1. 加载图片
	BMP_Data img = Read_A_BMP("E:\\Projects\\C++Projects\\OFGal_Engine\\莉可丽丝1.bmp");
	if (img.width <= 0 || img.height <= 0) {
		std::cout << "图片加载失败" << std::endl;
		system("pause");
		return 1;
	}
	std::cout << "图片加载成功: " << img.width << " x " << img.height << std::endl;

	// 2. 控制台初始化
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hConsole == INVALID_HANDLE_VALUE) {
		std::cerr << "获取控制台句柄失败。错误码：" << GetLastError() << std::endl;
		system("pause");
		return 1;
	}

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	WORD originalAttributes = csbi.wAttributes;

	// 设置字体 Consolas, 字号1x1
	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	GetCurrentConsoleFontEx(hConsole, FALSE, &cfi);
	cfi.dwFontSize.X = 1;
	cfi.dwFontSize.Y = 1;
	cfi.FontFamily = FF_DONTCARE;
	cfi.FontWeight = FW_NORMAL;
	wcscpy_s(cfi.FaceName, L"Consolas");
	SetCurrentConsoleFontEx(hConsole, FALSE, &cfi);

	// 最大化窗口
	HWND hwnd = GetConsoleWindow();
	if (hwnd) ShowWindow(hwnd, SW_MAXIMIZE);
	Sleep(100);

	// 获取窗口尺寸
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	SHORT windowWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	SHORT windowHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

	// 设置缓冲区等于窗口大小，隐藏滚动条
	COORD newBufferSize = { windowWidth, windowHeight };
	SetConsoleScreenBufferSize(hConsole, newBufferSize);

	GetConsoleScreenBufferInfo(hConsole, &csbi);
	COORD actualBufferSize = csbi.dwSize;
	std::cout << "画布尺寸（字符单元）: " << actualBufferSize.X << " x " << actualBufferSize.Y << std::endl;

	// 启用 VT 模式
	EnableVTMode(hConsole);

	// 禁用快速编辑模式
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD originalInputMode = 0;
	if (hStdin != INVALID_HANDLE_VALUE && GetConsoleMode(hStdin, &originalInputMode)) {
		DWORD newMode = originalInputMode;
		newMode |= ENABLE_EXTENDED_FLAGS;
		newMode &= ~ENABLE_QUICK_EDIT_MODE;
		SetConsoleMode(hStdin, newMode);
	}

	// 清屏
	system("cls");

	// 3. 绘制图片：每个像素一个空格，背景色为像素颜色
	int drawWidth = min(img.width, static_cast<int>(actualBufferSize.X));
	int drawHeight = min(img.height, static_cast<int>(actualBufferSize.Y));

	// 将光标置于左上角
	SetConsoleCursorPosition(hConsole, COORD{ 0, 0 });

	// 每行缓冲区：每个像素最多20字符 + 换行符
	const int maxLineBytes = drawWidth * 20 + 2;
	char* lineBuffer = new char[maxLineBytes];

	for (int y = 0; y < drawHeight; ++y) {
		char* p = lineBuffer;
		for (int x = 0; x < drawWidth; ++x) {
			const auto& pixel = img.pixels[y * img.width + x];

			// 写入 "\x1b[48;2;"
			*p++ = '\x1b';
			*p++ = '[';
			*p++ = '4';
			*p++ = '8';
			*p++ = ';';
			*p++ = '2';
			*p++ = ';';

			// 红色
			int r = pixel.red;
			memcpy(p, g_intTable.str[r], g_intTable.len[r]);
			p += g_intTable.len[r];
			*p++ = ';';

			// 绿色
			int g = pixel.green;
			memcpy(p, g_intTable.str[g], g_intTable.len[g]);
			p += g_intTable.len[g];
			*p++ = ';';

			// 蓝色
			int b = pixel.blue;
			memcpy(p, g_intTable.str[b], g_intTable.len[b]);
			p += g_intTable.len[b];

			// 后缀 "m "
			*p++ = 'm';
			*p++ = ' ';
		}
		// 行末换行
		*p++ = '\n';

		// 一次性写入一行
		DWORD written;
		WriteConsoleA(hConsole, lineBuffer, static_cast<DWORD>(p - lineBuffer), &written, nullptr);
	}

	delete[] lineBuffer;

	// 恢复原始颜色和属性
	printf("\x1b[0m");
	SetConsoleTextAttribute(hConsole, originalAttributes);

	std::cout << "\n绘制完成。按 Enter 键退出..." << std::endl;
	system("pause");
	return 0;
}