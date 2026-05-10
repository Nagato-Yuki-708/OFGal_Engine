#pragma once
// 这个头文件用来存储编译之后的蓝图
#include <unordered_map>
#include <vector>
#include "GameVM.h"
#include "SharedTypes.h"

class CompiledBlueprint {
public:
	BlueprintData sourceData;
	std::unordered_map<int, NODE*> nodeMap;
	std::vector<NODE*> entryNodes;

	~CompiledBlueprint() {
		for (auto& pair : nodeMap) {
			delete pair.second;
		}
	}

};