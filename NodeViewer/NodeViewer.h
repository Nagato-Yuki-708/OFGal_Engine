// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#pragma once
#define NOMINMAX
#include "SharedTypes.h"
#include "Json_BPData_ReadWrite.h"
#include "InputSystem.h"
#include "InputCollector.h"
#include "Debug.h"
#include <iostream>
#include <sstream>   // for std::ostringstream
#include <cstdio>    // for snprintf

static const char* RESET = "\x1b[0m";
static const char* CYAN = "\x1b[36m";
static const char* YELLOW = "\x1b[33m";
static const char* WHITE = "\x1b[37m";
static const char* ORANGE = "\x1b[38;5;208m";

class NodeViewer {
public:
    NodeViewer();
    ~NodeViewer();

    void Run();
private:
    std::wstring currentBPPath;
    BlueprintData currentBPData;
    int selectedNodeId1 = -1;
    int selectedNodeId2 = -1;

    int selectedPinId1 = -1;
    int selectedPinId2 = -1;

    InputSystem     m_inputSystem;
    InputCollector  m_inputCollector = InputCollector(&m_inputSystem);
    bool isEditing = false;

    // ---- 同步对象 ----
    HANDLE hLoadBPEvent;
    HANDLE hNodeChangedEvent;      // 保留，但本进程主要使用 LoadBP 和 NodeMove
    HANDLE hNodeMoveEvent;
    HANDLE hFileMapping;           // 蓝图路径共享内存
    LPVOID pSharedMem;
    HANDLE hNodeIdMapping;         // 节点 ID 共享内存
    int* pNodeIdSharedMem;

private:
    void ConfigureConsole();
    void ClearScreen();
    void SetWindowSizeAndPosition();
    void FlushInputBuffer();
    void BuildAndPrintAll();
    void ScrollToTheTop();

    std::string WideToUTF8(const std::wstring& wstr) const;
    int GetConsoleColumns();
    std::string TruncateText(const std::string& text, int maxWidth) const;
    static size_t VisibleLength(const std::string& s);

    void MoveToPrev(int target);
    void MoveToNext(int target);
    bool Cut(int target);
    bool Link();
    bool Edit(int target);
};