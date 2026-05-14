// Copyright 2026 MrSeagull. All Rights Reserved.
#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <functional>               // 为 std::function
#include "MyJson.h"
#include "SharedTypes.h"

// 读写函数声明（直接使用 MyJson，不定义全局别名）
void to_json(MyJson& j, const Location3D& v);
void from_json(const MyJson& j, Location3D& v);
void to_json(MyJson& j, const Location2D& v);
void from_json(const MyJson& j, Location2D& v);
void to_json(MyJson& j, const Location2DInt& v);
void from_json(const MyJson& j, Location2DInt& v);
void to_json(MyJson& j, const RotationEuler& v);
void from_json(const MyJson& j, RotationEuler& v);
void to_json(MyJson& j, const Scale2D& v);
void from_json(const MyJson& j, Scale2D& v);
void to_json(MyJson& j, const Size2D& v);
void from_json(const MyJson& j, Size2D& v);
void to_json(MyJson& j, const Size2DInt& v);
void from_json(const MyJson& j, Size2DInt& v);
void to_json(MyJson& j, const TransformComponent& v);
void from_json(const MyJson& j, TransformComponent& v);
void to_json(MyJson& j, const PictureComponent& v);
void from_json(const MyJson& j, PictureComponent& v);
void to_json(MyJson& j, const TextblockComponent::TextInfo& v);
void from_json(const MyJson& j, TextblockComponent::TextInfo& v);
void to_json(MyJson& j, const TextblockComponent& v);
void from_json(const MyJson& j, TextblockComponent& v);
void to_json(MyJson& j, const TriggerAreaComponent& v);
void from_json(const MyJson& j, TriggerAreaComponent& v);
void to_json(MyJson& j, const BlueprintComponent& v);
void from_json(const MyJson& j, BlueprintComponent& v);
void to_json(MyJson& j, const ObjectData& v);
void from_json(const MyJson& j, ObjectData& v);
void to_json(MyJson& j, const LevelData& v);
void from_json(const MyJson& j, LevelData& v);

// 文件读写接口
bool WriteLevelData(const std::string& filepath, const LevelData& data);
LevelData ReadLevelData(const std::string& filepath);