#include "SoundSystem.h"
#include "SharedTypes.h"
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

SoundSystem::SoundSystem() {}

SoundSystem::~SoundSystem() {
    Shutdown();
}

void SoundSystem::Init() {
    if (initialized) return;
    initialized = true;
}

void SoundSystem::Shutdown() {
    stopAllSounds();
    initialized = false;
}

void SoundSystem::update(float dt) {}
// 工具函数：取文件名（无后缀）
static std::string GetName(const std::string& path)
{
    size_t slash = path.find_last_of("\\/");
    size_t dot = path.find_last_of(".");
    return path.substr(slash + 1, dot - slash - 1);
}


// 播放

void SoundSystem::playSound(const std::string& path, bool loop)
{
    std::string name = GetName(path);

    if (播放集合.find(name) != 播放集合.end())
        return;

    AudioClip clip(path, loop);
    clip.volume = global_volumn;
    clip.isPaused = false;

    播放集合[name] = clip;

    std::string cmd = "open \"" + path + "\" alias " + name;
    mciSendStringA(cmd.c_str(), NULL, 0, NULL);

    if (loop)
        cmd = "play " + name + " repeat";
    else
        cmd = "play " + name;

    mciSendStringA(cmd.c_str(), NULL, 0, NULL);

    OutputDebugStringA(("Play: " + name + "\n").c_str());
}

// 暂停
void SoundSystem::pauseSound(const std::string& name)
{
    auto it = 播放集合.find(name);
    if (it == 播放集合.end())
    {
        throw std::runtime_error("Audio not found");
    }

    std::string cmd = "pause " + name;
    mciSendStringA(cmd.c_str(), NULL, 0, NULL);

    it->second.isPaused = true;

    OutputDebugStringA(("Pause: " + name + "\n").c_str());
}


// 停止
void SoundSystem::stopSound(const std::string& name)
{
    auto it = 播放集合.find(name);
    if (it == 播放集合.end())
        return;

    std::string cmd = "stop " + name;
    mciSendStringA(cmd.c_str(), NULL, 0, NULL);

    cmd = "close " + name;
    mciSendStringA(cmd.c_str(), NULL, 0, NULL);

    播放集合.erase(it);

    OutputDebugStringA(("Stop: " + name + "\n").c_str());
}

// 停止全部
void SoundSystem::stopAllSounds()
{
    for (auto& pair : 播放集合)
    {
        std::string name = pair.first;

        std::string cmd = "stop " + name;
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);

        cmd = "close " + name;
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);
    }

    播放集合.clear();

    OutputDebugStringA("Stop All Sounds\n");
}

// 全局音量

void SoundSystem::SetVolume(float volume)
{
    global_volumn = volume;

    for (auto& pair : 播放集合)
    {
        pair.second.volume = volume;
    }

    OutputDebugStringA("Set Global Volume\n");
}

// 单个音量
void SoundSystem::SetSoundVolume(const std::string& name, float volume)
{
    auto it = 播放集合.find(name);
    if (it == 播放集合.end())
    {
        throw std::runtime_error("Audio not found");
    }

    it->second.volume = volume;

    OutputDebugStringA(("Set Volume: " + name + "\n").c_str());
}


// 清空集合

void SoundSystem::clearAllSounds()
{
    stopAllSounds();
}

// Load（目前简单构造）

std::shared_ptr<AudioClip> SoundSystem::Load(const std::string& path)
{
    return std::make_shared<AudioClip>(path, false);
}


// MCI 播放
void SoundSystem::MCIPlay(const std::string& path, bool loop)
{
    std::string name = GetName(path);

    std::string cmd = "open \"" + path + "\" alias " + name;
    mciSendStringA(cmd.c_str(), NULL, 0, NULL);

    if (loop)
        cmd = "play " + name + " repeat";
    else
        cmd = "play " + name;

    mciSendStringA(cmd.c_str(), NULL, 0, NULL);
}

// MCI 停止
void SoundSystem::MCIStop(const std::string& path)
{
    std::string name = GetName(path);

    std::string cmd = "stop " + name;
    mciSendStringA(cmd.c_str(), NULL, 0, NULL);

    cmd = "close " + name;
    mciSendStringA(cmd.c_str(), NULL, 0, NULL);
}