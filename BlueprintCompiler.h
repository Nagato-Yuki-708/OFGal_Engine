#pragma once
#include <vector>
#include <unordered_map>
#include"GameVM.h"
#include"SharedTypes.h"
#include "CompiledBlueprint.h"

class BlueprintCompiler {
public:
	BlueprintData currentBlueprint;   //能够直接获得蓝图的原始数据
	CompiledBlueprint* Compile(const BlueprintData& data);
private:
	CompiledBlueprint* currentCompiled = nullptr;
	std::unordered_map<NODE*, While_Node*>nodeTowhile;    //用来进行循环的绑定


	NODE* CreateNode(const Node& n);
	void BuildExecLinks(const BlueprintData& data);
	void BuildDataLinks(const  BlueprintData& data);
	void InitNodeData(const  BlueprintData& data);

};



static std::unordered_map<std::string, KeyCode> keyMap = {

	{"A", KeyCode::A},
	{"B", KeyCode::B},
	{"C", KeyCode::C},
	{"D", KeyCode::D},
	{"E", KeyCode::E},
	{"F", KeyCode::F},
	{"G", KeyCode::G},
	{"H", KeyCode::H},
	{"I", KeyCode::I},
	{"J", KeyCode::J},
	{"K", KeyCode::K},
	{"L", KeyCode::L},
	{"M", KeyCode::M},
	{"N", KeyCode::N},
	{"O", KeyCode::O},
	{"P", KeyCode::P},
	{"Q", KeyCode::Q},
	{"R", KeyCode::R},
	{"S", KeyCode::S},
	{"T", KeyCode::T},
	{"U", KeyCode::U},
	{"V", KeyCode::V},
	{"W", KeyCode::W},
	{"X", KeyCode::X},
	{"Y", KeyCode::Y},
	{"Z", KeyCode::Z},

	{"0", KeyCode::Num0},
	{"1", KeyCode::Num1},
	{"2", KeyCode::Num2},
	{"3", KeyCode::Num3},
	{"4", KeyCode::Num4},
	{"5", KeyCode::Num5},
	{"6", KeyCode::Num6},
	{"7", KeyCode::Num7},
	{"8", KeyCode::Num8},
	{"9", KeyCode::Num9},

	{"F1", KeyCode::F1},
	{"F2", KeyCode::F2},
	{"F3", KeyCode::F3},
	{"F4", KeyCode::F4},
	{"F5", KeyCode::F5},
	{"F6", KeyCode::F6},
	{"F7", KeyCode::F7},
	{"F8", KeyCode::F8},
	{"F9", KeyCode::F9},
	{"F10", KeyCode::F10},
	{"F11", KeyCode::F11},
	{"F12", KeyCode::F12},

	{"Space", KeyCode::Space},
	{"Enter", KeyCode::Enter},
	{"Escape", KeyCode::Escape},

	{"Left", KeyCode::Left},
	{"Right", KeyCode::Right},
	{"Up", KeyCode::Up},
	{"Down", KeyCode::Down},

	{"MouseLeft", KeyCode::MouseLeft},
	{"MouseRight", KeyCode::MouseRight},
	{"MouseMiddle", KeyCode::MouseMiddle}
};

inline KeyCode StringToKeyCode(const std::string& str)
{
	auto it = keyMap.find(str);

	if (it != keyMap.end()) {
		return it->second;
	}

	return KeyCode::Unknown;
}
