#pragma once
#include <string>

struct PlaySoundEvent {
	std::string path;
	bool loop = false;
};

struct StopSoundEvent {

};

struct SetVolumeEvent {
	float volume = 1.0f;
};