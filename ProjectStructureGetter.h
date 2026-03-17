// Copyright 2026 MrSeagull. All Rights Reserved.

#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <filesystem>
#include <memory>

struct FolderStructure {
    std::string SelfName;
	std::vector<std::string> Files;
	std::map<std::string, std::unique_ptr<FolderStructure>> SubFolders;
};

struct ProjectStructure { 
	std::string RootDirectory;
	FolderStructure Self;
};

ProjectStructure GetProjectStructure(const char* RootDirectory);
FolderStructure GetFolderStructure(const char* Directory);