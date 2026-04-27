// Copyright 2026 MrSeagull. All Rights Reserved.
#include "BMP_Reader.h"
#include "Pic2Pix.cuh"
#include <fstream>
#include <iostream>
#include <algorithm>


BMP_Data Read_A_BMP(const char* filepath) {
	BMP_Data result{ 0, 0, {} };
	std::ifstream file(filepath, std::ios::binary);
	if (!file) {
		std::cerr << "Error: Failed to open file: " << filepath << std::endl;
		return result;
	}

	BITMAPFILEHEADER fileHeader;
	file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
	if (fileHeader.bfType != 0x4D42) return result;

	BITMAPINFOHEADER infoHeader;
	file.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
	if (infoHeader.biSize < sizeof(BITMAPINFOHEADER)) return result;
	if (infoHeader.biCompression != BI_RGB) return result;

	int width = infoHeader.biWidth;
	int height = abs(infoHeader.biHeight);
	bool topDown = (infoHeader.biHeight < 0);
	int bitCount = infoHeader.biBitCount;
	int rowStride = ((width * bitCount + 31) / 32) * 4;

	std::vector<RGBQUAD> palette;
	if (bitCount <= 8) {
		int colorCount = infoHeader.biClrUsed;
		if (colorCount == 0) colorCount = 1 << bitCount;
		palette.resize(colorCount);
		file.read(reinterpret_cast<char*>(palette.data()), colorCount * sizeof(RGBQUAD));
	}

	file.seekg(fileHeader.bfOffBits, std::ios::beg);
	std::streampos current = file.tellg();
	file.seekg(0, std::ios::end);
	std::streampos end = file.tellg();
	file.seekg(current, std::ios::beg);
	size_t rawDataSize = static_cast<size_t>(end - current);
	size_t expectedSize = height * rowStride;
	if (rawDataSize < expectedSize) return result;

	std::vector<uint8_t> rawData(rawDataSize);
	file.read(reinterpret_cast<char*>(rawData.data()), rawDataSize);
	if (!file) return result;

	result.width = width;
	result.height = height;
	result.pixels.resize(width * height);

	// 调用CUDA加速转换
	process_Raw_BMP_On_GPU(rawData.data(), width, height, bitCount,
		palette.empty() ? nullptr : palette.data(), static_cast<int>(palette.size()),
		topDown, result.pixels.data());

	return result;
}