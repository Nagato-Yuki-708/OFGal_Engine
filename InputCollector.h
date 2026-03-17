#pragma once
#include "Windows.h"
class InputCollector {
public:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	//四个参数分别是：窗口句柄、消息类型、消息参数1、消息参数2
};