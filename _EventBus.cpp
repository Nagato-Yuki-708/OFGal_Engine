#include "_EventBus.h"
#include <iostream>

// 播放音频事件
void _EventBus::subscribe_PlaySound(PlaySound_Handler handler) {
    Handlers_play.push_back(handler);
}
void _EventBus::publish_PlaySound(const PlaySoundEvent& playEvent) {
    for (auto& h : Handlers_play) h(playEvent);
}

// 暂停音频事件
void _EventBus::subscribe_PauseSound(PauseSound_Handler handler) {
    Handlers_pause.push_back(handler);
}
void _EventBus::publish_PauseSound(const PauseSoundEvent& playEvent) {
    for (auto& h :Handlers_pause) h(playEvent);
}

// 停止音频事件
void _EventBus::subscribe_StopSound(StopSound_Handler handler) {
    Handlers_stop.push_back(handler);
}
void _EventBus::publish_StopSound(const StopSoundEvent& playEvent) {
    for (auto& h : Handlers_stop) h(playEvent);
}

// 停止所有音频
void _EventBus::subscribe_StopAllSound(StopAllSound_Handler handler) {
    Handlers_stopAll.push_back(handler);
}
void _EventBus::publish_StopAllSound(const StopAllSoundEvent& playEvent) {
    for (auto& h : Handlers_stopAll) h(playEvent);
}

// 设置全局音量
void _EventBus::subscribe_SetVolume(SetVolume_Handler handler) {
    Handlers_volume.push_back(handler);
}
void _EventBus::publish_SetVolume(const SetVolumeEvent& playEvent) {
    for (auto& h : Handlers_volume) h(playEvent);
}

// 设置单个音频音量
void _EventBus::subscribe_SetSoundVolume(SetSoundVolume_Handler handler) {
    Handlers_soundVolume.push_back(handler);
}
void _EventBus::publish_SetSoundVolume(const SetSoundVolumeEvent& playEvent) {
    for (auto& h : Handlers_soundVolume) h(playEvent);
}

// 设置播放速度
void _EventBus::subscribe_SetSpeed(SetSpeed_Handler handler) {
    Handlers_speed.push_back(handler);
}
void _EventBus::publish_SetSpeed(const SetSpeedEvent& playEvent) {
    for (auto& h : Handlers_speed) h(playEvent);
}