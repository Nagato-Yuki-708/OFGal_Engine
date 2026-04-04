#pragma once
#include <string>

// 播放音频事件
struct PlaySoundEvent {
    std::string path;
    bool loop = false;
};

// 暂停音频事件
struct PauseSoundEvent {
    std::string path;
};

// 停止音频事件
struct StopSoundEvent {
    std::string path;
};

// 停止所有音频
struct StopAllSoundEvent {};

// 设置全局音量
struct SetVolumeEvent {
    float volume = 1.0f;
};

// 设置单个音频音量
struct SetSoundVolumeEvent {
    std::string path;
    float volume = 1.0f;
};

// 设置播放速度
struct SetSpeedEvent {
    std::string path;
    float speed = 1.0f;
};