// Copyright 2026 MrSeagull. All Rights Reserved.
#pragma once
#include <iostream>
#include <windows.h>
#include <unordered_map>
#include "SharedTypes.h"

class WindowsSystem {
public:
	WindowsSystem(const WindowsSystem&) = delete;
	WindowsSystem& operator=(const WindowsSystem&) = delete;
	static WindowsSystem& getInstance() {
        static WindowsSystem instance;
        return instance;
	}

	bool OpenProjectStructureViewer(const char* ViewerExePath, const char* ProjectRoot = nullptr);
	bool RefreshProjectStructureViewer();
	bool CloseProjectStructureViewer();
private:
	std::vector<LevelData> levels;
	char* currentProjectDirectory;

	std::unordered_map<std::string, HANDLE> ChildProcess;
    std::unordered_map<std::string, HANDLE> ChildThread;
	std::unordered_map<std::string, HANDLE> ProjectStructureViewer_ops;

	void CleanUpHandles(const std::string& processName);

	WindowsSystem() {
		std::string testpath = "E:\\Projects\\C++Projects\\OFGal_Engine";
		size_t len = testpath.size() + 1;
		currentProjectDirectory = new char[len];
		strcpy_s(currentProjectDirectory, len, testpath.c_str());

		ProjectStructureViewer_ops["Exit"] = NULL;
		ProjectStructureViewer_ops["Refresh"] = NULL;
		ProjectStructureViewer_ops["Write"] = NULL;
	}
	~WindowsSystem() {
		CloseProjectStructureViewer();          // 确保子进程退出
		CleanUpHandles("ProjectStructureViewer");
		delete[] currentProjectDirectory;
	}
};