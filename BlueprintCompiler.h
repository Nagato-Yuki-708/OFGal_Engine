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
	void Run();
private:
	CompiledBlueprint* currentCompiled = nullptr;
	std::unordered_map<NODE*, While_Node*>nodeTowhile;    //用来进行循环的绑定


	NODE* CreateNode(const Node& n);
	void BuildExecLinks(const BlueprintData& data);
	void BuildDataLinks(const  BlueprintData& data);
	void InitNodeData(const  BlueprintData& data);

};
