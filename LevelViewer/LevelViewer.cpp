// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#include "LevelViewer.h"

LevelViewer::LevelViewer()
	: m_hSharedMem(NULL)
	, m_pSharedMemView(nullptr)
	, m_hEventLevelChanged(NULL)
	, m_hEventRenderPreview(NULL)
{
	// 初始化渲染系统和窗口
	RenderingSystem::getInstance();

	// 打开共享内存（只读）
	m_hSharedMem = OpenFileMappingW(
		FILE_MAP_READ,
		FALSE,
		L"Global\\OFGal_Engine_ProjectStructureViewer_FolderViewer_OpenLevelPath"
	);
	if (m_hSharedMem != NULL) {
		m_pSharedMemView = MapViewOfFile(
			m_hSharedMem,
			FILE_MAP_READ,
			0,
			0,
			1024
		);
	}

	// 打开两个事件
	m_hEventLevelChanged = OpenEventW(
		EVENT_ALL_ACCESS,
		FALSE,
		L"Global\\OFGal_Engine_FolderViewer_LevelChanged"
	);
	m_hEventRenderPreview = OpenEventW(
		EVENT_ALL_ACCESS,
		FALSE,
		L"Global\\OFGal_Engine_DetailViewer_RenderPreviewFrame"
	);
}

LevelViewer::~LevelViewer() {
	if (m_pSharedMemView) {
		UnmapViewOfFile(m_pSharedMemView);
	}
	if (m_hSharedMem) {
		CloseHandle(m_hSharedMem);
	}
	if (m_hEventLevelChanged) {
		CloseHandle(m_hEventLevelChanged);
	}
	if (m_hEventRenderPreview) {
		CloseHandle(m_hEventRenderPreview);
	}
}

void LevelViewer::Run() {
	HANDLE events[2] = { m_hEventLevelChanged, m_hEventRenderPreview };

	while (true) {
		// 20ms 超时轮询
		DWORD result = WaitForMultipleObjects(2, events, FALSE, 20);

		if (result == WAIT_OBJECT_0) {
			// LevelChanged 事件触发：从共享内存读取路径
			if (m_pSharedMemView != nullptr) {
				const wchar_t* pView = static_cast<const wchar_t*>(m_pSharedMemView);
				// 安全读取最多 512 个 wchar_t（1024 字节）
				size_t len = wcsnlen(pView, 512);
				m_currentLevelPath.assign(pView, len);
			}
			if (!m_currentLevelPath.empty())
				m_currentLevel = ReadLevelData(WStringToString(m_currentLevelPath));
			shouldRender = true;
		}
		else if (result == WAIT_OBJECT_0 + 1) {
			// RenderPreviewFrame 事件触发
			if (!m_currentLevelPath.empty())
				m_currentLevel = ReadLevelData(WStringToString(m_currentLevelPath));
			shouldRender = true;
		}

		if (shouldRender) {
			shouldRender = false;
			ClearScreen();
			RenderingSystem::getInstance().RenderThumbnailAndPrint(m_currentLevel);
		}
	}
}

std::string LevelViewer::WStringToString(const std::wstring& wstr) {
	if (wstr.empty()) return {};
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string result(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size_needed, NULL, NULL);
	return result;
}

void LevelViewer::ClearScreen() {
	std::cout << "\x1b[2J\x1b[H" << std::flush;
}