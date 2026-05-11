#include "BlueprintScheduler.h"

double BlueprintScheduler::GetTimeSeconds() {
	using namespace std::chrono;
	auto now = high_resolution_clock::now();
	auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
	return ms / 1000.0;     
}

bool BlueprintScheduler::CanExecute(NODE* node) {      //这个是关键的判断函数
	if (auto* begin = dynamic_cast<BeginPlay_Node*>(node)) {
		if (!begin->hasExecuted) {
			begin->hasExecuted = true;
			return true;
		}
		return false;
	}
	if (auto* timer = dynamic_cast<PlayPerNMsNode*>(node)) {
		double now = GetTimeSeconds();
		double delta = now - timer->lastTriggerTime;
		if (delta >= (timer->intervalMs / 1000.0)) {
			timer->lastTriggerTime = now;
			return true;
		}
		return false;
	}
	if (auto* keyNode = dynamic_cast<PlayWhenKeyNode*>(node)) {
		for (const auto& event : g_inputSystem.getEvents()) {
			if (event.type != InputType::KeyDown) {
				continue;
			}
			if (keyNode->targetKey == event.key) {
				return true;
			}
		}
		return false;
	}
}

void BlueprintScheduler::Tick() {     //这个是调度器执行函数，到时候主循环还要控制一下执行函数的使用。
	for (auto bp : blueprints) {
		if (!bp) continue;
		for (auto* entry : bp->entryNodes) {
			if (!entry) continue;
			entryNodess.push_back(entry);  //将所有的入口节点都放在一个数组里面
		}
	}
	for (auto* entrys : entryNodess) {
		if (CanExecute(entrys)) {
			ExecutionContext ctx;
			ctx.current = entrys;
			RunVM(ctx);
		}
	
	}
}

void BlueprintScheduler::getBlueprint(LevelData* data) {
	if (!data) return;

	// 清空旧蓝图
	blueprints.clear();

	// 遍历场景根对象
	for (auto& [name, obj] : data->objects) {

		if (!obj) continue;

		ScanObject(obj);
	}
}

void BlueprintScheduler::ScanObject(ObjectData* obj) {
	if (!obj) return;

	// =========================
	// 1. 检查当前对象是否有蓝图
	// =========================

	if (obj->Blueprint.has_value()) {

		BlueprintCompiler compiler;

		CompiledBlueprint* compiled = compiler.Compile(ReadBPData(obj->Blueprint->Path));

		if (compiled) {
			for (auto& pair : compiled->nodeMap) {
				pair.second->owner = obj;   // 绑定对象指针
			}
			blueprints.push_back(compiled);
		}
	}


	for (auto& [name, child] : obj->objects) {

		if (!child) continue;

		ScanObject(child);
	}

}