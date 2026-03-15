// Copyright 2026 MrSeagull. All Rights Reserved.
#pragma once
#include <windows.h>  // for RGBQUAD

struct BMP_Pixel; // 前向声明

#ifdef __cplusplus
extern "C" {
#endif

    void process_Raw_BMP_On_GPU(const unsigned char* input, int width, int height, int bitCount,
        const RGBQUAD* palette, int paletteSize, bool topDown,
        BMP_Pixel* output);

#ifdef __cplusplus
}
#endif