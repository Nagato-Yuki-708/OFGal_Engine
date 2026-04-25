// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.

#include "DetailViewer.h"
#include <windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <conio.h>      // _getch, _putch
#include <algorithm>    // std::remove_if

// 共享内存大小定义
static const DWORD PATH_SHARED_MEM_SIZE = 1024;
static const DWORD OBJECT_SHARED_MEM_SIZE = 1024;

// ANSI 颜色转义常量
static const char* RESET = "\x1b[0m";
static const char* CYAN = "\x1b[36m";
static const char* YELLOW = "\x1b[33m";
static const char* WHITE = "\x1b[37m";

// 组件类型转字符串
static const char* ComponentTypeToString(ComponentType type) {
    switch (type) {
    case ComponentType::Transform:   return "Transform";
    case ComponentType::Picture:     return "Picture";
    case ComponentType::Textblock:   return "Textblock";
    case ComponentType::TriggerArea: return "TriggerArea";
    case ComponentType::Blueprint:   return "Blueprint";
    default:                         return "None";
    }
}

DetailViewer::DetailViewer()
    : m_hEventPathUpdate(nullptr)
    , m_hEventDataChanged(nullptr)
    , m_hEventObjectChanged(nullptr)
    , m_hEventRenderPreview(nullptr)
    , m_hMapPath(nullptr)
    , m_hMapObject(nullptr)
    , m_pViewPath(nullptr)
    , m_pViewObject(nullptr)
    , m_inputCollector(&m_inputSystem)
{
    SetWindowSizeAndPosition();
    SetConsoleTitleW(L"OFGal_Engine/DetailViewer");
    ConfigureConsole();

    m_hEventPathUpdate = OpenEventW(SYNCHRONIZE, FALSE, L"Global\\OFGal_Engine_LevelTreeList_DetailViewer_PathUpdate");
    m_hEventDataChanged = OpenEventW(SYNCHRONIZE, FALSE, L"Global\\OFGal_Engine_LevelTreeList_DetailViewer_DataChanged");
    m_hEventObjectChanged = OpenEventW(SYNCHRONIZE, FALSE, L"Global\\OFGal_Engine_LevelTreeList_DetailViewer_ObjectChanged");

    // 创建渲染预览通知事件
    m_hEventRenderPreview = CreateEventW(
        NULL, TRUE, FALSE,
        L"Global\\OFGal_Engine_DetailViewer_RenderPreviewFrame"
    );
    if (!m_hEventRenderPreview) {
        // 事件创建失败时记录调试输出，但程序继续运行
        OutputDebugStringA("[DetailViewer] Failed to create render preview event.\n");
    }

    m_hMapPath = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Global\\OFGal_Engine_LevelTreeList_DetailViewer_PathSharedMem");
    if (m_hMapPath)
        m_pViewPath = MapViewOfFile(m_hMapPath, FILE_MAP_READ, 0, 0, PATH_SHARED_MEM_SIZE);

    m_hMapObject = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Global\\OFGal_Engine_LevelTreeList_DetailViewer_ObjectSharedMem");
    if (m_hMapObject)
        m_pViewObject = MapViewOfFile(m_hMapObject, FILE_MAP_READ, 0, 0, OBJECT_SHARED_MEM_SIZE);

    m_inputSystem.SetGlobalCapture(false);

    // 强制处理消息队列，让窗口位置/大小立即生效
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

DetailViewer::~DetailViewer()
{
    if (m_pViewPath)   UnmapViewOfFile(m_pViewPath);
    if (m_pViewObject) UnmapViewOfFile(m_pViewObject);
    if (m_hMapPath)    CloseHandle(m_hMapPath);
    if (m_hMapObject)  CloseHandle(m_hMapObject);
    if (m_hEventPathUpdate)   CloseHandle(m_hEventPathUpdate);
    if (m_hEventDataChanged)  CloseHandle(m_hEventDataChanged);
    if (m_hEventObjectChanged) CloseHandle(m_hEventObjectChanged);
    if (m_hEventRenderPreview) CloseHandle(m_hEventRenderPreview);
}

void DetailViewer::ClearScreen() {
    std::cout << "\x1b[2J\x1b[H" << std::flush;
}

void DetailViewer::FlushInputBuffer() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(hIn);
}

ObjectData* DetailViewer::FindObjectByName(const std::string& name) const {
    if (!m_currentLevel) return nullptr;

    std::function<ObjectData* (const std::map<std::string, ObjectData*>&)> search =
        [&](const std::map<std::string, ObjectData*>& container) -> ObjectData* {
        for (const auto& [objName, obj] : container) {
            if (objName == name) return obj;
            if (!obj->objects.empty()) {
                ObjectData* found = search(obj->objects);
                if (found) return found;
            }
        }
        return nullptr;
        };
    return search(m_currentLevel->objects);
}

void DetailViewer::ConfigureConsole() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hIn, &dwMode)) {
            dwMode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_INSERT_MODE);
            dwMode |= ENABLE_EXTENDED_FLAGS;
            SetConsoleMode(hIn, dwMode);
        }
    }
}

void DetailViewer::SetWindowSizeAndPosition() {
    HWND hwndConsole = GetConsoleWindow();
    if (hwndConsole) {
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
        SetWindowPos(hwndConsole, nullptr,
            static_cast<int>(2040.0f * scaleX),
            static_cast<int>(0.0f),
            static_cast<int>(520.0f * scaleX),
            static_cast<int>(1480.0f * scaleY),
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void DetailViewer::UpdateExistingComponents() {
    m_existingComponents.clear();
    if (!m_currentObject) return;

    if (m_currentObject->Transform.has_value())   m_existingComponents.push_back(ComponentType::Transform);
    if (m_currentObject->Picture.has_value())     m_existingComponents.push_back(ComponentType::Picture);
    if (m_currentObject->Textblock.has_value())   m_existingComponents.push_back(ComponentType::Textblock);
    if (m_currentObject->TriggerArea.has_value()) m_existingComponents.push_back(ComponentType::TriggerArea);
    if (m_currentObject->Blueprint.has_value())   m_existingComponents.push_back(ComponentType::Blueprint);
}

void DetailViewer::PrintHelpText() {
    std::cout << CYAN << "W/up  last component\nS/down  next component\nF  Edit this component" << RESET << "\n";
}

void DetailViewer::PrintObjectComponents() {
    if (!m_currentObject) {
        std::cout << "No object selected." << std::endl;
        return;
    }

    int componentCount = static_cast<int>(m_existingComponents.size());
    std::cout << "Components in object \"" << m_currentObject->name << "\": " << componentCount << std::endl;

    if (componentCount == 0) return;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);

    const char* sep = "==============================";
    const char* subSep = "------------------------------";

    // 当前选中组件类型
    oss << WHITE << sep << "\n";
    oss << "Now selected " << CYAN << ComponentTypeToString(m_selectedComponent.type) << WHITE << "\n";
    oss << sep << "\n" << RESET;

    // 获取各组件指针的辅助 lambda
    auto getComponentPtr = [&](ComponentType type) -> void* {
        switch (type) {
        case ComponentType::Transform:   return &m_currentObject->Transform.value();
        case ComponentType::Picture:     return &m_currentObject->Picture.value();
        case ComponentType::Textblock:   return &m_currentObject->Textblock.value();
        case ComponentType::TriggerArea: return &m_currentObject->TriggerArea.value();
        case ComponentType::Blueprint:   return &m_currentObject->Blueprint.value();
        default: return nullptr;
        }
        };

    // 遍历所有存在的组件并打印
    for (ComponentType type : m_existingComponents) {
        bool isSelected = (type == m_selectedComponent.type);
        const char* nameColor = isSelected ? CYAN : WHITE;

        oss << nameColor << ComponentTypeToString(type) << RESET << "\n";
        oss << WHITE << subSep << RESET << "\n";

        void* ptr = getComponentPtr(type);
        if (type == ComponentType::Transform) {
            auto& comp = *static_cast<TransformComponent*>(ptr);
            oss << YELLOW << "Location" << RESET << " : x = " << comp.Location.x
                << " | y = " << comp.Location.y << " | z = " << comp.Location.z << "\n";
            oss << YELLOW << "Rotation" << RESET << " : r = " << comp.Rotation.r << "\n";
            oss << YELLOW << "Scale" << RESET << " : x = " << comp.Scale.x
                << " | y = " << comp.Scale.y << "\n";
        }
        else if (type == ComponentType::Picture) {
            auto& comp = *static_cast<PictureComponent*>(ptr);
            oss << YELLOW << "Path" << RESET << " : " << comp.Path << "\n";
            oss << YELLOW << "Location" << RESET << " : x = " << comp.Location.x
                << " | y = " << comp.Location.y << " | z = " << comp.Location.z << "\n";
            oss << YELLOW << "Rotation" << RESET << " : r = " << comp.Rotation.r << "\n";
            oss << YELLOW << "Size" << RESET << " : x = " << comp.Size.x
                << " | y = " << comp.Size.y << "\n";
        }
        else if (type == ComponentType::Textblock) {
            auto& comp = *static_cast<TextblockComponent*>(ptr);
            oss << YELLOW << "Location" << RESET << " : x = " << comp.Location.x
                << " | y = " << comp.Location.y << "\n";
            oss << YELLOW << "Size" << RESET << " : x = " << comp.Size.x
                << " | y = " << comp.Size.y << "\n";
            oss << YELLOW << "Text Component" << RESET << " : " << comp.Text.component << "\n";
            oss << YELLOW << "Font Size" << RESET << " : " << comp.Text.Font_size << "\n";
            oss << YELLOW << "ANSI Print" << RESET << " : " << (comp.Text.ANSI_Print ? "true" : "false") << "\n";
            oss << YELLOW << "Scale" << RESET << " : x = " << comp.Scale.x
                << " | y = " << comp.Scale.y << "\n";
        }
        else if (type == ComponentType::TriggerArea) {
            auto& comp = *static_cast<TriggerAreaComponent*>(ptr);
            oss << YELLOW << "Location" << RESET << " : x = " << comp.Location.x
                << " | y = " << comp.Location.y << "\n";
            oss << YELLOW << "Size" << RESET << " : x = " << comp.Size.x
                << " | y = " << comp.Size.y << "\n";
        }
        else if (type == ComponentType::Blueprint) {
            auto& comp = *static_cast<BlueprintComponent*>(ptr);
            oss << YELLOW << "Path" << RESET << " : " << comp.Path << "\n";
        }

        oss << WHITE << sep << RESET << "\n";
    }

    std::cout << oss.str();
}

// 逐字符读取一行，支持退格与 Esc 取消，返回 true 表示正常结束输入
bool DetailViewer::ReadLineWithCancel(std::string& output) {
    output.clear();
    while (true) {
        int ch = _getch();
        if (ch == 0 || ch == 0xE0) {
            // 功能键，忽略第二个字节（方向键等），这里不处理
            _getch();
            continue;
        }
        if (ch == 27) { // Esc
            output.clear();
            return false;
        }
        if (ch == '\r' || ch == '\n') {
            _putch('\n');
            return true;
        }
        if (ch == '\b') {
            if (!output.empty()) {
                output.pop_back();
                _putch('\b');
                _putch(' ');
                _putch('\b');
            }
            continue;
        }
        // 普通字符
        output.push_back(static_cast<char>(ch));
        _putch(ch);
    }
}

// 编辑当前选中组件
void DetailViewer::EditCurrentComponent() {
    if (!m_currentObject ||
        m_selectedComponent.type == ComponentType::None ||
        m_existingComponents.empty())
        return;

    const char* compName = ComponentTypeToString(m_selectedComponent.type);

    FlushInputBuffer();

    ClearScreen();
    std::cout << "Editing " << CYAN << compName << RESET << " of object \""
        << m_currentObject->name << "\"\n";
    std::cout << "(Press Escape or enter \"#esc#\" to cancel at any time)\n\n";

    // 根据组件类型编辑各自的字段
    switch (m_selectedComponent.type) {
    case ComponentType::Transform: {
        auto& orig = *static_cast<TransformComponent*>(m_selectedComponent.ptr);
        TransformComponent temp = orig;

        // Location.x
        std::cout << YELLOW << "Location.x" << RESET << " = " << orig.Location.x << "\n";
        std::cout << "Enter new value: " << std::flush;
        std::string input;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) {
            try { temp.Location.x = std::stof(input); }
            catch (...) { /* 忽略无效输入，保持原值 */ }
        }
        // Location.y
        std::cout << YELLOW << "Location.y" << RESET << " = " << orig.Location.y << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) {
            try { temp.Location.y = std::stof(input); }
            catch (...) {}
        }
        // Location.z
        std::cout << YELLOW << "Location.z" << RESET << " = " << orig.Location.z << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) {
            try { temp.Location.z = std::stoi(input); }
            catch (...) {}
        }
        // Rotation.r
        std::cout << YELLOW << "Rotation.r" << RESET << " = " << orig.Rotation.r << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) {
            try { temp.Rotation.r = std::stof(input); }
            catch (...) {}
        }
        // Scale.x
        std::cout << YELLOW << "Scale.x" << RESET << " = " << orig.Scale.x << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) {
            try { temp.Scale.x = std::stof(input); }
            catch (...) {}
        }
        // Scale.y
        std::cout << YELLOW << "Scale.y" << RESET << " = " << orig.Scale.y << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) {
            try { temp.Scale.y = std::stof(input); }
            catch (...) {}
        }

        // 应用修改
        orig = temp;
        if (!m_currentLevelPath.empty()) {
            if (WriteLevelData(m_currentLevelPath, *m_currentLevel)) {
                std::cout << "\nComponent updated and saved to file.\n";
                Sleep(1000);
                if (m_hEventRenderPreview) {
                    SetEvent(m_hEventRenderPreview); // 发出渲染预览通知
                }
            }
            else {
                std::cout << "\nComponent updated in memory, but failed to save file.\n";
            }
        }
        else {
            std::cout << "\nComponent updated in memory (no file path).\n";
        }
        break;
    }
    case ComponentType::Picture: {
        auto& orig = *static_cast<PictureComponent*>(m_selectedComponent.ptr);
        PictureComponent temp = orig;

        // Path
        std::cout << YELLOW << "Path" << RESET << " = \"" << orig.Path << "\"\n";
        std::cout << "Enter new value: " << std::flush;
        std::string input;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) temp.Path = input;
        // Location.x
        std::cout << YELLOW << "Location.x" << RESET << " = " << orig.Location.x << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Location.x = std::stof(input); } catch (...) {} }
        // Location.y
        std::cout << YELLOW << "Location.y" << RESET << " = " << orig.Location.y << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Location.y = std::stof(input); } catch (...) {} }
        // Location.z
        std::cout << YELLOW << "Location.z" << RESET << " = " << orig.Location.z << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Location.z = std::stoi(input); } catch (...) {} }
        // Rotation.r
        std::cout << YELLOW << "Rotation.r" << RESET << " = " << orig.Rotation.r << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Rotation.r = std::stof(input); } catch (...) {} }
        // Size.x
        std::cout << YELLOW << "Size.x" << RESET << " = " << orig.Size.x << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Size.x = std::stof(input); } catch (...) {} }
        // Size.y
        std::cout << YELLOW << "Size.y" << RESET << " = " << orig.Size.y << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Size.y = std::stof(input); } catch (...) {} }

        orig = temp;
        std::cout << "\nComponent updated successfully.\n";
        break;
    }
    case ComponentType::Textblock: {
        auto& orig = *static_cast<TextblockComponent*>(m_selectedComponent.ptr);
        TextblockComponent temp = orig;

        // Location.x
        std::cout << YELLOW << "Location.x" << RESET << " = " << orig.Location.x << "\n";
        std::cout << "Enter new value: " << std::flush;
        std::string input;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Location.x = std::stoi(input); } catch (...) {} }
        // Location.y
        std::cout << YELLOW << "Location.y" << RESET << " = " << orig.Location.y << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Location.y = std::stoi(input); } catch (...) {} }
        // Size.x
        std::cout << YELLOW << "Size.x" << RESET << " = " << orig.Size.x << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Size.x = std::stoi(input); } catch (...) {} }
        // Size.y
        std::cout << YELLOW << "Size.y" << RESET << " = " << orig.Size.y << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Size.y = std::stoi(input); } catch (...) {} }
        // Text.component
        std::cout << YELLOW << "Text.component" << RESET << " = \"" << orig.Text.component << "\"\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) temp.Text.component = input;
        // Font_size
        std::cout << YELLOW << "Font Size" << RESET << " = " << orig.Text.Font_size << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Text.Font_size = std::stoi(input); } catch (...) {} }
        // ANSI Print
        std::cout << YELLOW << "ANSI Print" << RESET << " = " << (orig.Text.ANSI_Print ? "true" : "false") << "\n";
        std::cout << "Enter new value (true/false): " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) {
            std::string lower;
            lower.resize(input.size());
            std::transform(input.begin(), input.end(), lower.begin(), ::tolower);
            temp.Text.ANSI_Print = (lower == "true" || lower == "1");
        }
        // Scale.x
        std::cout << YELLOW << "Scale.x" << RESET << " = " << orig.Scale.x << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Scale.x = std::stof(input); } catch (...) {} }
        // Scale.y
        std::cout << YELLOW << "Scale.y" << RESET << " = " << orig.Scale.y << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Scale.y = std::stof(input); } catch (...) {} }

        orig = temp;
        std::cout << "\nComponent updated successfully.\n";
        break;
    }
    case ComponentType::TriggerArea: {
        auto& orig = *static_cast<TriggerAreaComponent*>(m_selectedComponent.ptr);
        TriggerAreaComponent temp = orig;

        // Location.x
        std::cout << YELLOW << "Location.x" << RESET << " = " << orig.Location.x << "\n";
        std::cout << "Enter new value: " << std::flush;
        std::string input;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Location.x = std::stof(input); } catch (...) {} }
        // Location.y
        std::cout << YELLOW << "Location.y" << RESET << " = " << orig.Location.y << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Location.y = std::stof(input); } catch (...) {} }
        // Size.x
        std::cout << YELLOW << "Size.x" << RESET << " = " << orig.Size.x << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Size.x = std::stof(input); } catch (...) {} }
        // Size.y
        std::cout << YELLOW << "Size.y" << RESET << " = " << orig.Size.y << "\n";
        std::cout << "Enter new value: " << std::flush;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) { try { temp.Size.y = std::stof(input); } catch (...) {} }

        orig = temp;
        std::cout << "\nComponent updated successfully.\n";
        break;
    }
    case ComponentType::Blueprint: {
        auto& orig = *static_cast<BlueprintComponent*>(m_selectedComponent.ptr);
        BlueprintComponent temp = orig;

        // Path
        std::cout << YELLOW << "Path" << RESET << " = \"" << orig.Path << "\"\n";
        std::cout << "Enter new value: " << std::flush;
        std::string input;
        if (!ReadLineWithCancel(input) || input == "#esc#") goto cancel;
        if (!input.empty()) temp.Path = input;

        orig = temp;
        std::cout << "\nComponent updated successfully.\n";
        break;
    }
    default:
        break;
    }

    // 短暂停顿以便用户看到成功信息
    Sleep(1000);
    isEditing = false;
    return;

cancel:
    std::cout << "\nEdit cancelled.\n";
    Sleep(1000);
    isEditing = false;
}

void DetailViewer::Run()
{
    if (!m_bindingsAdded) {
        m_inputCollector.AddBinding({ 'W',        Modifier::None, KeyCode::W,    true });
        m_inputCollector.AddBinding({ VK_UP,      Modifier::None, KeyCode::Up,   true });
        m_inputCollector.AddBinding({ 'S',        Modifier::None, KeyCode::S,    true });
        m_inputCollector.AddBinding({ VK_DOWN,    Modifier::None, KeyCode::Down, true });
        m_inputCollector.AddBinding({ 'F',        Modifier::None, KeyCode::F,    true });
        m_bindingsAdded = true;
    }

    while (true) {
        // 1. PathUpdate 事件
        if (m_hEventPathUpdate &&
            WaitForSingleObject(m_hEventPathUpdate, 0) == WAIT_OBJECT_0 &&
            !isEditing)
        {
            if (m_pViewPath) {
                const wchar_t* wpath = static_cast<const wchar_t*>(m_pViewPath);
                std::wstring ws(wpath);
                int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string utf8Path(len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &utf8Path[0], len, nullptr, nullptr);

                try {
                    LevelData newLevel = ReadLevelData(utf8Path);
                    m_currentLevel = std::make_unique<LevelData>(std::move(newLevel));
                    m_currentLevelPath = utf8Path;
                    m_currentObject = nullptr;
                    m_selectedComponent = { ComponentType::None, nullptr };
                    m_existingComponents.clear();
                    OutputDebugStringA("[DetailViewer] PathUpdate: level reloaded.\n");
                }
                catch (const std::exception& e) {
                    m_currentLevelPath.clear();
                    OutputDebugStringA(("[DetailViewer] ERROR reloading level: " + std::string(e.what()) + "\n").c_str());
                }
            }

            ClearScreen();
            PrintHelpText();
            if (m_currentLevel) {
                std::cout << "Objects in level: " << m_currentLevel->objects.size() << std::endl;
            }
            else {
                std::cout << "No level loaded." << std::endl;
            }
            ResetEvent(m_hEventPathUpdate);
        }

        // 2. ObjectChanged 事件
        if (m_hEventObjectChanged &&
            WaitForSingleObject(m_hEventObjectChanged, 0) == WAIT_OBJECT_0 &&
            !isEditing)
        {
            if (m_pViewObject && m_currentLevel) {
                const wchar_t* wObjName = static_cast<const wchar_t*>(m_pViewObject);
                std::wstring wsName(wObjName);
                int len = WideCharToMultiByte(CP_UTF8, 0, wsName.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string name(len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, wsName.c_str(), -1, &name[0], len, nullptr, nullptr);

                ObjectData* foundObj = FindObjectByName(name);
                if (foundObj) {
                    m_currentObject = foundObj;
                    UpdateExistingComponents();

                    m_selectedComponent = { ComponentType::None, nullptr };
                    if (!m_existingComponents.empty()) {
                        m_selectedComponent.type = m_existingComponents.front();
                        switch (m_selectedComponent.type) {
                        case ComponentType::Transform:   m_selectedComponent.ptr = &m_currentObject->Transform.value(); break;
                        case ComponentType::Picture:     m_selectedComponent.ptr = &m_currentObject->Picture.value(); break;
                        case ComponentType::Textblock:   m_selectedComponent.ptr = &m_currentObject->Textblock.value(); break;
                        case ComponentType::TriggerArea: m_selectedComponent.ptr = &m_currentObject->TriggerArea.value(); break;
                        case ComponentType::Blueprint:   m_selectedComponent.ptr = &m_currentObject->Blueprint.value(); break;
                        }
                    }
                    OutputDebugStringA(("[DetailViewer] ObjectChanged: selected " + name + "\n").c_str());
                }
                else {
                    OutputDebugStringA(("[DetailViewer] ObjectChanged: object not found - " + name + "\n").c_str());
                    m_currentObject = nullptr;
                    m_selectedComponent = { ComponentType::None, nullptr };
                    m_existingComponents.clear();
                }
            }

            ClearScreen();
            PrintHelpText();
            PrintObjectComponents();
            ResetEvent(m_hEventObjectChanged);
        }

        // 3. 输入处理
        if (!isEditing)
            m_inputCollector.update();
        const auto& events = m_inputSystem.getEvents();
        for (const auto& ev : events) {
            if (ev.type == InputType::KeyDown) {
                switch (ev.key) {
                case KeyCode::W:
                case KeyCode::Up:
                    OutputDebugStringA("[DetailViewer] Branch 1: W/Up (last component)\n");
                    if (m_currentObject && !m_existingComponents.empty()) {
                        auto it = std::find(m_existingComponents.begin(), m_existingComponents.end(), m_selectedComponent.type);
                        if (it != m_existingComponents.end() && it != m_existingComponents.begin()) {
                            --it;
                            m_selectedComponent.type = *it;
                            switch (m_selectedComponent.type) {
                            case ComponentType::Transform:   m_selectedComponent.ptr = &m_currentObject->Transform.value(); break;
                            case ComponentType::Picture:     m_selectedComponent.ptr = &m_currentObject->Picture.value(); break;
                            case ComponentType::Textblock:   m_selectedComponent.ptr = &m_currentObject->Textblock.value(); break;
                            case ComponentType::TriggerArea: m_selectedComponent.ptr = &m_currentObject->TriggerArea.value(); break;
                            case ComponentType::Blueprint:   m_selectedComponent.ptr = &m_currentObject->Blueprint.value(); break;
                            }
                            ClearScreen();
                            PrintHelpText();
                            PrintObjectComponents();
                        }
                    }
                    break;

                case KeyCode::S:
                case KeyCode::Down:
                    OutputDebugStringA("[DetailViewer] Branch 2: S/Down (next component)\n");
                    if (m_currentObject && !m_existingComponents.empty()) {
                        auto it = std::find(m_existingComponents.begin(), m_existingComponents.end(), m_selectedComponent.type);
                        if (it != m_existingComponents.end() && it != m_existingComponents.end() - 1) {
                            ++it;
                            m_selectedComponent.type = *it;
                            switch (m_selectedComponent.type) {
                            case ComponentType::Transform:   m_selectedComponent.ptr = &m_currentObject->Transform.value(); break;
                            case ComponentType::Picture:     m_selectedComponent.ptr = &m_currentObject->Picture.value(); break;
                            case ComponentType::Textblock:   m_selectedComponent.ptr = &m_currentObject->Textblock.value(); break;
                            case ComponentType::TriggerArea: m_selectedComponent.ptr = &m_currentObject->TriggerArea.value(); break;
                            case ComponentType::Blueprint:   m_selectedComponent.ptr = &m_currentObject->Blueprint.value(); break;
                            }
                            ClearScreen();
                            PrintHelpText();
                            PrintObjectComponents();
                        }
                    }
                    break;

                case KeyCode::F:
                    OutputDebugStringA("[DetailViewer] Branch F (edit component)\n");
                    if (m_currentObject &&
                        m_selectedComponent.type != ComponentType::None &&
                        !m_existingComponents.empty())
                    {
                        isEditing = true;
                        EditCurrentComponent(); // 内部会设置 isEditing = false
                        // 编辑结束后刷新界面
                        ClearScreen();
                        PrintHelpText();
                        PrintObjectComponents();
                    }
                    break;

                default:
                    break;
                }
            }
        }
        m_inputSystem.clearEvent();
        Sleep(20);
    }
}