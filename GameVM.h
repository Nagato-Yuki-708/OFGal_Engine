// Copyright 2026 MrSeagull. All Rights Reserved.
#pragma once
#include<thread>
#include<iostream>
#include "SharedTypes.h"
#include "_EventBus.h"
#include "SoundEvents.h"
#include"InputCollector.h"
#include "InputSystem.h"
#include <chrono>
#include <unordered_map>

// ============================================================
// 前向声明
// ============================================================
KeyCode StringToKeyCode(const std::string& str);
class NODE;  // ExecutionContext 需要 NODE* 成员

// 执行上下文（必须在 NODE 之前定义，因为 NODE::func_for_VM 需要 ExecutionContext 完整类型）


class ExecutionContext {   //执行引擎,起到一个记录上下文的作用
public:
	NODE* current = nullptr;
	bool running = true;
	bool paused = false;
	NODE* lastExecuted = nullptr;  //在调试的时候使用
	std::unordered_map<std::string, Value> variables;  //蓝图上下文的变量表，GET_VAR/SET_VAR 节点通过这个表读写变量
	ObjectData* selfObject = nullptr;  // 当前执行上下文的对象引用
};




// ============================================================
// NODE 基类
// ============================================================
class NODE { //这是父类
public:
	NODE* lastNode = nullptr;
	NODE* nextNode = nullptr;
	NODE* loopNode = nullptr;    //这里记录了循环的节点
	ObjectData* owner = nullptr;   //所属对象指针（可选，视节点类型而定）

	// ★ 编译注意：所有节点统一签名 void func_for_VM(ExecutionContext& ctx)
	//   RunVM 在调用前将 ctx.current 默认设为 node->nextNode
	//   单出口节点无需修改 ctx.current；分支节点覆盖 ctx.current 即可改变执行流
	virtual void func_for_VM(ExecutionContext& ctx) = 0;
};

// ============================================================
// 运算节点
// ============================================================

class BinaryOpNode :public NODE {  // 蓝图节点类型：ADD / Sub / Mul / Div 的公共基类
public:
	std::vector<Value*>InData;
	std::vector<Value> OutData;
	virtual Value compute(const Value& a, const Value& b) = 0;
	void func_for_VM(ExecutionContext& ctx) override {
		Value a = InData[0] ? *InData[0] : Value();
		Value b = InData[1] ? *InData[1] : Value();
		Value result = compute(a, b);
			OutData[0] = result;
	}
};
class Node_Equal : public BinaryOpNode {    //这个是等号的比较节点
public:
	Value compute(const Value& a, const Value& b);
};
class Node_Greater : public BinaryOpNode {
public:
	Value compute(const Value& a, const Value& b);
};
class Node_Less : public BinaryOpNode {
public:
	Value compute(const Value& a, const Value& b);
};
class Node_ADD :public BinaryOpNode {
public:
	Value compute(const Value& a, const Value& b);
};

class Node_Sub : public BinaryOpNode {  // 蓝图节点类型："Sub"
public:
	Value compute(const Value& a, const Value& b);
};

class Node_Mul :public BinaryOpNode {  // 蓝图节点类型："Mul"
public:
	Value compute(const Value& a, const Value& b);
};

class Node_Div :public BinaryOpNode {  // 蓝图节点类型："Div"
public:
	Value compute(const Value& a, const Value& b);
};

// ============================================================
// 入口 / 退出节点
// ============================================================

class BeginPlay_Node :public NODE {  // 蓝图节点类型："BeginPlay"
public:
	bool hasExecuted = false;

	void func_for_VM(ExecutionContext& ctx) override {
		
	}

};

class PlayPerNMsNode :public NODE {  // 蓝图节点类型："PlayPerNMs" —— 另类的开始节点
public:
	int intervalMs = 0;       //这两值分别代表
	double lastTriggerTime = 0.0;
	void func_for_VM(ExecutionContext& ctx) {
	//入口节点本身不执行逻辑
	}

	//分工要明确！
};

class PlayWhenKeyNode : public NODE {
public:
	KeyCode targetKey = KeyCode::Unknown;
	void func_for_VM(ExecutionContext& ctx) override {

		// 入口节点本身不执行逻辑
	}
};








class Exit : public NODE {  // 蓝图节点类型："Exit"
public:
	void func_for_VM(ExecutionContext& ctx) override {
		ctx.running = false;
	}

};

// ============================================================
// 变换节点
// ============================================================

class SetTransforNode : public NODE {  // 蓝图节点类型："SetTransform"
public:
	ObjectData* obj = nullptr;
	Value* in_loc_x = nullptr;
	Value* in_loc_y = nullptr;
	Value* in_loc_z = nullptr;
	Value* in_rotation = nullptr;
	Value* in_scale_x = nullptr;
	Value* in_scale_y = nullptr;
	// ★ 编译注意：不要在此类中声明 NODE* nextNode，会遮蔽基类成员！已删除
	void func_for_VM(ExecutionContext& ctx);  // 定义在 GameVM.cpp
	// ★ 编译注意：
	//   1. BuildDataLinks 中根据 targetPin 名称绑定到对应字段：
	//      "Location.x"→in_loc_x, "Location.y"→in_loc_y, "Location.z"→in_loc_z 等
	//   2. InitNodeData 中无需额外操作（指针已默认 nullptr，由 BuildDataLinks 绑定）
	//   3. obj 指针需要由编译器在链接阶段设置（当前代码中未实现，需要补充）
};

// ============================================================
// 条件分支 / 循环节点
// ============================================================

class If_Node : public NODE {  // 蓝图节点类型："If"
public:
	Value* condition = nullptr;
	NODE* trueNode = nullptr;
	NODE* falseNode = nullptr;
	void func_for_VM(ExecutionContext& ctx) override {
		bool cond = (condition && condition->type == ValueType::BOOL) ? condition->b : false;
		ctx.current = cond ? trueNode : falseNode;
	}
	// ★ 编译注意：
	//   1. BuildExecLinks 需要处理 sourcePin=="true"→trueBranch, sourcePin=="false"→falseBranch
	//   2. If_Node 不使用基类 nextNode（双出口节点用 trueBranch/falseBranch 代替）
	//   3. BuildDataLinks 需要处理 targetPin=="condition"→condition 指针绑定
};

class While_Node : public NODE {  // 蓝图节点类型："While"
public:
	Value* condition = nullptr;
	NODE* loopBodyNode = nullptr;
	NODE* loopExitNode = nullptr;
	Value iterationCount;
	int _count = 0;
	void func_for_VM(ExecutionContext& ctx) override {
		bool cond = (condition && condition->type == ValueType::BOOL) ? condition->b : false;
		if (!cond) {
			_count = 0;
			iterationCount = Value::makeInt(0);
			// ctx.current 已被 RunVM 默认设为 nextNode（退出循环），无需覆盖
		} else {
			iterationCount = Value::makeInt(_count);
			_count++;
			ctx.current = loopBodyNode;
		}
	}
	
};

class Break_Node : public NODE {  // 蓝图节点类型："Break"
public:
	void func_for_VM(ExecutionContext& ctx) override {
		if (loopNode) {
			auto* whileNode = dynamic_cast<While_Node*>(loopNode);
			if (whileNode) {
				ctx.current = whileNode->loopExitNode;// 跳到循环外，不用改执行流
			}
		}

	}
	// ★ 编译注意：
	//   1. BuildExecLinks 需将此节点的 nextNode → While_Node 的 nextNode（循环外首节点）
	//   2. BuildExecLinks 需将此节点的 loopNode → 所属 While_Node
	//   3. 确定 Break 所属 While 的方法：沿 links 逆推，找到包含此 Break 的 While
};

class Continue_Node : public NODE {  // 蓝图节点类型："Continue"
public:
	void func_for_VM(ExecutionContext& ctx) override {
		ctx.current = loopNode;  // 跳回 While 重新判断条件
	}
	// ★ 编译注意：
	//   1. BuildExecLinks 需将此节点的 loopNode → 所属 While_Node
	//   2. 此节点不使用 nextNode（Continue 走 loopNode，不走 nextNode）
	//   3. 确定 Continue 所属 While 的方法同 Break_Node
};

// ============================================================
// 显示节点
// ============================================================

class PrintText_Node : public NODE {
public:
	Value* text = nullptr;
	ObjectData* obj = nullptr;  // ★ 由编译器绑定

	void func_for_VM(ExecutionContext& ctx) override {
		// 优先使用 ctx.selfObject，其次使用节点绑定的 obj
		ObjectData* targetObj = ctx.selfObject ? ctx.selfObject : obj;

		if (!targetObj) {
			std::cout << "PrintText_Node: No target object\n";
			return;
		}

		// 检查对象是否有文本块组件
		if (!targetObj->Textblock.has_value()) {
			std::cout << "PrintText_Node: Object has no Textblock component\n";
			return;
		}

		// 获取输入文本（支持多类型自动转换）
		std::string textToPrint;
		if (text) {
			switch (text->type) {
			case ValueType::STRING:
				textToPrint = text->s;
				break;
			case ValueType::INT:
				textToPrint = std::to_string(text->i);
				break;
			case ValueType::FLOAT:
				textToPrint = std::to_string(text->f);
				break;
			case ValueType::BOOL:
				textToPrint = text->b ? "true" : "false";
				break;
			default:
				textToPrint = "";
				break;
			}
		}

		// 更新文本块内容
		auto& textblock = targetObj->Textblock.value();
		textblock.Text.component = textToPrint;

		std::cout << "PrintText_Node: Updated text to: " << textToPrint << "\n";
	}
};
// ★ 编译注意：
	//   1. BuildDataLinks 需要处理 targetPin=="text"→text 指针绑定
	//   2. "当前对象" 的绑定方式未定——可能需要额外输入引脚或节点内存储对象引用

// ============================================================
// 音频节点（通过 _EventBus 事件总线与 SoundSystem 通信）
// ============================================================

class PlaySound_Node : public NODE {  // 蓝图节点类型："PlaySound"
public:
	Value* path = nullptr;
	Value* loop = nullptr;
	Value* volume = nullptr;  // ★ 新增：音量参数

	void func_for_VM(ExecutionContext& ctx) override {
		std::string soundPath = (path && path->type == ValueType::STRING) ? path->s : "";
		bool shouldLoop       = (loop && loop->type == ValueType::BOOL)   ? loop->b : false;
		float vol             = (volume && volume->type == ValueType::FLOAT) ? volume->f : 1.0f;

		// 音量限制在 0.0 ~ 1.0 范围
		if (vol < 0.0f) vol = 0.0f;
		if (vol > 1.0f) vol = 1.0f;

		if (!soundPath.empty()) {
			// 通过 _EventBus 发布 PlaySoundEvent，SoundSystem 在构造时已订阅此事件
			// 回调内执行 SoundSystem::playSound(playEvent.path, playEvent.loop)
			// SoundSystem::playSound 使用 Windows MCI API 打开设备并播放
			_EventBus::getInstance().publish_PlaySound(PlaySoundEvent{soundPath, shouldLoop});
			// TODO: 传递音量参数到 SoundSystem
		}
		// 单出口节点，不修改 ctx.current，RunVM 自动走 nextNode
	}
	// ★ 编译注意：BuildDataLinks 需处理 targetPin=="path"/"loop"/"Volume"
};

class PauseSound_Node : public NODE {  // 蓝图节点类型："PauseSound"
public:
	Value* path = nullptr;
	void func_for_VM(ExecutionContext& ctx) override {
		std::string soundPath = (path && path->type == ValueType::STRING) ? path->s : "";

		if (!soundPath.empty()) {
			// 通过 _EventBus 发布 PauseSoundEvent
			// SoundSystem 回调内执行 SoundSystem::pauseSound(playEvent.path)
			// pauseSound 会发送 MCI "pause" 命令，并设置 AudioClip::isPaused = true
			_EventBus::getInstance().publish_PauseSound(PauseSoundEvent{soundPath});
		}
		// 单出口节点，不修改 ctx.current，RunVM 自动走 nextNode
	}
	// ★ 编译注意：BuildDataLinks 需处理 targetPin=="path"
};

// ============================================================
// 蓝图变量上下文
// ============================================================

// 蓝图运行时变量表，由 BlueprintCompiler 在编译阶段从 BlueprintData::variables 初始化
// 使用方式：编译器在 Run() 前创建实例，调用 setCurrent()，执行链中 GET_VAR / SET_VAR 即可访问
class BlueprintContext {
public:
	std::unordered_map<std::string, Value> variables;

	// 根据变量名查找，返回指针（不存在则 nullptr）
	Value* getVariable(const std::string& name) {
		auto it = variables.find(name);
		return (it != variables.end()) ? &it->second : nullptr;
	}

	// 写入变量（不存在则创建，已存在则覆盖）
	void setVariable(const std::string& name, const Value& val) {
		variables[name] = val;  // 拷贝写入
	}

	// 获取当前线程的蓝图上下文（线程局部存储，PlayPerNMsNode 等多线程场景安全）
	static BlueprintContext* current() {
		return currentRef();
	}

	// 设置当前线程的蓝图上下文
	static void setCurrent(BlueprintContext* ctx) {
		currentRef() = ctx;
	}

private:
	// 函数级 thread_local，头文件安全（无需 .cpp 定义）
	static BlueprintContext*& currentRef() {
		thread_local BlueprintContext* s_currentContext = nullptr;
		return s_currentContext;
	}
};

// ============================================================
// 变量节点
// ============================================================
class GET_VAR : public NODE {
public:
	std::string varName;   // 编译期写死
	Value outValue;

	void func_for_VM(ExecutionContext& ctx) override {

		if (varName.empty()) {
			OutputDebugStringA("GET_VAR: empty name\n");
			outValue = Value();
			return;
		}

		auto it = ctx.variables.find(varName);
		if (it != ctx.variables.end()) {
			outValue = it->second;
		}
		else {
			//std::cout << "Variable not found: " << varName << "\n";
			outValue = Value();
		}
	}
};
class SET_VAR : public NODE {
public:
	std::string varName;     // 来自 VarToSet.literal
	Value* inValue = nullptr; // Link 输入
	Value literalValue;      // literal 输入（备用）

	Value outValue;          // VarCopy 输出

	void func_for_VM(ExecutionContext& ctx) override {

		if (varName.empty()) {
			OutputDebugStringA("SET_VAR: empty name\n");
			return;
		}

		Value finalValue;

		// 优先使用数据流
		if (inValue) {
			finalValue = *inValue;
		}
		else {
			finalValue = literalValue;
		}

		ctx.variables[varName] = finalValue;

		// 输出副本
		outValue = finalValue;
	}
};// 渲染相关节点
// ============================================================

class Render_Node : public NODE {  // 蓝图节点类型："Render"
public:
	// —— 数据输入 ——
	LevelData* levelData = nullptr;               // 场景指针，不通过 Value 传递
	Value* samplingMethod = nullptr;              // 0=NEAREST, 1=BILINEAR, 2=BICUBIC, 3=ANISOTROPIC
	Value* msaaMultiple = nullptr;

	// —— 数据输出 ——
	Value outFrame;   // 执行成功后覆盖为 FRAME

	void func_for_VM(ExecutionContext& ctx) override {
		if (!levelData) return;   // 无场景数据，跳过渲染

		// 读取采样方法（默认 BICUBIC）
		int methodVal = (samplingMethod && samplingMethod->type == ValueType::INT)
			? samplingMethod->i : 2;

		// 读取 MSAA 倍数（默认 1x）
		int msaaVal = (msaaMultiple && msaaMultiple->type == ValueType::INT)
			? msaaMultiple->i : 1;

		// 整数映射为枚举（防止非法值）
		TextureSamplingMethod method = SAMPLING_BICUBIC;
		switch (methodVal) {
			case 0: method = SAMPLING_NEAREST;    break;
			case 1: method = SAMPLING_BILINEAR;   break;
			case 2: method = SAMPLING_BICUBIC;    break;
			case 3: method = SAMPLING_ANISOTROPIC; break;
			default: method = SAMPLING_BICUBIC;   break;
		}

		// 通过 _EventBus 调用渲染系统

		Frame f;
		if(method != SAMPLING_ANISOTROPIC){
			f = _EventBus::getInstance().publish_Render_A_Frame(
				*levelData, method, msaaVal);
		}
			
		else{
			f = _EventBus::getInstance().publish_Render_A_Frame_ANISOTROPIC(
				*levelData, SAMPLING_BICUBIC, msaaVal);
		}
		
		outFrame = Value::makeFrame(f);

		// 单出口节点，不修改 ctx.current，RunVM 自动走 nextNode
	}
	// ★ 编译注意：
	//   1. BuildDataLinks 需处理 targetPin=="samplingMethod"/"msaaMultiple"
	//   2. levelData 是 LevelData* 直接指针，编译器需要一种方式从 BlueprintData 获取或绑定
	//   3. InitNodeData 无需额外操作（outFrame 是值类型，默认构造即可）
};

class FrameProcess_Node : public NODE {  // 蓝图节点类型："FrameProcess"
public:
	Value* processName = nullptr;
	Value* inFrame = nullptr;
	Value* processParams = nullptr;

	struct BloomParams {
		float threshold = 220.0f;
		float intensity = 0.8f;
		int blurRadius = 4;
		float sigma = -1.0f;
	};

	struct BlurParams {
		int radius = 3;
		float sigma = -1.0f;
		int direction = 0;
	};

	struct FXAAParams {
		float edgeThreshold = 0.166f;
		float edgeThresholdMin = 0.05f;
		float spanMax = 8.0f;
		float reduceMul = 0.125f;
		float reduceMin = 0.0078125f;
	};

	struct SMAAParams {
		float edgeThreshold = 0.05f;
		int maxSearchSteps = 4;
		bool enableDiag = true;
	};

	struct LensDistortionParams {
		float strength = 0.0f;
		float centerX = 0.5f;
		float centerY = 0.5f;
	};

	struct ChromaticAberrationParams {
		float strength = 2.0f;
		int mode = 0;
		float centerX = 0.5f;
		float centerY = 0.5f;
	};

	struct SharpenParams {
		float strength = 0.5f;
		int radius = 2;
		float sigma = -1.0f;
	};

	struct FilmGrainParams {
		float intensity = 0.05f;
		int grainSize = 1;
		bool dynamic = true;
		int frameId = 0;
	};

	struct VignetteParams {
		float intensity = 0.3f;
		float innerRadius = 0.6f;
		float outerRadius = 1.0f;
		float centerX = 0.5f;
		float centerY = 0.5f;
		float exponent = 1.0f;
	};

	struct ColorCorrectionParams {
		float brightness = 0.0f;
		float contrast = 1.0f;
		float saturation = 1.0f;
		float3 whiteBalance = { 1.0f, 1.0f, 1.0f };
		float hueShift = 0.0f;
	};

	struct ColorGradingParams {
		int style = 0;
		float intensity = 0.8f;
		float3 customColor = { 1.0f, 1.0f, 1.0f };
	};

	// 预设参数结构体
	BloomParams bloom;
	BlurParams blur;
	FXAAParams fxaa;
	SMAAParams smaa;
	LensDistortionParams lensDistortion;
	ChromaticAberrationParams chromaticAberration;
	SharpenParams sharpen;
	FilmGrainParams filmGrain;
	VignetteParams vignette;
	ColorCorrectionParams colorCorrection;
	ColorGradingParams colorGrading;

	Value outFrame;

	void func_for_VM(ExecutionContext& ctx) override {
		// 1. 检查输入帧有效性
		if (!inFrame || inFrame->type != ValueType::FRAME) {
			outFrame = inFrame ? *inFrame : Value();
			return;
		}

		// 2. 拷贝帧数据
		Frame f = inFrame->frame;
		outFrame = Value::makeFrame(f);

		// 3. 读取后处理名称
		std::string name = (processName && processName->type == ValueType::STRING)
			? processName->s : "";

		if (name == "Bloom") {
			_EventBus::getInstance().publish_applyBloom(
				outFrame.frame, bloom.threshold, bloom.intensity,
				bloom.blurRadius, bloom.sigma);
		}
		else if (name == "Blur") {
			_EventBus::getInstance().publish_applyBlur(
				outFrame.frame, blur.radius, blur.sigma, blur.direction);
		}
		else if (name == "FXAA") {
			_EventBus::getInstance().publish_applyFXAA(
				outFrame.frame, fxaa.edgeThreshold, fxaa.edgeThresholdMin,
				fxaa.spanMax, fxaa.reduceMul, fxaa.reduceMin);
		}
		else if (name == "SMAA") {
			_EventBus::getInstance().publish_applySMAA(
				outFrame.frame, smaa.edgeThreshold, smaa.maxSearchSteps, smaa.enableDiag);
		}
		else if (name == "LensDistortion") {
			_EventBus::getInstance().publish_applyLensDistortion(
				outFrame.frame, lensDistortion.strength,
				lensDistortion.centerX, lensDistortion.centerY);
		}
		else if (name == "ChromaticAberration") {
			_EventBus::getInstance().publish_applyChromaticAberration(
				outFrame.frame, chromaticAberration.strength, chromaticAberration.mode,
				chromaticAberration.centerX, chromaticAberration.centerY);
		}
		else if (name == "Sharpen") {
			_EventBus::getInstance().publish_applySharpen(
				outFrame.frame, sharpen.strength, sharpen.radius, sharpen.sigma);
		}
		else if (name == "FilmGrain") {
			_EventBus::getInstance().publish_applyFilmGrain(
				outFrame.frame, filmGrain.intensity, filmGrain.grainSize,
				filmGrain.dynamic, filmGrain.frameId);
		}
		else if (name == "Vignette") {
			_EventBus::getInstance().publish_applyVignette(
				outFrame.frame, vignette.intensity, vignette.innerRadius,
				vignette.outerRadius, vignette.centerX, vignette.centerY,
				vignette.exponent);
		}
		else if (name == "ColorCorrection") {
			_EventBus::getInstance().publish_applyColorCorrection(
				outFrame.frame, colorCorrection.brightness, colorCorrection.contrast,
				colorCorrection.saturation, colorCorrection.whiteBalance,
				colorCorrection.hueShift);
		}
		else if (name == "ColorGrading") {
			_EventBus::getInstance().publish_applyColorGrading(
				outFrame.frame, colorGrading.style, colorGrading.intensity,
				colorGrading.customColor);
		}
		// 未知名称 → 透传原始帧（outFrame 已是 inFrame 的拷贝，无需额外操作）

		// 单出口节点，不修改 ctx.current，RunVM 自动走 nextNode
	}
	// ★ 编译注意：BuildDataLinks 需处理 targetPin=="processName"/"inFrame"/"processParams"
};

class ShowtheFrame_Node : public NODE {  // 蓝图节点类型："ShowtheFrame"
public:
	Value* inFrame = nullptr;
	void func_for_VM(ExecutionContext& ctx) override {
		if (inFrame && inFrame->type == ValueType::FRAME) {
			_EventBus::getInstance().publish_Print_A_Frame(inFrame->frame);
		}
		// 单出口节点，不修改 ctx.current，RunVM 自动走 nextNode
	}
	// ★ 编译注意：BuildDataLinks 需处理 targetPin=="inFrame"
};

// 执行引擎，还有相关的节点



inline void RunVM(ExecutionContext& ctx) {
	while (ctx.current && ctx.running) {
		NODE* node = ctx.current;
		ctx.current = node->nextNode;   //一定要先赋值再进行执行，否则他的值可能会被覆盖

		node->func_for_VM(ctx);
		if (!node->nextNode) {
			ctx.running = false;
		}
		ctx.lastExecuted = node;

		//Pause  等待

		if (ctx.paused) {
			return;
		}
	}
}
inline double GetTimeSeconds() {  //计算时间函数
	using namespace std::chrono;
	static auto start = high_resolution_clock::now();  //让时间点只会初始化一次
	auto now = high_resolution_clock::now();
	return duration<double>(now - start).count();  //这里是要进行时间单位的转换
}
//头文件里在类体外定义方法必须加 inline，否则多个.cpp include 同一个.h 时会报"多重定义"链接错误。