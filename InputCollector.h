#pragma once
#include "Windows.h"
#include "InputSystem.h"
#include <vector>
#include <unordered_map>
#include "SharedTypes.h"

class InputCollector {
public:
    InputCollector(InputSystem* system);
    void update();

    // 注册一个按键绑定
    void AddBinding(const KeyBinding& binding);
private:
    InputSystem* inputsystem;

    std::vector<KeyBinding> m_bindings;
    std::unordered_map<int, bool> m_keyStates;   // 记录各虚拟键前一帧状态

    bool GetKeyState(int vk) const;
    bool GetPrevKeyState(int vk) const;
    void SetKeyState(int vk, bool down);

    bool CheckModifiers(Modifier required) const;
};
