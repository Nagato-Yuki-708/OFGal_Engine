// Copyright 2026 MrSeagull. All Rights Reserved.
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include "SharedTypes.h"
#include "MyJson.h"

// ========== 结构体序列化声明（使用 MyJson，避免别名冲突） ==========
void to_json(MyJson& j, const Pin& v);
void from_json(const MyJson& j, Pin& v);

void to_json(MyJson& j, const Node& v);
void from_json(const MyJson& j, Node& v);

void to_json(MyJson& j, const Variable& v);
void from_json(const MyJson& j, Variable& v);

void to_json(MyJson& j, const Parameter& v);
void from_json(const MyJson& j, Parameter& v);

void to_json(MyJson& j, const Event& v);
void from_json(const MyJson& j, Event& v);

void to_json(MyJson& j, const Link& v);
void from_json(const MyJson& j, Link& v);

void to_json(MyJson& j, const BlueprintData& v);
void from_json(const MyJson& j, BlueprintData& v);

// ========== 文件读写接口 ==========
bool WriteBPData(const std::string& filepath, const BlueprintData& data);
BlueprintData ReadBPData(const std::string& filepath);