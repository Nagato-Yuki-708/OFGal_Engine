#pragma once
#include <vector>
#include <memory>
#include "GameVM.h"
#include "BlueprintCompiler.h"

class BlueprintScheduler {
public:

	struct VMInstance {
		int id = 0;
		BlueprintCompiler* compiler = nullptr;
		ExecutionContext context;
		bool finished = false;
	};
public:
	std::vector<std::shared_ptr<VMInstance>> runningVMs;  //ÄÜ¹»´æ´¢VMInstanceµÄÖ¸Õë
	int nextVMID = 1;
public:
	void StartBlueprint(BlueprintCompiler* compoiler);
	void Tick();
	void ResumeVM(std::shared_ptr<VMInstance> vm);
};
