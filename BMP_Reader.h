// Copyright 2026 MrSeagull. All Rights Reserved.
#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>

struct BMP_Pixel {
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	unsigned char alpha;
};

struct BMP_Data {
	int width;
	int height;
	std::vector<BMP_Pixel> pixels;
};

// 函数声明
BMP_Data Read_A_BMP(const char* filepath);