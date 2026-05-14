// Copyright 2026 MrSeagull. All Rights Reserved.
#include "Json_LevelData_ReadWrite.h"
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include "FileSystem.h"

namespace fs = std::filesystem;

// ========== 辅助结构序列化 ==========
void to_json(MyJson& j, const Location3D& v) {
    j = MyJson();
    j["x"] = v.x;
    j["y"] = v.y;
    j["z"] = v.z;
}
void from_json(const MyJson& j, Location3D& v) {
    j.at("x").get_to(v.x);
    j.at("y").get_to(v.y);
    j.at("z").get_to(v.z);
}

void to_json(MyJson& j, const Location2D& v) {
    j = MyJson();
    j["x"] = v.x;
    j["y"] = v.y;
}
void from_json(const MyJson& j, Location2D& v) {
    j.at("x").get_to(v.x);
    j.at("y").get_to(v.y);
}

void to_json(MyJson& j, const Location2DInt& v) {
    j = MyJson();
    j["x"] = v.x;
    j["y"] = v.y;
}
void from_json(const MyJson& j, Location2DInt& v) {
    j.at("x").get_to(v.x);
    j.at("y").get_to(v.y);
}

void to_json(MyJson& j, const RotationEuler& v) {
    j = MyJson();
    j["r"] = v.r;
}
void from_json(const MyJson& j, RotationEuler& v) {
    j.at("r").get_to(v.r);
}

void to_json(MyJson& j, const Scale2D& v) {
    j = MyJson();
    j["x"] = v.x;
    j["y"] = v.y;
}
void from_json(const MyJson& j, Scale2D& v) {
    j.at("x").get_to(v.x);
    j.at("y").get_to(v.y);
}

void to_json(MyJson& j, const Size2D& v) {
    j = MyJson();
    j["x"] = v.x;
    j["y"] = v.y;
}
void from_json(const MyJson& j, Size2D& v) {
    j.at("x").get_to(v.x);
    j.at("y").get_to(v.y);
}

void to_json(MyJson& j, const Size2DInt& v) {
    j = MyJson();
    j["x"] = v.x;
    j["y"] = v.y;
}
void from_json(const MyJson& j, Size2DInt& v) {
    j.at("x").get_to(v.x);
    j.at("y").get_to(v.y);
}

// ========== 组件序列化 ==========
void to_json(MyJson& j, const TransformComponent& v) {
    j = MyJson();
    MyJson loc;
    to_json(loc, v.Location);
    j["Location"] = std::move(loc);
    MyJson rot;
    to_json(rot, v.Rotation);
    j["Rotation"] = std::move(rot);
    MyJson scale;
    to_json(scale, v.Scale);
    j["Scale"] = std::move(scale);
}
void from_json(const MyJson& j, TransformComponent& v) {
    j.at("Location").get_to(v.Location);
    j.at("Rotation").get_to(v.Rotation);
    j.at("Scale").get_to(v.Scale);
}

void to_json(MyJson& j, const PictureComponent& v) {
    j = MyJson();
    j["Path"] = v.Path;
    MyJson loc;
    to_json(loc, v.Location);
    j["Location"] = std::move(loc);
    MyJson rot;
    to_json(rot, v.Rotation);
    j["Rotation"] = std::move(rot);
    MyJson size;
    to_json(size, v.Size);
    j["Size"] = std::move(size);
}
void from_json(const MyJson& j, PictureComponent& v) {
    j.at("Path").get_to(v.Path);
    j.at("Location").get_to(v.Location);
    j.at("Rotation").get_to(v.Rotation);
    j.at("Size").get_to(v.Size);
}

void to_json(MyJson& j, const TextblockComponent::TextInfo& v) {
    j = MyJson();
    j["component"] = v.component;
    j["Font size"] = v.Font_size;
    j["ANSI Print"] = v.ANSI_Print;
}
void from_json(const MyJson& j, TextblockComponent::TextInfo& v) {
    j.at("component").get_to(v.component);
    j.at("Font size").get_to(v.Font_size);
    j.at("ANSI Print").get_to(v.ANSI_Print);
}

void to_json(MyJson& j, const TextblockComponent& v) {
    j = MyJson();
    MyJson loc;
    to_json(loc, v.Location);
    j["Location"] = std::move(loc);
    MyJson size;
    to_json(size, v.Size);
    j["Size"] = std::move(size);
    // Text 是 vector<TextInfo>
    MyJson textArr;
    to_json(textArr, v.Text);       // 通用 vector to_json
    j["Text"] = std::move(textArr);
    MyJson scale;
    to_json(scale, v.Scale);
    j["Scale"] = std::move(scale);
}
void from_json(const MyJson& j, TextblockComponent& v) {
    j.at("Location").get_to(v.Location);
    j.at("Size").get_to(v.Size);
    j.at("Text").get_to(v.Text);
    j.at("Scale").get_to(v.Scale);
}

void to_json(MyJson& j, const TriggerAreaComponent& v) {
    j = MyJson();
    MyJson loc;
    to_json(loc, v.Location);
    j["Location"] = std::move(loc);
    MyJson size;
    to_json(size, v.Size);
    j["Size"] = std::move(size);
}
void from_json(const MyJson& j, TriggerAreaComponent& v) {
    j.at("Location").get_to(v.Location);
    j.at("Size").get_to(v.Size);
}

void to_json(MyJson& j, const BlueprintComponent& v) {
    j = MyJson();
    j["Path"] = v.Path;
}
void from_json(const MyJson& j, BlueprintComponent& v) {
    j.at("Path").get_to(v.Path);
}

// ========== ObjectData 序列化（仅组件） ==========
void to_json(MyJson& j, const ObjectData& v) {
    j = MyJson();   // 空对象，后续添加组件
    if (v.Transform) {
        MyJson t;
        to_json(t, *v.Transform);
        j["Transform"] = std::move(t);
    }
    if (v.Picture) {
        MyJson p;
        to_json(p, *v.Picture);
        j["Picture"] = std::move(p);
    }
    if (v.Textblock) {
        MyJson tb;
        to_json(tb, *v.Textblock);
        j["Textblock"] = std::move(tb);
    }
    if (v.TriggerArea) {
        MyJson ta;
        to_json(ta, *v.TriggerArea);
        j["TriggerArea"] = std::move(ta);
    }
    if (v.Blueprint) {
        MyJson bp;
        to_json(bp, *v.Blueprint);
        j["Blueprint"] = std::move(bp);
    }
}

void from_json(const MyJson& j, ObjectData& v) {
    v.Transform.reset();
    v.Picture.reset();
    v.Textblock.reset();
    v.TriggerArea.reset();
    v.Blueprint.reset();

    if (j.contains("Transform")) v.Transform = j.at("Transform").get<TransformComponent>();
    if (j.contains("Picture"))   v.Picture = j.at("Picture").get<PictureComponent>();
    if (j.contains("Textblock")) v.Textblock = j.at("Textblock").get<TextblockComponent>();
    if (j.contains("TriggerArea")) v.TriggerArea = j.at("TriggerArea").get<TriggerAreaComponent>();
    if (j.contains("Blueprint")) v.Blueprint = j.at("Blueprint").get<BlueprintComponent>();
}

// ========== LevelData 序列化/反序列化（处理关系） ==========
void to_json(MyJson& j, const LevelData& v) {
    MyJson levelObj;

    // 收集根对象列表
    std::vector<std::string> rootObjects;
    for (const auto& [name, obj] : v.objects) {
        if (obj->parent == nullptr) {
            rootObjects.push_back(name);
        }
    }
    if (!rootObjects.empty()) {
        MyJson subArr;
        to_json(subArr, rootObjects);       // 使用通用 vector<string> to_json
        levelObj["SubObjects"] = std::move(subArr);
    }

    // 递归添加所有对象
    std::function<void(const std::map<std::string, ObjectData*>&)> collect;
    collect = [&](const std::map<std::string, ObjectData*>& children) {
        for (const auto& [name, obj] : children) {
            MyJson objJson;
            to_json(objJson, *obj);         // 组件部分
            if (obj->parent != nullptr) {
                objJson["ParObject"] = obj->parent->name;
            }
            if (!obj->objects.empty()) {
                std::vector<std::string> subNames;
                for (const auto& [subName, _] : obj->objects) {
                    subNames.push_back(subName);
                }
                MyJson subArr;
                to_json(subArr, subNames);
                objJson["SubObjects"] = std::move(subArr);
            }
            levelObj[name] = std::move(objJson);

            if (!obj->objects.empty()) {
                collect(obj->objects);
            }
        }
        };
    collect(v.objects);

    j = MyJson();
    j[v.name] = std::move(levelObj);
}

void from_json(const MyJson& j, LevelData& v) {
    if (j.size() != 1)
        throw std::runtime_error("Level JSON must have exactly one top-level key (level name)");
    auto it = j.begin();
    v.name = it->first;          // 假设 begin() 返回 pair<const string, MyJson> 迭代器
    const MyJson& levelObj = it->second;

    std::unordered_map<std::string, std::unique_ptr<ObjectData>> allObjects;
    std::unordered_map<std::string, std::string> parentNames;
    std::unordered_map<std::string, std::vector<std::string>> childrenNames;

    for (auto& [objName, objJson] : levelObj.items()) {
        if (objName == "SubObjects") continue;
        auto obj = std::make_unique<ObjectData>();
        obj->name = objName;
        from_json(objJson, *obj);          // 填充组件
        if (objJson.contains("ParObject")) {
            parentNames[objName] = objJson["ParObject"].get<std::string>();
        }
        if (objJson.contains("SubObjects")) {
            childrenNames[objName] = objJson["SubObjects"].get<std::vector<std::string>>();
        }
        allObjects[objName] = std::move(obj);
    }

    std::function<ObjectData* (const std::string&)> buildTree =
        [&](const std::string& objName) -> ObjectData* {
        auto it = allObjects.find(objName);
        if (it == allObjects.end())
            throw std::runtime_error("Object not found: " + objName);
        ObjectData* ptr = it->second.release();
        allObjects.erase(it);
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

    std::vector<std::string> rootNames;
    if (levelObj.contains("SubObjects")) {
        rootNames = levelObj["SubObjects"].get<std::vector<std::string>>();
    }
    else {
        for (const auto& [name, _] : allObjects) {
            if (parentNames.find(name) == parentNames.end())
                rootNames.push_back(name);
        }
    }

    for (const auto& rootName : rootNames) {
        ObjectData* rootPtr = buildTree(rootName);
        rootPtr->parent = nullptr;
        v.objects[rootName] = rootPtr;
    }

    if (!allObjects.empty()) {
        throw std::runtime_error("Some objects are not referenced in the tree.");
    }
}

// ========== 文件读写 ==========
bool WriteLevelData(const std::string& filepath, const LevelData& data) {
    try {
        MyJson j;
        to_json(j, data);               // 显式调用，避免隐式转换问题
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;
        file << j.dump(4);
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
    MyJson j;
    file >> j;
    return j.get<LevelData>();
}

LevelData FileSystem::ReadLevelData(const std::string& filepath) {
    return ::ReadLevelData(filepath);
}