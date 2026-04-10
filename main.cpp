#include <iostream>
#include "_EventBus.h"
#include "BMP_Reader.h"
#include "cmdDrawer.h"
#include "InputSystem.h"
#include<windows.h>
#include <atomic>
#include <thread>
#include"InputCollector.h"
#include "FileSystem.h"
#include "SoundSystem.h"
#include "GameVM.h"
#include "RenderingSystem.h"
#include "WindowsSystem.h"
std::atomic<bool> running(true);    //创建原子变量，表示程序是否在运行
void InputThread(InputCollector* collector) {
	while (running) {
		collector->update();
	}
	Sleep(20);
}
int main() {
	_EventBus* pEventBus = &_EventBus::getInstance();
	FileSystem* pFileSystem = &FileSystem::getInstance();
	SoundSystem* pSoundSystem = &SoundSystem::getInstance();
    WindowsSystem* pWindowsSystem = &WindowsSystem::getInstance();
	GameVM* pGameVM = &GameVM::getInstance();
	RenderingSystem* pRenderingSystem = &RenderingSystem::getInstance();

	InputSystem inputSystem;
	InputCollector collector(&inputSystem);
	std::thread inputThread(InputThread, &collector);



	OutputDebugStringA("=== main START ===\n");
	SoundSystem::getInstance().Init();
	OutputDebugStringA("=== After Init ===\n");


    OutputDebugStringA("=== About to publish PlaySoundEvent ===\n");
	

	/*重复播放测试
	PlaySoundEvent ev;
	ev.path = "C:\\Windows\\Media\\Windows Ding.wav";
	ev.loop = true;

	_EventBus::getInstance().publish_PlaySound(ev);

	auto start = std::chrono::steady_clock::now();
	while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
		SoundSystem::getInstance().update(0.05f);  // dt = 0.05 秒
		OutputDebugStringA("update called from main loop\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	*/
	


	/*暂停测试
	//1. 播放循环音频
	PlaySoundEvent evPlay;
	evPlay.path = "C:\\Windows\\Media\\flourish.mid";
	evPlay.loop = true;
	_EventBus::getInstance().publish_PlaySound(evPlay);
	OutputDebugStringA("=== Play started ===\n");

	std::this_thread::sleep_for(std::chrono::seconds(3));

	//2. 暂停音频
	PauseSoundEvent evPause;
	evPause.path = "C:\\Windows\\Media\\flourish.mid";  // 使用原始路径，而不是别名
	_EventBus::getInstance().publish_PauseSound(evPause);
	OutputDebugStringA("=== Pause event published ===\n");

	std::this_thread::sleep_for(std::chrono::seconds(3));
	*/
	


	/*多音频并行测试
	// 1. 播放长背景音乐
	PlaySoundEvent longMusic;
	longMusic.path = "C:\\Windows\\Media\\flourish.mid";
	longMusic.loop = false;
	_EventBus::getInstance().publish_PlaySound(longMusic);
	OutputDebugStringA("=== About to publish PlaySoundEvent1 ===\n");
	// 2. 播放短提示音
	PlaySoundEvent shortBeep;
	shortBeep.path = "C:\\Windows\\Media\\Windows Ding.wav";
	shortBeep.loop = false;
	_EventBus::getInstance().publish_PlaySound(shortBeep);
	OutputDebugStringA("=== About to publish PlaySoundEvent2 ===\n");
	*/



	/*停止测试
	// 播放一个循环音频，延时 5 秒后停止它
	PlaySoundEvent evPlay;
	evPlay.path = "C:\\Windows\\Media\\Windows Ding.wav";
	evPlay.loop = true;
	_EventBus::getInstance().publish_PlaySound(evPlay);
	OutputDebugStringA("=== About to publish PlaySoundEvent ===\n");

	std::this_thread::sleep_for(std::chrono::seconds(3));

	StopSoundEvent evStop;
	evStop.path = "Windows_Ding";   // 别名
	_EventBus::getInstance().publish_StopSound(evStop);
	OutputDebugStringA("=== About to publish StopSoundEvent ===\n");
	*/



	/*音量测试
	// 播放音频
	PlaySoundEvent ev;
	ev.path = "C:\\Windows\\Media\\Windows Ding.wav";
	ev.loop = true;
	_EventBus::getInstance().publish_PlaySound(ev);
	OutputDebugStringA("=== About to publish PlaySoundEvent ===\n");
	auto start = std::chrono::steady_clock::now();
	while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
		SoundSystem::getInstance().update(0.05f);  // dt = 0.05s，循环5次
		OutputDebugStringA("update called from main loop\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	// 等待2秒
	std::this_thread::sleep_for(std::chrono::seconds(2));
	// 设置音量为 10%
	SoundSystem::getInstance().SetVolume(0.1f);
	OutputDebugStringA("=== About to publish PlaySoundEvent2 ===\n");
	// 等待5秒
	std::this_thread::sleep_for(std::chrono::seconds(5));
	// 设置音量为 100%
	SoundSystem::getInstance().SetVolume(1.0f);
	OutputDebugStringA("=== About to publish PlaySoundEvent3 ===\n");
	*/


	system("pause");
	running = false;
	inputThread.join();  // 等待子线程结束

	OutputDebugStringA("=== main END ===\n");
	//std::this_thread::sleep_for(std::chrono::seconds(5));

	return 0;
}