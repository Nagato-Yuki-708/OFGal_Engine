#include "InputCollector.h"
#include "InputEvent.h"

InputCollector::InputCollector(InputSystem* system) : inputsystem(system) {}

void InputCollector::AddBinding(const KeyBinding& binding) {
    m_bindings.push_back(binding);
}

bool InputCollector::GetKeyState(int vk) const {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool InputCollector::GetPrevKeyState(int vk) const {
    auto it = m_keyStates.find(vk);
    return (it != m_keyStates.end()) ? it->second : false;
}

void InputCollector::SetKeyState(int vk, bool down) {
    m_keyStates[vk] = down;
}

bool InputCollector::CheckModifiers(Modifier required) const {
    if (required == Modifier::None)
        return true;

    bool ctrlOk = !(static_cast<int>(required) & static_cast<int>(Modifier::Ctrl)) || GetKeyState(VK_CONTROL);
    bool shiftOk = !(static_cast<int>(required) & static_cast<int>(Modifier::Shift)) || GetKeyState(VK_SHIFT);
    bool altOk = !(static_cast<int>(required) & static_cast<int>(Modifier::Alt)) || GetKeyState(VK_MENU);
    return ctrlOk && shiftOk && altOk;
}

void InputCollector::update() {
    for (const auto& binding : m_bindings) {
        bool currentDown = GetKeyState(binding.vk);
        bool prevDown = GetPrevKeyState(binding.vk);
        bool modifiersMatch = CheckModifiers(binding.modifiers);

        if (binding.edgeOnly) {
            // 仅当按键从 up→down 且修饰键满足时触发一次
            if (currentDown && !prevDown && modifiersMatch) {
                InputEvent event{};
                event.key = binding.eventCode;
                event.type = InputType::KeyDown;
                inputsystem->pushEvent(event);
            }
        }
        else {
            // 状态变化时生成对应事件（要求修饰键满足）
            if (currentDown != prevDown && modifiersMatch) {
                InputEvent event{};
                event.key = binding.eventCode;
                event.type = currentDown ? InputType::KeyDown : InputType::KeyUp;
                inputsystem->pushEvent(event);
            }
        }

        // 无论是否触发事件，都要更新状态记录
        SetKeyState(binding.vk, currentDown);
    }
}