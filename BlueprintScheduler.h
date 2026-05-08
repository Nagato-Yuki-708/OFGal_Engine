#pragma once
#include <vector>
#include <memory>
#include "GameVM.h"
#include "BlueprintCompiler.h"

class BlueprintScheduler {
public:

	struct VMInstance {
		int id = 0;
		CompiledBlueprint* blueprint = nullptr;
		ExecutionContext context;
		bool finished = false;  //记录是否完成运行
	};
public:
	std::vector<std::shared_ptr<VMInstance>> runningVMs;  //能够存储VMInstance的指针
	int nextVMID = 1;
public:
	void StartBlueprint(CompiledBlueprint* blueprint);
	void Tick();
	void ResumeVM(std::shared_ptr<VMInstance> vm);
};
