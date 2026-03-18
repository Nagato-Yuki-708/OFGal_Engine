#include"SoundSystem.h"
#include"AudioClip.h"
#include"SoundEvents.h"
#include"_EventBus.h"

SoundSystem& SoundSystem::getInstance()
{
	static SoundSystem instance;
	return instance;
}
void SoundSystem::Init()
{
	if (initialized)
		return;
	_EventBus::getInstance().subscribe_SoundPlay
	([this](const char* sound_path)->bool {
		if (sound_path == nullptr || sound_path[0] == '\0')
		{
			return false;
		}this->playSound(sound_path, false);
		return true;

		});
	initialized = true;

}

void SoundSystem::Shutdown()
{
	cache.clear();
	initialized = false;
}

void SoundSystem::update(float dt)
{
	if (!initialized)
		return;
}

void SoundSystem::playSound(const std::string& path, bool loop) {
	auto clip = Load(path);
	Play(clip, loop);
};

void SoundSystem::StopSounds() {
};

void SoundSystem::SetVolume(float volume) {
	master_volume = volume;

};

std::shared_ptr<AudioClip> SoundSystem::Load(const std::string& path)
{
	
	auto it = cache.find(path);
	if (it != cache.end())
		return it->second;

	auto clip = std::make_shared<AudioClip>(path, false);
	cache[path] = clip;//判断是否缓存，没有就在AudioClip里面加载

	return clip;
}

void SoundSystem::Play(std::shared_ptr<AudioClip> clip, bool loop)
{
	if (!clip)
		return;

	clip->loop = loop;

}

SoundSystem::~SoundSystem() 
{

};
