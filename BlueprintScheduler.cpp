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
		for (const auto& event : m_inputSystem.getEvents()) {
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

	entryNodess.clear(); //开始节点也要清空
	// 遍历场景根对象
	for (auto& [name, obj] : data->objects) {

		if (!obj) continue;

		ScanObject(obj);
	}
	for (auto* bp : blueprints) {  //加入开始节点

		if (!bp)
			continue;

		for (auto* entry : bp->entryNodes) {

			if (!entry)
				continue;

			entryNodess.push_back(entry);
		}
	}
}

void BlueprintScheduler::ScanObject(ObjectData* obj) {
	if (!obj)
		return;

	// 编译当前对象蓝图
	if (obj->Blueprint.has_value()) {

		BlueprintCompiler compiler;

		CompiledBlueprint* compiled =
			compiler.Compile(
				ReadBPData(obj->Blueprint->Path)
			);

		if (compiled) {

			// 给所有节点绑定 owner
			for (auto& pair : compiled->nodeMap) {

				pair.second->owner = obj;
			}

			blueprints.push_back(compiled);
		}
	}

	// 递归扫描子对象
	for (auto& [name, child] : obj->objects) {

		if (!child)
			continue;

		ScanObject(child);
	}

}



void BlueprintScheduler::Start(LevelData* data) {
	if (!data)
		return;

	m_currentLevel = data;

	m_inputSystem.SetGlobalCapture(false);		// 后续改为true
	m_inputSystem.SetWindowHandle(GetConsoleWindow());

	getBlueprint(data);

	isRunning = true;

	//Tick一定要执行很多次
	while (isRunning) {

		// 更新输入系统
		m_inputCollector.update();

		// 执行蓝图
		Tick();

		// 清空输入事件
		m_inputSystem.clearEvent();

		// 控制帧率
		std::this_thread::sleep_for(
			std::chrono::milliseconds(16)
		);
	}
}

void BlueprintScheduler::Stop() {

	isRunning = false;
}