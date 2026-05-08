#include "BlueprintScheduler.h"

void BlueprintScheduler::StartBlueprint(CompiledBlueprint* blueprint) {
	if (!blueprint)
		return;
	for (auto* entry : blueprint->entryNodes) {
		auto vm = std::make_shared<VMInstance>();
		vm->id = nextVMID++;
		vm->blueprint = blueprint;
		vm->context.vmID = vm->id;
		for (auto& var : blueprint->sourceData.variables) {
			Value v;
			if (var.type == "int") {
				v = Value::makeInt(std::stoi(var.value));
			}
			else if (var.type == "float") {
				v = Value::makeFloat(std::stof(var.value));
			}
			else if (var.type == "bool") {
				v = Value::makeBool(var.value == "true");
			}
			else if (var.type == "string") {
				v = Value::makeString(var.value);
			}
			vm->context.variables[var.name] = v;
		}
		RunVM(vm->context);
		//只有在运行，还有暂停中的VM才能够进入调度器
		if (vm->context.running || vm->context.paused) {
			runningVMs.push_back(vm);
		}
	}
}

void BlueprintScheduler::Tick() {
	double now = GetTimeSeconds();
	for (auto& vm : runningVMs) {
		if (vm->finished)
			continue;
		if (!vm->context.running) {
			vm->finished = true;
			continue;   
			//如果运行完成，那么修改他的状态，让他下次无法运行
		}
		//Delay的恢复机制
		if (vm->context.paused) {
			if (now >= vm->context.wakeUpTime) {
				vm->context.paused = false;
				ResumeVM(vm);
			}
		}
	}
	runningVMs.erase(
		std::remove_if(
			runningVMs.begin(),
			runningVMs.end(),
			[](const std::shared_ptr<VMInstance>& vm) {
				return vm->finished;
			}),
		runningVMs.end()
	);

}

void BlueprintScheduler::ResumeVM(std::shared_ptr<VMInstance> vm) { 
	//对整个函数进行执行
	if (!vm || vm->finished)
		return;
	RunVM(vm->context);
	if (!vm->context.running) {
		vm->finished = true;
	}
}