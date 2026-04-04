#include "_EventBus.h"
#include <iostream>

// 播放音频事件
void _EventBus::subscribe_PlaySound(PlaySound_Handler handler) {
    playHandlers.push_back(handler);
}
void _EventBus::publish_PlaySound(const PlaySoundEvent& playEvent) {
    for (auto& h : playHandlers) h(playEvent);
}

// 暂停音频事件
void _EventBus::subscribe_PauseSound(PauseSound_Handler handler) {
    pauseHandlers.push_back(handler);
}
void _EventBus::publish_PauseSound(const PauseSoundEvent& playEvent) {
    for (auto& h : pauseHandlers) h(playEvent);
}

// 停止音频事件
void _EventBus::subscribe_StopSound(StopSound_Handler handler) {
    stopHandlers.push_back(handler);
}
void _EventBus::publish_StopSound(const StopSoundEvent& playEvent) {
    for (auto& h : stopHandlers) h(playEvent);
}

// 停止所有音频
void _EventBus::subscribe_StopAllSound(StopAllSound_Handler handler) {
    stopAllHandlers.push_back(handler);
}
void _EventBus::publish_StopAllSound(const StopAllSoundEvent& playEvent) {
    for (auto& h : stopAllHandlers) h(playEvent);
}

// 设置全局音量
void _EventBus::subscribe_SetVolume(SetVolume_Handler handler) {
    volumeHandlers.push_back(handler);
}
void _EventBus::publish_SetVolume(const SetVolumeEvent& playEvent) {
    for (auto& h : volumeHandlers) h(playEvent);
}

// 设置单个音频音量
void _EventBus::subscribe_SetSoundVolume(SetSoundVolume_Handler handler) {
    soundVolumeHandlers.push_back(handler);
}
void _EventBus::publish_SetSoundVolume(const SetSoundVolumeEvent& playEvent) {
    for (auto& h : soundVolumeHandlers) h(playEvent);
}

// 设置播放速度
void _EventBus::subscribe_SetSpeed(SetSpeed_Handler handler) {
    speedHandlers.push_back(handler);
}
void _EventBus::publish_SetSpeed(const SetSpeedEvent& playEvent) {
    for (auto& h : speedHandlers) h(playEvent);
}