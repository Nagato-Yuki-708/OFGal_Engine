// Copyright 2026 MrSeagull. All Rights Reserved.
#include "Json_BPData_ReadWrite.h"
#include <fstream>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

// ========== Pin 序列化 ==========
void to_json(MyJson& j, const Pin& v) {
    j = MyJson();
    j["name"] = v.name;
    j["io"] = v.io;
    j["type"] = v.type;
    if (v.literal.has_value()) {
        j["literal"] = v.literal.value();
    }
}

void from_json(const MyJson& j, Pin& v) {
    j.at("name").get_to(v.name);
    j.at("io").get_to(v.io);
    j.at("type").get_to(v.type);
    if (j.contains("literal") && !j["literal"].is_null()) {
        v.literal = j["literal"].get<std::string>();
    }
    else {
        v.literal = std::nullopt;
    }
}

// ========== Node 序列化 ==========
void to_json(MyJson& j, const Node& v) {
    j = MyJson();
    j["id"] = v.id;
    j["type"] = v.type;

    // pins: vector<Pin> 需要通过 to_json 转换为 MyJson
    MyJson pinsJson;
    to_json(pinsJson, v.pins);          // 使用通用的 vector<T> to_json
    j["pins"] = std::move(pinsJson);

    // properties: map<string,string> 需要通过 to_json 转换
    MyJson propsJson;
    to_json(propsJson, v.properties);   // 使用通用的 map<K,V> to_json
    j["properties"] = std::move(propsJson);
}

void from_json(const MyJson& j, Node& v) {
    j.at("id").get_to(v.id);
    j.at("type").get_to(v.type);
    j.at("pins").get_to(v.pins);
    j.at("properties").get_to(v.properties);
}

// ========== Variable 序列化 ==========
void to_json(MyJson& j, const Variable& v) {
    j = MyJson();
    j["name"] = v.name;
    j["type"] = v.type;
    j["value"] = v.value;
}

void from_json(const MyJson& j, Variable& v) {
    j.at("name").get_to(v.name);
    j.at("type").get_to(v.type);
    j.at("value").get_to(v.value);
}

// ========== Parameter 序列化 ==========
void to_json(MyJson& j, const Parameter& v) {
    j = MyJson();
    j["name"] = v.name;
    j["type"] = v.type;
    j["defaultValue"] = v.defaultValue;
}

void from_json(const MyJson& j, Parameter& v) {
    j.at("name").get_to(v.name);
    j.at("type").get_to(v.type);
    j.at("defaultValue").get_to(v.defaultValue);
}

// ========== Event 序列化 ==========
void to_json(MyJson& j, const Event& v) {
    j = MyJson();
    j["Event_Name"] = v.event_name;
    j["id"] = v.id;
}

void from_json(const MyJson& j, Event& v) {
    j.at("Event_Name").get_to(v.event_name);
    j.at("id").get_to(v.id);
}

// ========== Link 序列化 ==========
void to_json(MyJson& j, const Link& v) {
    j = MyJson();
    j["sourceNode"] = v.sourceNode;
    j["sourcePin"] = v.sourcePin;
    j["targetNode"] = v.targetNode;
    j["targetPin"] = v.targetPin;
}

void from_json(const MyJson& j, Link& v) {
    j.at("sourceNode").get_to(v.sourceNode);
    j.at("sourcePin").get_to(v.sourcePin);
    j.at("targetNode").get_to(v.targetNode);
    j.at("targetPin").get_to(v.targetPin);
}

// ========== BlueprintData 序列化 ==========
void to_json(MyJson& j, const BlueprintData& v) {
    j = MyJson();
    j["Name"] = v.name;
    j["id"] = v.id;

    // 处理各个自定义容器类型，通过 to_json 转换
    MyJson nodesJson;
    to_json(nodesJson, v.nodes);
    j["Nodes"] = std::move(nodesJson);

    MyJson varsJson;
    to_json(varsJson, v.variables);
    j["Variables"] = std::move(varsJson);

    MyJson inParamsJson;
    to_json(inParamsJson, v.inParameters);
    j["InParameters"] = std::move(inParamsJson);

    MyJson outParamsJson;
    to_json(outParamsJson, v.outParameters);
    j["OutParameters"] = std::move(outParamsJson);

    MyJson eventsJson;
    to_json(eventsJson, v.events);
    j["Events"] = std::move(eventsJson);

    MyJson linksJson;
    to_json(linksJson, v.links);
    j["Links"] = std::move(linksJson);
}

void from_json(const MyJson& j, BlueprintData& v) {
    j.at("Name").get_to(v.name);
    j.at("id").get_to(v.id);
    j.at("Nodes").get_to(v.nodes);
    j.at("Variables").get_to(v.variables);
    j.at("InParameters").get_to(v.inParameters);
    j.at("OutParameters").get_to(v.outParameters);
    j.at("Events").get_to(v.events);
    j.at("Links").get_to(v.links);
}

// ========== 文件读写 ==========
bool WriteBPData(const std::string& filepath, const BlueprintData& data) {
    try {
        MyJson j;
        to_json(j, data);          // 显式调用 to_json，避免隐式转换
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;
        file << j.dump(4);
        return true;
    }
    catch (...) {
        return false;
    }
}

BlueprintData ReadBPData(const std::string& filepath) {
    if (!fs::exists(filepath)) {
        throw std::runtime_error("File not found: " + filepath);
    }
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    MyJson j;
    file >> j;
    return j.get<BlueprintData>();   // 自动调用 from_json
}