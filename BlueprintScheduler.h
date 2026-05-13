#pragma once
#include <vector>
#include <memory>
#include <chrono>
#include "GameVM.h"
#include "BlueprintCompiler.h"
#include "InputCollector.h"
#include "InputSystem.h"
#include "WindowsSystem.h"
#include "Json_BPData_ReadWrite.h"

class BlueprintScheduler {
public:
	std::vector<CompiledBlueprint*> blueprints;  //存储所有编译之后的蓝图
	std::vector<NODE*> entryNodess;  //存储所有的入口节点
	void Tick();    //逐帧调用，进行轮询
	void getBlueprint(LevelData* data);    //用来加入蓝图
	void Start(LevelData* data);        //预想中的一键启动函数
	void Stop();
private:
	InputSystem     m_inputSystem;
	InputCollector  m_inputCollector = InputCollector(&m_inputSystem);

	LevelData* m_currentLevel = nullptr;

	bool isRunning = false;  //用于判断运行的状态
	bool CanExecute(NODE* node);     //能够判断一个节点是否满足执行的条件
	double GetTimeSeconds();    //获得当前的时间
	void ScanObject(ObjectData* obj);  //用来扫描所有的子对象
};