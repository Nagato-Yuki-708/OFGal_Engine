#include"BlueprintCompiler.h"
#include<iostream>

CompiledBlueprint* BlueprintCompiler::Compile(const BlueprintData& data){
	currentCompiled = new CompiledBlueprint();
	currentCompiled->sourceData = data;
	//这是蓝图编辑器的核心函数，能够调用其他的组件函数
	for (auto& n : data.nodes) {
		NODE* node = CreateNode(n);
		if (node) {
			currentCompiled->nodeMap[n.id] = node;
		}
	}
	InitNodeData(data);
	BuildExecLinks(data);
	BuildDataLinks(data);
	for (auto& n : data.nodes) {
		if (n.type == "BeginPlay" || n.type == "Play_per_N_ms" || n.type == "Play_when_N_push_down") {
			currentCompiled->entryNodes.push_back(currentCompiled->nodeMap[n.id]);
		}
	}
	return currentCompiled;
}

NODE* BlueprintCompiler::CreateNode(const Node& n) {  //这个函数负责创建各个节点
	if (n.type == "ADD")return new Node_ADD();
	if (n.type == "Sub") return new Node_Sub();
	if (n.type == "Mul")return new Node_Mul();
	if (n.type == "Div")return new Node_Div();
	if (n.type == "BeginPlay") return new BeginPlay_Node();
	if (n.type == "Exit") return new Exit();

	if (n.type == "SetTransform") {
		return new SetTransforNode();
	}

	// ★ 以下是条件分支 / 循环 / 显示 / 变量类节点
	if (n.type == "If")            return new If_Node();
	if (n.type == "While")         return new While_Node();
	if (n.type == "Break")         return new Break_Node();
	if (n.type == "Continue")      return new Continue_Node();
	if (n.type == "PrintText")     return new PrintText_Node();
	if (n.type == "Render")        return new Render_Node();
	if (n.type == "FrameProcess")  return new FrameProcess_Node();
	if (n.type == "ShowtheFrame")  return new ShowtheFrame_Node();
	if (n.type == "PlaySound")     return new PlaySound_Node();
	if (n.type == "PauseSound")    return new PauseSound_Node();
	if (n.type == "GET_VAR")       return new GET_VAR();
	if (n.type == "SET_VAR")       return new SET_VAR();

	//std::cout << "Unknown node: " << n.type << "\n";
	DEBUG_LOG("Unknown node: " << n.type << "\n");
	return nullptr;
}

void BlueprintCompiler::BuildExecLinks(const BlueprintData& data) {
	for (auto& link : data.links) {
		NODE* A = currentCompiled->nodeMap[link.sourceNode];
		NODE* B = currentCompiled->nodeMap[link.targetNode];
		if (!A || !B) continue;
		if (link.sourcePin == "exec" || link.sourcePin == "then") {  //这里进行基础的链接
			A->nextNode = B;
			B->lastNode = A;
		}
		auto* whileNode = dynamic_cast<While_Node*>(A);
		if (whileNode && link.sourcePin == "OEXEC_Loop") {  //这里能够进行传导，将每一个节点都带上循环的属性
			nodeTowhile[B] = whileNode;
		}
		bool updated = true;
		while (updated) {
			updated = false;
			for (auto& link : data.links) {
				NODE* A = currentCompiled->nodeMap[link.sourceNode];
				NODE* B = currentCompiled->nodeMap[link.targetNode];
				if (nodeTowhile.count(A) && !nodeTowhile.count(B)) {
					nodeTowhile[B] = nodeTowhile[A];
					updated = true;
				}
			}
		}
		for (auto& pair : nodeTowhile) {
			NODE* node = pair.first;
			While_Node* whileNode = pair.second;
			if (auto* br = dynamic_cast<Break_Node*> (node)) {
				br->loopNode = whileNode;
			}
			if (auto* cont = dynamic_cast<Continue_Node*>(node)) {
				cont->loopNode = whileNode;
			}
		}

		auto* branch = dynamic_cast<If_Node*>(A);
		if (branch) {
			if (link.sourcePin == "OEXEC_A") {
				branch->trueNode = B;
			}
			if (link.sourcePin == "OEXEC_B") {
				branch->falseNode = B;
			}
		}
		if (whileNode) {
			if (link.sourcePin == "OEXEC_Loop") {
				whileNode->loopBodyNode = B;
			}
			else if (link.sourcePin == "OEXEC") {
				whileNode->loopExitNode = B;
			}
		}
	}
}



void BlueprintCompiler::InitNodeData(const BlueprintData& data) {
	for (auto& n : data.nodes) {
		NODE* node = currentCompiled->nodeMap[n.id];
		if (auto* bin = dynamic_cast<BinaryOpNode*>(node)) {  //如果是运算节点，那么就给他们的输入输出数据分配空间
			bin->InData.resize(2);
			bin->OutData.resize(1);
			for (int i = 0; i < 2; i++) {
				bin->InData[i] = nullptr;
			}
		}
		if (auto* st = dynamic_cast<SetTransforNode*>(node)) {

		}
		if (auto* get = dynamic_cast<GET_VAR*>(node)) {
			for (auto& pin : n.pins) {
				if (pin.name == "VarToGet" && pin.literal.has_value()) {
					get->varName = pin.literal.value();  // 获取该字面量的值
				}
			}
		}

		if (auto* set = dynamic_cast<SET_VAR*>(node)) {

			for (auto& pin : n.pins) {

				// 变量名（必须 literal）
				if (pin.name == "VarToSet" && pin.literal.has_value()) {
					set->varName = pin.literal.value();
				}

				// NewValue literal（如果存在）
				if (pin.name == "NewValue" && pin.literal.has_value()) {

					const std::string& val = pin.literal.value();

					// 根据 type 转 Value
					if (pin.type == "int") {
						set->literalValue = Value::makeInt(std::stoi(val));
					}
					else if (pin.type == "float") {
						set->literalValue = Value::makeFloat(std::stof(val));
					}
					else if (pin.type == "bool") {
						set->literalValue = Value::makeBool(val == "true");
					}
					else if (pin.type == "string") {
						set->literalValue = Value::makeString(val);
					}
				}
			}
		}

		// =========================
		// PrintText
		// =========================
		if (auto* printText = dynamic_cast<PrintText_Node*>(node)) {
			for (auto& pin : n.pins) {
				if (pin.name == "Text" && pin.literal.has_value()) {
					printText->text = new Value();
					*printText->text = Value::makeString(pin.literal.value());
				}
			}
		}

		// =========================
		// Render
		// =========================
		if (auto* render = dynamic_cast<Render_Node*>(node)) {
			for (auto& pin : n.pins) {
				if (pin.name == "samplingMethod" && pin.literal.has_value()) {
					render->samplingMethod = new Value();
					*render->samplingMethod = Value::makeInt(std::stoi(pin.literal.value()));
				}
				if (pin.name == "msaaMultiple" && pin.literal.has_value()) {
					render->msaaMultiple = new Value();
					*render->msaaMultiple = Value::makeInt(std::stoi(pin.literal.value()));
				}
			}
		}

		// =========================
		// FrameProcess - 解析用户自定义参数
		// =========================
		if (auto* frameProc = dynamic_cast<FrameProcess_Node*>(node)) {
			// 解析引脚字面量
			for (auto& pin : n.pins) {
				if (pin.name == "ProcessOp" && pin.literal.has_value()) {
					frameProc->processName = new Value();
					*frameProc->processName = Value::makeString(pin.literal.value());
				}
			}
			// 解析 properties 中的后处理参数
			for (auto& prop : n.properties) {
				// Bloom: "threshold,intensity,blurRadius,sigma"
				if (prop.first == "Bloom") {
					sscanf_s(prop.second.c_str(), "%f,%f,%d,%f",
						&frameProc->bloom.threshold,
						&frameProc->bloom.intensity,
						&frameProc->bloom.blurRadius,
						&frameProc->bloom.sigma);
				}
				// Blur: "radius,sigma,direction"
				if (prop.first == "Blur") {
					float radius_f;
					sscanf_s(prop.second.c_str(), "%f,%f,%d",
						&radius_f,
						&frameProc->blur.sigma,
						&frameProc->blur.direction);
					frameProc->blur.radius = (int)radius_f;
				}
				// FXAA: "edgeThreshold,edgeThresholdMin,spanMax,reduceMul,reduceMin"
				if (prop.first == "FXAA") {
					sscanf_s(prop.second.c_str(), "%f,%f,%f,%f,%f",
						&frameProc->fxaa.edgeThreshold,
						&frameProc->fxaa.edgeThresholdMin,
						&frameProc->fxaa.spanMax,
						&frameProc->fxaa.reduceMul,
						&frameProc->fxaa.reduceMin);
				}
				// SMAA: "edgeThreshold,maxSearchSteps,enableDiag"
				if (prop.first == "SMAA") {
					int enableDiagInt;
					sscanf_s(prop.second.c_str(), "%f,%d,%d",
						&frameProc->smaa.edgeThreshold,
						&frameProc->smaa.maxSearchSteps,
						&enableDiagInt);
					frameProc->smaa.enableDiag = (enableDiagInt != 0);
				}
				// LensDistortion: "strength,centerX,centerY"
				if (prop.first == "LensDistortion") {
					sscanf_s(prop.second.c_str(), "%f,%f,%f",
						&frameProc->lensDistortion.strength,
						&frameProc->lensDistortion.centerX,
						&frameProc->lensDistortion.centerY);
				}
				// ChromaticAberration: "strength,mode,centerX,centerY"
				if (prop.first == "ChromaticAberration") {
					sscanf_s(prop.second.c_str(), "%f,%d,%f,%f",
						&frameProc->chromaticAberration.strength,
						&frameProc->chromaticAberration.mode,
						&frameProc->chromaticAberration.centerX,
						&frameProc->chromaticAberration.centerY);
				}
				// Sharpen: "strength,radius,sigma"
				if (prop.first == "Sharpen") {
					float radius_f;
					sscanf_s(prop.second.c_str(), "%f,%f,%f",
						&frameProc->sharpen.strength,
						&radius_f,
						&frameProc->sharpen.sigma);
					frameProc->sharpen.radius = (int)radius_f;
				}
				// FilmGrain: "intensity,grainSize,dynamic,frameId"
				if (prop.first == "FilmGrain") {
					int dynamicInt;
					sscanf_s(prop.second.c_str(), "%f,%d,%d,%d",
						&frameProc->filmGrain.intensity,
						&frameProc->filmGrain.grainSize,
						&dynamicInt,
						&frameProc->filmGrain.frameId);
					frameProc->filmGrain.dynamic = (dynamicInt != 0);
				}
				// Vignette: "intensity,innerRadius,outerRadius,centerX,centerY,exponent"
				if (prop.first == "Vignette") {
					sscanf_s(prop.second.c_str(), "%f,%f,%f,%f,%f,%f",
						&frameProc->vignette.intensity,
						&frameProc->vignette.innerRadius,
						&frameProc->vignette.outerRadius,
						&frameProc->vignette.centerX,
						&frameProc->vignette.centerY,
						&frameProc->vignette.exponent);
				}
				// ColorCorrection: "brightness,contrast,saturation,whiteBalance.x,whiteBalance.y,whiteBalance.z,hueShift"
				if (prop.first == "ColorCorrection") {
					sscanf_s(prop.second.c_str(), "%f,%f,%f,%f,%f,%f,%f",
						&frameProc->colorCorrection.brightness,
						&frameProc->colorCorrection.contrast,
						&frameProc->colorCorrection.saturation,
						&frameProc->colorCorrection.whiteBalance.x,
						&frameProc->colorCorrection.whiteBalance.y,
						&frameProc->colorCorrection.whiteBalance.z,
						&frameProc->colorCorrection.hueShift);
				}
				// ColorGrading: "style,intensity,customColor.x,customColor.y,customColor.z"
				if (prop.first == "ColorGrading") {
					sscanf_s(prop.second.c_str(), "%d,%f,%f,%f,%f",
						&frameProc->colorGrading.style,
						&frameProc->colorGrading.intensity,
						&frameProc->colorGrading.customColor.x,
						&frameProc->colorGrading.customColor.y,
						&frameProc->colorGrading.customColor.z);
				}
			}
		}

		// =========================
		// ShowtheFrame
		// =========================
		if (auto* showFrame = dynamic_cast<ShowtheFrame_Node*>(node)) {
			// 无需额外初始化
		}

		// =========================
		// PlaySound
		// =========================
		if (auto* playSound = dynamic_cast<PlaySound_Node*>(node)) {
			for (auto& pin : n.pins) {
				if (pin.name == "Path" && pin.literal.has_value()) {
					playSound->path = new Value();
					*playSound->path = Value::makeString(pin.literal.value());
				}
				if (pin.name == "shouldLoop" && pin.literal.has_value()) {
					playSound->loop = new Value();
					*playSound->loop = Value::makeBool(pin.literal.value() == "true");
				}
				if (pin.name == "Volume" && pin.literal.has_value()) {
					playSound->volume = new Value();
					*playSound->volume = Value::makeFloat(std::stof(pin.literal.value()));
				}
			}
		}

		// =========================
		// PauseSound
		// =========================
		if (auto* pauseSound = dynamic_cast<PauseSound_Node*>(node)) {
			for (auto& pin : n.pins) {
				if (pin.name == "Path" && pin.literal.has_value()) {
					pauseSound->path = new Value();
					*pauseSound->path = Value::makeString(pin.literal.value());
				}
			}
		}

	}
}


void BlueprintCompiler::BuildDataLinks(const BlueprintData& data) {
	for (auto& link : data.links) {

		if (link.sourcePin == "exec" || link.sourcePin == "then") {
			continue;
		}

		NODE* src = currentCompiled->nodeMap[link.sourceNode];
		NODE* dst = currentCompiled->nodeMap[link.targetNode];
		if (!src || !dst) continue;

		Value* out = nullptr;

		// =========================
		// 获取输出
		// =========================
		if (auto* bin = dynamic_cast<BinaryOpNode*>(src)) {
			out = &bin->OutData[0];
		}
		else if (auto* get = dynamic_cast<GET_VAR*>(src)) {
			out = &get->outValue;
		}
		else if (auto* set = dynamic_cast<SET_VAR*>(src)) {
			out = &set->outValue;   // ✅ 支持链式
		}
		// =========================
		// Render / FrameProcess / ShowtheFrame 输出
		// =========================
		else if (auto* render = dynamic_cast<Render_Node*>(src)) {
			out = &render->outFrame;
		}
		else if (auto* frameProc = dynamic_cast<FrameProcess_Node*>(src)) {
			out = &frameProc->outFrame;
		}

		// =========================
		// Binary
		// =========================
		if (auto* dstBin = dynamic_cast<BinaryOpNode*>(dst)) {
			if (out) {
				int index = (link.targetPin == "A") ? 0 : 1;
				dstBin->InData[index] = out;
			}
		}

		// =========================
		// SET_VAR（只处理 NewValue）
		// =========================
		if (auto* set = dynamic_cast<SET_VAR*>(dst)) {
			if (link.targetPin == "NewValue") {
				set->inValue = out;
			}
		}

		// =========================
		// If / While / Transform（你原来的）
		// =========================
		if (auto* branch = dynamic_cast<If_Node*>(dst)) {
			if (link.targetPin == "shouldRunA" && out) {
				branch->condition = out;
			}
		}

		if (auto* whileNode = dynamic_cast<While_Node*>(dst)) {
			if (link.targetPin == "shouldRunLoop" && out) {
				whileNode->condition = out;
			}
		}

		if (auto* st = dynamic_cast<SetTransforNode*>(dst)) {
			if (link.targetPin == "Location.x") st->in_loc_x = out;
			if (link.targetPin == "Location.y") st->in_loc_y = out;
			if (link.targetPin == "Location.z") st->in_loc_z = out;
		}

		// =========================
		// PrintText
		// =========================
		if (auto* printText = dynamic_cast<PrintText_Node*>(dst)) {
			if (link.targetPin == "Text" && out) {
				printText->text = out;
			}
		}

		// =========================
		// Render
		// =========================
		if (auto* render = dynamic_cast<Render_Node*>(dst)) {
			if (link.targetPin == "samplingMethod" && out) {
				render->samplingMethod = out;
			}
			if (link.targetPin == "msaaMultiple" && out) {
				render->msaaMultiple = out;
			}
		}

		// =========================
		// FrameProcess
		// =========================
		if (auto* frameProc = dynamic_cast<FrameProcess_Node*>(dst)) {
			if (link.targetPin == "ProcessOp" && out) {
				frameProc->processName = out;
			}
			if (link.targetPin == "FrameToProcess" && out) {
				frameProc->inFrame = out;
			}
		}

		// =========================
		// ShowtheFrame
		// =========================
		if (auto* showFrame = dynamic_cast<ShowtheFrame_Node*>(dst)) {
			if (link.targetPin == "Frame" && out) {
				showFrame->inFrame = out;
			}
		}

		// =========================
		// PlaySound
		// =========================
		if (auto* playSound = dynamic_cast<PlaySound_Node*>(dst)) {
			if (link.targetPin == "Path" && out) {
				playSound->path = out;
			}
			if (link.targetPin == "shouldLoop" && out) {
				playSound->loop = out;
			}
			if (link.targetPin == "Volume" && out) {
				playSound->volume = out;
			}
		}

		// =========================
		// PauseSound
		// =========================
		if (auto* pauseSound = dynamic_cast<PauseSound_Node*>(dst)) {
			if (link.targetPin == "Path" && out) {
				pauseSound->path = out;
			}
		}
	}
}

