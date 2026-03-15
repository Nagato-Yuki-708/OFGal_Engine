#include "_EventBus.h"
#include <iostream>

void _EventBus::publish_SoundPlay(const char* sound_path)
{
    for (auto& handler : handlers_SoundPlay)
    {
        handler(sound_path);
    }
}
void _EventBus::subscribe_SoundPlay(SoundPlay_Handler handler)
{
    handlers_SoundPlay.push_back(handler);
}