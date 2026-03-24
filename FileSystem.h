// Copyright 2026 MrSeagull. All Rights Reserved.

#pragma once
#include "BMP_Reader.h"
#include "Json_LevelData_ReadWrite.h"
#include "ProjectStructureGetter.h"
#include "Json_BPData_ReadWrite.h"
#include "SharedTypes.h"
#include "_EventBus.h"

class FileSystem {
public:

    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;

	static FileSystem& getInstance()
	{
		static FileSystem instance;
		return instance;
	}

    // 场景文件读写接口(*.level)
    bool WriteLevelData(const std::string& filepath, const LevelData& data);
    LevelData ReadLevelData(const std::string& filepath);
    // 图片文件读写接口(*.bmp)
    BMP_Data Read_A_BMP(const char* filepath);
    // 项目文件结构获取接口
    ProjectStructure GetProjectStructure(const char* RootDirectory);
    FolderStructure GetFolderStructure(const char* Directory);

    // 蓝图文件读写接口(*.bp)
    bool WriteBPData(const std::string& filepath, const BlueprintData& data);
    BlueprintData ReadBPData(const std::string& filepath);
private:
    ProjectStructure project_structure;
    std::string project_path;

    FileSystem() {
		_EventBus::getInstance().subscribe_GetFolderStructure(::GetFolderStructure);
		_EventBus::getInstance().subscribe_GetProjectStructure(::GetProjectStructure);
		_EventBus::getInstance().subscribe_ReadBMP(::Read_A_BMP);
		_EventBus::getInstance().subscribe_ReadBPData(::ReadBPData);
		_EventBus::getInstance().subscribe_ReadLevelData(::ReadLevelData);
		_EventBus::getInstance().subscribe_WriteBPData(::WriteBPData);
		_EventBus::getInstance().subscribe_WriteLevelData(::WriteLevelData);
    }
    ~FileSystem() = default;
};