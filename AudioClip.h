#pragma once
#include<string>

class AudioClip {
public:
	std::string path; //音频文件路径
	bool loop; //是否循环

	void* audioData; //指针调用音频

	AudioClip(const std::string& path, bool is_loop);
	~AudioClip();

};
