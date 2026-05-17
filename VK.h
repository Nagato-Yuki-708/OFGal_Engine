// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.

#pragma once
#include <windows.h>   // 提供 VK_xxx 宏
#include "SharedTypes.h"

// 全局函数：KeyCode → Windows 虚拟键码
unsigned int KeyCodeToVK(KeyCode code);