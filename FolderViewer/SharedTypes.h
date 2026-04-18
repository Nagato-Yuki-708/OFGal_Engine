// Copyright 2026 MrSeagull. All Rights Reserved.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>

struct FolderStructure {
    std::string SelfName;
    std::vector<std::string> Files;
    std::map<std::string, std::unique_ptr<FolderStructure>> SubFolders;
};

struct ProjectStructure {
    std::string RootDirectory;
    FolderStructure Self;
};

struct TreeDisplayInfo {
    int width;   // 纯文本宽度（不含ANSI转义）
    int height;  // 总行数
};

struct SelectedFolderInfo {
    std::string absolutePath;
    int lineNumber;
};

enum class Modifier {
    None = 0,
    Ctrl = 1 << 0,
    Shift = 1 << 1,
    Alt = 1 << 2
};

inline Modifier operator|(Modifier a, Modifier b) {
    return static_cast<Modifier>(static_cast<int>(a) | static_cast<int>(b));
}

struct KeyBinding {
    int vk;                 // 虚拟键码 (如 'N', VK_DELETE)
    Modifier modifiers;     // 需要的修饰键
    KeyCode eventCode;      // 触发时生成的事件码
    bool edgeOnly;          // true = 仅按下瞬间触发一次；false = 按下/释放均触发
};

struct YesNoDialogData {
    WCHAR message[300];   // 使用 WCHAR (UTF-16)
    BOOL  result;
};