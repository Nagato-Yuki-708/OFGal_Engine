// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#pragma once
#define NOMINMAX
#include "SharedTypes.h"
#include "Json_BPData_ReadWrite.h"
#include "InputSystem.h"
#include "InputCollector.h"
#include "Debug.h"
#include <iostream>

static const char* RESET = "\x1b[0m";
static const char* CYAN = "\x1b[36m";
static const char* YELLOW = "\x1b[33m";
static const char* WHITE = "\x1b[37m";
static const char* ORANGE = "\x1b[38;5;208m";

class VariablesViewer {
public:
	VariablesViewer();
	~VariablesViewer();

	void Run();
private:
    std::wstring currentBPPath;
    BlueprintData currentBPData;

	InputSystem     m_inputSystem;
	InputCollector  m_inputCollector = InputCollector(&m_inputSystem);
	bool isEditing = false;
private:
    void ConfigureConsole();
    void ClearScreen();
    void SetWindowSizeAndPosition();
    void FlushInputBuffer();
    void BuildAndPrintAll();
    void ScrollToTheTop();

    std::string WideToUTF8(const std::wstring& wstr) const;
    int GetConsoleColumns();

    // 工具：计算字符串中可见字符的长度（忽略ANSI转义序列）
    static size_t VisibleLength(const std::string& s);
};