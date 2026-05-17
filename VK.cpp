// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#include "VK.h"
unsigned int KeyCodeToVK(KeyCode code) {
    switch (code) {
        // 字母
    case KeyCode::A: return 'A';
    case KeyCode::B: return 'B';
    case KeyCode::C: return 'C';
    case KeyCode::D: return 'D';
    case KeyCode::E: return 'E';
    case KeyCode::F: return 'F';
    case KeyCode::G: return 'G';
    case KeyCode::H: return 'H';
    case KeyCode::I: return 'I';
    case KeyCode::J: return 'J';
    case KeyCode::K: return 'K';
    case KeyCode::L: return 'L';
    case KeyCode::M: return 'M';
    case KeyCode::N: return 'N';
    case KeyCode::O: return 'O';
    case KeyCode::P: return 'P';
    case KeyCode::Q: return 'Q';
    case KeyCode::R: return 'R';
    case KeyCode::S: return 'S';
    case KeyCode::T: return 'T';
    case KeyCode::U: return 'U';
    case KeyCode::V: return 'V';
    case KeyCode::W: return 'W';
    case KeyCode::X: return 'X';
    case KeyCode::Y: return 'Y';
    case KeyCode::Z: return 'Z';

        // 数字（主键盘区）
    case KeyCode::Num0: return '0';
    case KeyCode::Num1: return '1';
    case KeyCode::Num2: return '2';
    case KeyCode::Num3: return '3';
    case KeyCode::Num4: return '4';
    case KeyCode::Num5: return '5';
    case KeyCode::Num6: return '6';
    case KeyCode::Num7: return '7';
    case KeyCode::Num8: return '8';
    case KeyCode::Num9: return '9';

        // 功能键
    case KeyCode::F1:  return VK_F1;
    case KeyCode::F2:  return VK_F2;
    case KeyCode::F3:  return VK_F3;
    case KeyCode::F4:  return VK_F4;
    case KeyCode::F5:  return VK_F5;
    case KeyCode::F6:  return VK_F6;
    case KeyCode::F7:  return VK_F7;
    case KeyCode::F8:  return VK_F8;
    case KeyCode::F9:  return VK_F9;
    case KeyCode::F10: return VK_F10;
    case KeyCode::F11: return VK_F11;
    case KeyCode::F12: return VK_F12;

        // 控制与特殊键
    case KeyCode::Space:      return VK_SPACE;
    case KeyCode::Enter:      return VK_RETURN;
    case KeyCode::Backspace:  return VK_BACK;
    case KeyCode::Tab:        return VK_TAB;
    case KeyCode::Escape:     return VK_ESCAPE;
    case KeyCode::Pause:      return VK_PAUSE;
    case KeyCode::PrintScreen:return VK_SNAPSHOT;   // 注意：PrintScreen 对应 VK_SNAPSHOT

        // 编辑键
    case KeyCode::Insert: return VK_INSERT;
    case KeyCode::Delete: return VK_DELETE;
    case KeyCode::Home:   return VK_HOME;
    case KeyCode::End:    return VK_END;
    case KeyCode::PageUp: return VK_PRIOR;
    case KeyCode::PageDown: return VK_NEXT;

        // 方向键
    case KeyCode::Left:  return VK_LEFT;
    case KeyCode::Right: return VK_RIGHT;
    case KeyCode::Up:    return VK_UP;
    case KeyCode::Down:  return VK_DOWN;

        // 标点符号键（美式键盘布局）
    case KeyCode::Grave:        return VK_OEM_3;      // `~
    case KeyCode::Minus:        return VK_OEM_MINUS;  // -_
    case KeyCode::Equal:        return VK_OEM_PLUS;   // =+
    case KeyCode::LeftBracket:  return VK_OEM_4;      // [{
    case KeyCode::RightBracket: return VK_OEM_6;      // ]}
    case KeyCode::Backslash:    return VK_OEM_5;      // \|

        // 鼠标按键
    case KeyCode::MouseLeft:   return VK_LBUTTON;
    case KeyCode::MouseRight:  return VK_RBUTTON;
    case KeyCode::MouseMiddle: return VK_MBUTTON;

        // 以防万一
    default: return 0;   // 未知键返回 0
    }
}