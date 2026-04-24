// Copyright 2026 MrSeagull. All Rights Reserved.
#include "Json_LevelData_ReadWrite.h"
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include "FileSystem.h"

namespace fs = std::filesystem;

// ========== 辅助结构序列化 ==========
void to_json(json& j, const Location3D& v) {
	j = json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
}
void from_json(const json& j, Location3D& v) {
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
	j.at("z").get_to(v.z);
}

void to_json(json& j, const Location2D& v) {
	j = json{ {"x", v.x}, {"y", v.y} };
}
void from_json(const json& j, Location2D& v) {
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
}

void to_json(json& j, const Location2DInt& v) {
	j = json{ {"x", v.x}, {"y", v.y} };
}
void from_json(const json& j, Location2DInt& v) {
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
}

void to_json(json& j, const RotationEuler& v) {
	j = json{ {"r", v.r} };
}
void from_json(const json& j, RotationEuler& v) {
	j.at("r").get_to(v.r);
}

void to_json(json& j, const Scale2D& v) {
	j = json{ {"x", v.x}, {"y", v.y} };
}
void from_json(const json& j, Scale2D& v) {
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
	// 文档：<=0 视为1.0，但这里保留原始值，使用时处理
}

void to_json(json& j, const Size2D& v) {
	j = json{ {"x", v.x}, {"y", v.y} };
}
void from_json(const json& j, Size2D& v) {
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
}

void to_json(json& j, const Size2DInt& v) {
	j = json{ {"x", v.x}, {"y", v.y} };
}
void from_json(const json& j, Size2DInt& v) {
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
}

// ========== 组件序列化 ==========
void to_json(json& j, const TransformComponent& v) {
	j = json{ {"Location", v.Location}, {"Rotation", v.Rotation}, {"Scale", v.Scale} };
}
void from_json(const json& j, TransformComponent& v) {
	j.at("Location").get_to(v.Location);
	j.at("Rotation").get_to(v.Rotation);
	j.at("Scale").get_to(v.Scale);
}

void to_json(json& j, const PictureComponent& v) {
	j = json{ {"Path", v.Path}, {"Location", v.Location}, {"Rotation", v.Rotation}, {"Size", v.Size} };
}
void from_json(const json& j, PictureComponent& v) {
	j.at("Path").get_to(v.Path);
	j.at("Location").get_to(v.Location);
	j.at("Rotation").get_to(v.Rotation);
	j.at("Size").get_to(v.Size);
}

void to_json(json& j, const TextblockComponent::TextInfo& v) {
	j = json{ {"component", v.component}, {"Font size", v.Font_size}, {"ANSI Print", v.ANSI_Print} };
}
void from_json(const json& j, TextblockComponent::TextInfo& v) {
	j.at("component").get_to(v.component);
	j.at("Font size").get_to(v.Font_size);
	j.at("ANSI Print").get_to(v.ANSI_Print);
}

void to_json(json& j, const TextblockComponent& v) {
	j = json{ {"Location", v.Location}, {"Size", v.Size}, {"Text", v.Text}, {"Scale", v.Scale} };
}
void from_json(const json& j, TextblockComponent& v) {
	j.at("Location").get_to(v.Location);
	j.at("Size").get_to(v.Size);
	j.at("Text").get_to(v.Text);
	j.at("Scale").get_to(v.Scale);
}

void to_json(json& j, const TriggerAreaComponent& v) {
	j = json{ {"Location", v.Location}, {"Size", v.Size} };
}
void from_json(const json& j, TriggerAreaComponent& v) {
	j.at("Location").get_to(v.Location);
	j.at("Size").get_to(v.Size);
}

void to_json(json& j, const BlueprintComponent& v) {
	j = json{ {"Path", v.Path} };
}
void from_json(const json& j, BlueprintComponent& v) {
	j.at("Path").get_to(v.Path);
}

// ========== ObjectData 序列化/反序列化（仅组件） ==========
void to_json(json& j, const ObjectData& v) {
	// 只输出组件，关系由 LevelData 处理
	if (v.Transform) j["Transform"] = *v.Transform;
	if (v.Picture) j["Picture"] = *v.Picture;
	if (v.Textblock) j["Textblock"] = *v.Textblock;
	if (v.TriggerArea) j["TriggerArea"] = *v.TriggerArea;
	if (v.Blueprint) j["Blueprint"] = *v.Blueprint;
}

void from_json(const json& j, ObjectData& v) {
	// 只解析组件，关系由 LevelData 处理
	v.Transform.reset();
	v.Picture.reset();
	v.Textblock.reset();
	v.TriggerArea.reset();
	v.Blueprint.reset();

	if (j.contains("Transform")) v.Transform = j.at("Transform").get<TransformComponent>();
	if (j.contains("Picture")) v.Picture = j.at("Picture").get<PictureComponent>();
	if (j.contains("Textblock")) v.Textblock = j.at("Textblock").get<TextblockComponent>();
	if (j.contains("TriggerArea")) v.TriggerArea = j.at("TriggerArea").get<TriggerAreaComponent>();
	if (j.contains("Blueprint")) v.Blueprint = j.at("Blueprint").get<BlueprintComponent>();
}

// ========== LevelData 序列化/反序列化（处理关系） ==========
void to_json(json& j, const LevelData& v) {
	json levelObj;

	// 收集根对象列表（不变）
	std::vector<std::string> rootObjects;
	for (const auto& [name, obj] : v.objects) {
		if (obj->parent == nullptr) {
			rootObjects.push_back(name);
		}
	}
	if (!rootObjects.empty()) levelObj["SubObjects"] = rootObjects;

	// 递归辅助函数：将所有对象添加到 levelObj
	std::function<void(const std::map<std::string, ObjectData*>&)> collect;
	collect = [&](const std::map<std::string, ObjectData*>& children) {
		for (const auto& [name, obj] : children) {
			json objJson = *obj;               // 获取组件部分
			if (obj->parent != nullptr) {
				objJson["ParObject"] = obj->parent->name;
			}
			if (!obj->objects.empty()) {
				std::vector<std::string> subNames;
				for (const auto& [subName, subObj] : obj->objects) {
					subNames.push_back(subName);
				}
				objJson["SubObjects"] = subNames;
			}
			levelObj[name] = objJson;

			// 递归处理子对象
			if (!obj->objects.empty()) {
				collect(obj->objects);
			}
		}
		};
	collect(v.objects);   // 从根对象开始递归

	j = json{ {v.name, levelObj} };
}

void from_json(const json& j, LevelData& v) {
	if (j.size() != 1)
		throw std::runtime_error("Level JSON must have exactly one top-level key (level name)");
	auto it = j.begin();
	v.name = it.key();
	const json& levelObj = it.value();

	// 临时存放所有对象（std::unique_ptr 安全）
	std::unordered_map<std::string, std::unique_ptr<ObjectData>> allObjects;
	std::unordered_map<std::string, std::string> parentNames;          // childName -> parentName
	std::unordered_map<std::string, std::vector<std::string>> childrenNames; // parentName -> childrenNames

	// 第一步：创建所有对象，填充组件，记录关系字符串
	for (auto& [objName, objJson] : levelObj.items()) {
		if (objName == "SubObjects") continue;
		auto obj = std::make_unique<ObjectData>();
		obj->name = objName;
		objJson.get_to(*obj);      // 填充组件
		if (objJson.contains("ParObject")) {
			parentNames[objName] = objJson["ParObject"].get<std::string>();
		}
		if (objJson.contains("SubObjects")) {
			childrenNames[objName] = objJson["SubObjects"].get<std::vector<std::string>>();
		}
		allObjects[objName] = std::move(obj);
	}

	// 第二步：递归构建树，转移所有权
	std::function<ObjectData* (const std::string&)> buildTree =
		[&](const std::string& objName) -> ObjectData* {
		auto it = allObjects.find(objName);
		if (it == allObjects.end())
			throw std::runtime_error("Object not found: " + objName);

		ObjectData* ptr = it->second.release();   // 所有权取出
		allObjects.erase(it);

		// 处理该节点的子对象
		auto childIt = childrenNames.find(objName);
		if (childIt != childrenNames.end()) {
			for (const auto& childName : childIt->second) {
				ObjectData* childPtr = buildTree(childName);
				childPtr->parent = ptr;
				ptr->objects[childName] = childPtr;
			}
		}
		return ptr;
		};

	// 第三步：处理根对象（从顶层 SubObjects 列表获取）
	std::vector<std::string> rootNames;
	if (levelObj.contains("SubObjects")) {
		rootNames = levelObj["SubObjects"].get<std::vector<std::string>>();
	}
	else {
		// Fallback：所有没有父对象的都视为根
		for (const auto& [name, _] : allObjects) {
			if (parentNames.find(name) == parentNames.end())
				rootNames.push_back(name);
		}
	}

	// 构建根对象并放入 v.objects（所有权转移）
	for (const auto& rootName : rootNames) {
		ObjectData* rootPtr = buildTree(rootName);
		rootPtr->parent = nullptr;
		v.objects[rootName] = rootPtr;
	}

	// 安全检查：不应该有剩余对象
	if (!allObjects.empty()) {
		throw std::runtime_error("Some objects are not referenced in the tree.");
	}
}

// ========== 文件读写 ==========
bool WriteLevelData(const std::string& filepath, const LevelData& data) {
	try {
		json j = data;  // 自动调用 to_json
		std::ofstream file(filepath, std::ios::binary);
		if (!file.is_open()) return false;
		file << j.dump(4);  // 美观输出，可调为紧凑格式(-1)以提升性能
		return true;
	}
	catch (...) {
		return false;
	}
}
bool FileSystem::WriteLevelData(const std::string& filepath, const LevelData& data) {
    return ::WriteLevelData(filepath, data);
}

LevelData ReadLevelData(const std::string& filepath) {
	if (!fs::exists(filepath)) {
		throw std::runtime_error("File not found: " + filepath);
	}
	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open file: " + filepath);
	}
	json j;
	file >> j;
	return j.get<LevelData>();  // 自动调用 from_json
}
LevelData FileSystem::ReadLevelData(const std::string& filepath) {
    return ::ReadLevelData(filepath);
}