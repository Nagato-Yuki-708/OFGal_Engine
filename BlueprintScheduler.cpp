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
	return false;   //默认的情况下不能够执行
}

void BlueprintScheduler::Tick() {     //这个是调度器执行函数，到时候主循环还要控制一下执行函数的使用。
	std::vector<InputEvent> eventsCopy = m_inputSystem.getEvents();
	for (const auto& ev : eventsCopy) {
		if (ev.key== KeyCode::Escape &&ev.type == InputType::KeyDown) {
			Stop();
			return;
		}
	}
	for (auto bp : blueprints) {
		if (!bp) continue;
	}
	for (auto* entrys : entryNodess) {
		if (!entrys)   //修复了重复加入开始节点的问题。
			continue;
		if (CanExecute(entrys)) {  //已经被执行过的开始节点就不能再次执行。
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

		ScanObject(obj,data);
	}
	for (auto* bp : blueprints) {  //加入开始节点

		if (!bp)
			continue;

		for (auto* entry : bp->entryNodes) {

			if (!entry)
				continue;

			entryNodess.push_back(entry);
			//让执行器能够获得所有的开始节点
		}
	}
}

void BlueprintScheduler::ScanObject(ObjectData* obj,LevelData* data) {
	if (!obj)
		return;

	// 编译当前对象蓝图
	if (obj->Blueprint.has_value()) {

		try {
			// 核心函数: 传入路径，返回布尔值
			if (std::filesystem::exists(obj->Blueprint->Path)) {
			}
			else {
				DEBUG_LOG("[BPSche] 文件或目录不存在: " << obj->Blueprint->Path << "\n");
				return;
			}
		}
		catch (const std::filesystem::filesystem_error& e) {
			DEBUG_LOG("[BPSche] 访问文件系统出错: " << e.what() << "\n");
			return;
		}

		BlueprintCompiler compiler;
		BlueprintData TempBPData = ReadBPData(obj->Blueprint->Path);

		CompiledBlueprint* compiled =
			compiler.Compile(
				TempBPData
			);
		DEBUG_LOG("[BPSche] After Complie \n");
		if (compiled) {

			// 给所有节点绑定 owner, 还有Level
			for (auto& pair : compiled->nodeMap) {
				pair.second->level = data;
				pair.second->owner = obj;
			}

			blueprints.push_back(compiled);

			if (TempBPData.nodes.size() > 0) {
				for (Node& it : TempBPData.nodes) {
					if (it.type != "Play_when_N_push_down")
						continue;
					else {
						for (Pin& pin : it.pins) {
							if (pin.name != "Btn")
								continue;
							else {
								if(pin.literal.has_value() && StdkeysMap.contains(pin.literal.value()))
								{
									if(KeyCodeToVK(StdkeysMap.at(pin.literal.value())))
									{
										int VK = KeyCodeToVK(StdkeysMap.at(pin.literal.value()));
										m_inputCollector.AddBinding({ VK,
											Modifier::None,
											StdkeysMap.at(pin.literal.value()),
											true });
									}
								}
							}
						}
					}
				}
				m_inputCollector.AddBinding({ int(VK_ESCAPE),
											Modifier::None,
											StdkeysMap.at("Escape"),
											true });
			}
		}
	}

	// 递归扫描子对象
	for (auto& [name, child] : obj->objects) {

		if (!child)
			continue;

		ScanObject(child,data);
	}

}



void BlueprintScheduler::Start(LevelData* data) {
	if (!data)
		return;

	m_currentLevel = data;

	m_inputSystem.SetGlobalCapture(true);
	m_inputSystem.SetWindowHandle(GetConsoleWindow());
	//这个地方加入调试信息
	DEBUG_LOG("[BPSche] Enter 1 \n");
	getBlueprint(data);

	isRunning = true;
	DEBUG_LOG("[BPSche] Enter running \n");
	//Tick一定要执行很多次
	while (isRunning) {

		// 更新输入系统
		m_inputCollector.update();
		DEBUG_LOG("[BPSche] Start Running \n");
		// 执行蓝图
		Tick();

		// 清空输入事件
		m_inputSystem.clearEvent();
		DEBUG_LOG(" After Tick \n");
		// 控制帧率
		std::this_thread::sleep_for(
			std::chrono::milliseconds(16)
		);
	}
	m_inputSystem.SetGlobalCapture(false);
}

void BlueprintScheduler::Stop() {

	isRunning = false;
}