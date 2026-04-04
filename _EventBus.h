#pragma once
#include <functional>
#include <vector>
#include "SoundEvents.h"



class _EventBus {
	//使用单例模式
public:
	_EventBus(const _EventBus&) = delete;
	_EventBus& operator=(const _EventBus&) = delete;

	static _EventBus& getInstance() {
		static _EventBus instance;
		return instance;
	}

	


	using SoundPlay_Handler = std::function < bool(const char*) >;		//bool表示是否成功，char*则指向音频文件路径，视实际情况更改
	//其它事件类型，请自行添加
	
    // ===== 音频委托类型 =====
	using PlaySound_Handler = std::function<void(const PlaySoundEvent&)>;
	using PauseSound_Handler = std::function<void(const PauseSoundEvent&)>;
	using StopSound_Handler = std::function<void(const StopSoundEvent&)>;
	using StopAllSound_Handler = std::function<void(const StopAllSoundEvent&)>;
	using SetVolume_Handler = std::function<void(const SetVolumeEvent&)>;
	using SetSoundVolume_Handler = std::function<void(const SetSoundVolumeEvent&)>;
	using SetSpeed_Handler = std::function<void(const SetSpeedEvent&)>;
	
	// ===== 音频订阅 =====
	void subscribe_PlaySound(PlaySound_Handler handler);
	void subscribe_PauseSound(PauseSound_Handler handler);
	void subscribe_StopSound(StopSound_Handler handler);
	void subscribe_StopAllSound(StopAllSound_Handler handler);
	void subscribe_SetVolume(SetVolume_Handler handler);
	void subscribe_SetSoundVolume(SetSoundVolume_Handler handler);
	void subscribe_SetSpeed(SetSpeed_Handler handler);
	
	// ===== 音频发布 =====
	void publish_PlaySound(const PlaySoundEvent& playEvent);
	void publish_PauseSound(const PauseSoundEvent& playEvent);
	void publish_StopSound(const StopSoundEvent& playEvent);
	void publish_StopAllSound(const StopAllSoundEvent& playEvent);
	void publish_SetVolume(const SetVolumeEvent& playEvent);
	void publish_SetSoundVolume(const SetSoundVolumeEvent& playEvent);
	void publish_SetSpeed(const SetSpeedEvent& playEvent);


	// 其它事件订阅和发布函数，请像上面两个函数一样自行添加

private:
	_EventBus() = default;
	~_EventBus() = default;

	// ===== 委托容器 =====
	std::vector<PlaySound_Handler> playHandlers;
	std::vector<PauseSound_Handler> pauseHandlers;
	std::vector<StopSound_Handler> stopHandlers;
	std::vector<StopAllSound_Handler> stopAllHandlers;
	std::vector<SetVolume_Handler> volumeHandlers;
	std::vector<SetSoundVolume_Handler> soundVolumeHandlers;
	std::vector<SetSpeed_Handler> speedHandlers;

	std::vector<SoundPlay_Handler> handlers_SoundPlay;  // 所有播放音频事件订阅者，按理来说只有 音频系统 会订阅
	// 其它订阅者类型，请像上面一样自行添加
};