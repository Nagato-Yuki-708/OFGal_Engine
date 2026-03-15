// Copyright 2026 MrSeagull. All Rights Reserved.
#include "Pic2Pix.cuh"
#include "BMP_Reader.h"
#include <cuda_runtime.h>

__global__ void convertBMPKernel(const uint8_t* rawData, int width, int height, int bitCount,
    int rowStride, const RGBQUAD* palette, bool topDown,
    BMP_Pixel* output) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= width * height) return;
    int y = idx / width;
    int x = idx % width;
    int destY = topDown ? y : (height - 1 - y);
    int destIdx = destY * width + x;

    int srcOffset = y * rowStride;
    BMP_Pixel pixel;
    pixel.alpha = 255;

    if (bitCount == 24) {
        int off = srcOffset + x * 3;
        pixel.blue = rawData[off];
        pixel.green = rawData[off + 1];
        pixel.red = rawData[off + 2];
    }
    else if (bitCount == 32) {
        int off = srcOffset + x * 4;
        pixel.blue = rawData[off];
        pixel.green = rawData[off + 1];
        pixel.red = rawData[off + 2];
        pixel.alpha = rawData[off + 3];
    }
    else if (bitCount == 16) {
        int off = srcOffset + x * 2;
        uint16_t val = *reinterpret_cast<const uint16_t*>(&rawData[off]);
        uint8_t r5 = (val >> 10) & 0x1F;
        uint8_t g5 = (val >> 5) & 0x1F;
        uint8_t b5 = val & 0x1F;
        pixel.red = (r5 * 255 + 15) / 31;
        pixel.green = (g5 * 255 + 15) / 31;
        pixel.blue = (b5 * 255 + 15) / 31;
    }
    else if (bitCount == 8) {
        uint8_t index = rawData[srcOffset + x];
        if (palette) {
            RGBQUAD c = palette[index];
            pixel.red = c.rgbRed;
            pixel.green = c.rgbGreen;
            pixel.blue = c.rgbBlue;
        }
    }
    else if (bitCount == 4) {
        int off = srcOffset + x / 2;
        uint8_t byte = rawData[off];
        uint8_t index = (x % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
        if (palette) {
            RGBQUAD c = palette[index];
            pixel.red = c.rgbRed;
            pixel.green = c.rgbGreen;
            pixel.blue = c.rgbBlue;
        }
    }
    else if (bitCount == 1) {
        int off = srcOffset + x / 8;
        uint8_t byte = rawData[off];
        int bitPos = 7 - (x % 8);
        uint8_t index = (byte >> bitPos) & 0x01;
        if (palette) {
            RGBQUAD c = palette[index];
            pixel.red = c.rgbRed;
            pixel.green = c.rgbGreen;
            pixel.blue = c.rgbBlue;
        }
    }
    else {
        pixel.red = pixel.green = pixel.blue = 0;
    }

    output[destIdx] = pixel;
}

extern "C" void process_Raw_BMP_On_GPU(const unsigned char* input, int width, int height,
    int bitCount, const RGBQUAD* palette, int paletteSize,
    bool topDown, BMP_Pixel* output) {
    int rowStride = ((width * bitCount + 31) / 32) * 4;
    size_t inputSize = height * rowStride;
    size_t paletteBytes = paletteSize * sizeof(RGBQUAD);
    size_t outputSize = width * height * sizeof(BMP_Pixel);

    uint8_t* d_input = nullptr;
    RGBQUAD* d_palette = nullptr;
    BMP_Pixel* d_output = nullptr;

    cudaMalloc(&d_input, inputSize);
    if (paletteSize > 0) cudaMalloc(&d_palette, paletteBytes);
    cudaMalloc(&d_output, outputSize);

    cudaMemcpy(d_input, input, inputSize, cudaMemcpyHostToDevice);
    if (paletteSize > 0) cudaMemcpy(d_palette, palette, paletteBytes, cudaMemcpyHostToDevice);

    int totalPixels = width * height;
    int blockSize = 256;
    int gridSize = (totalPixels + blockSize - 1) / blockSize;
    convertBMPKernel <<< gridSize, blockSize >>> (d_input, width, height, bitCount,
        rowStride, d_palette, topDown, d_output);
    cudaDeviceSynchronize();

    cudaMemcpy(output, d_output, outputSize, cudaMemcpyDeviceToHost);

    cudaFree(d_input);
    if (d_palette) cudaFree(d_palette);
    cudaFree(d_output);
}

