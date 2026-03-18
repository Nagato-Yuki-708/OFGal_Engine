#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include<memory>

class AudioClip;

class SoundSystem {
public:
	static SoundSystem& getInstance();
	void Init();//初始化
	void Shutdown();
	void update(float dt);//帧刷新音频
	void playSound(const std::string& path,bool loop=false);
	void StopSounds();
	void SetVolume(float volume);

private:
	SoundSystem();
	~SoundSystem();
	std::shared_ptr<AudioClip>Load(const std::string& path);
	void Play(std::shared_ptr<AudioClip>, bool loop);
	bool initialized = false;
	float master_volume = 1.0f;
	std::unordered_map<std::string, std::shared_ptr<AudioClip>> cache;


};