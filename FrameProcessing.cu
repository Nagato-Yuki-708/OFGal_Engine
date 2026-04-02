// Copyright 2026 MrSeagull. All Rights Reserved.
#include "FrameProcessing.cuh"
#include <cuda_runtime.h>
#include <math.h>
#include <algorithm>

// ----------------------------- 辅助函数 -----------------------------

// 计算感知亮度 (0~255)
__device__ __host__ inline float luminance(unsigned char r, unsigned char g, unsigned char b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

// 将 StdPixel 转换为 float3 (范围 0~1)
__device__ __host__ inline float3 pixelToFloat3(const StdPixel& p) {
    return make_float3(p.red / 255.0f, p.green / 255.0f, p.blue / 255.0f);
}

// 将 float3 转换为 StdPixel (clamp 0~1)
__device__ __host__ inline StdPixel float3ToPixel(const float3& v) {
    return {
        static_cast<unsigned char>(fminf(fmaxf(v.x * 255.0f, 0.0f), 255.0f)),
        static_cast<unsigned char>(fminf(fmaxf(v.y * 255.0f, 0.0f), 255.0f)),
        static_cast<unsigned char>(fminf(fmaxf(v.z * 255.0f, 0.0f), 255.0f))
    };
}

// 主机端生成一维高斯核 (大小 2*radius+1)
std::vector<float> makeGaussianKernel(int radius, float sigma) {
    if (sigma <= 0.0f) sigma = radius / 3.0f;
    std::vector<float> kernel(2 * radius + 1);
    float sum = 0.0f;
    for (int i = -radius; i <= radius; ++i) {
        float x = static_cast<float>(i);
        kernel[i + radius] = expf(-(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i + radius];
    }
    for (float& v : kernel) v /= sum;
    return kernel;
}

// ---------------------------- Bloom 核函数 -----------------------------

// 1. 提取高亮区域 (Bright Pass)
// 输入: src (float3 0~1) , 输出: dst (float3)
// 阈值 thres 为亮度值 (0~1)，内部先将 RGB 转亮度再比较
__global__ void brightPassKernel(const float3* src, float3* dst, int width, int height, float thres) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    int idx = y * width + x;
    float3 color = src[idx];
    float lum = 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;
    dst[idx] = (lum > thres) ? color : make_float3(0.0f, 0.0f, 0.0f);
}

// 2. 水平高斯模糊 (简单全局内存版本，适合半径 <= 15)
// 输入: src, 输出: dst, kernel: 一维权重数组, radius: 半径
__global__ void gaussianBlurHorizontal(const float3* src, float3* dst,
    int width, int height,
    const float* kernel, int radius) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    float3 sum = make_float3(0.0f, 0.0f, 0.0f);
    int base = y * width;
    for (int k = -radius; k <= radius; ++k) {
        int nx = min(max(x + k, 0), width - 1);
        sum.x += src[base + nx].x * kernel[k + radius];
        sum.y += src[base + nx].y * kernel[k + radius];
        sum.z += src[base + nx].z * kernel[k + radius];
    }
    dst[base + x] = sum;
}

// 3. 垂直高斯模糊
__global__ void gaussianBlurVertical(const float3* src, float3* dst,
    int width, int height,
    const float* kernel, int radius) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    float3 sum = make_float3(0.0f, 0.0f, 0.0f);
    for (int k = -radius; k <= radius; ++k) {
        int ny = min(max(y + k, 0), height - 1);
        int idx = ny * width + x;
        sum.x += src[idx].x * kernel[k + radius];
        sum.y += src[idx].y * kernel[k + radius];
        sum.z += src[idx].z * kernel[k + radius];
    }
    dst[y * width + x] = sum;
}

// 4. 合成：原图 + 强度 * 泛光图，并 clamp
__global__ void composeKernel(const float3* original, const float3* bloom,
    float3* output, int width, int height, float intensity) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    int idx = y * width + x;
    float3 orig = original[idx];
    float3 blo = bloom[idx];
    output[idx] = make_float3(
        fminf(orig.x + blo.x * intensity, 1.0f),
        fminf(orig.y + blo.y * intensity, 1.0f),
        fminf(orig.z + blo.z * intensity, 1.0f)
    );
}

// ----------------------------- 主机端泛光函数 -----------------------------
void applyBloom(Frame& frame, float threshold, float intensity, int blurRadius, float sigma) {
    if (frame.width <= 0 || frame.height <= 0 || frame.pixels.empty())
        return;

    // 参数钳制
    threshold = fminf(fmaxf(threshold, 0.0f), 255.0f);
    intensity = fmaxf(intensity, 0.0f);
    blurRadius = min(max(blurRadius, 1), 15);   // 限制半径最大 15，避免性能过差
    if (sigma <= 0.0f) sigma = blurRadius / 3.0f;

    int totalPixels = frame.width * frame.height;
    size_t imgSize = totalPixels * sizeof(float3);

    // 1. 分配设备内存
    float3* d_image = nullptr, * d_bright = nullptr, * d_temp = nullptr, * d_output = nullptr;
    cudaMalloc(&d_image, imgSize);
    cudaMalloc(&d_bright, imgSize);
    cudaMalloc(&d_temp, imgSize);
    cudaMalloc(&d_output, imgSize);
    if (!d_image || !d_bright || !d_temp || !d_output) {
        // 内存分配失败处理
        cudaFree(d_image); cudaFree(d_bright); cudaFree(d_temp); cudaFree(d_output);
        return;
    }

    // 2. 将主机像素转换为 float3 并拷贝到设备
    std::vector<float3> h_image(totalPixels);
    for (int i = 0; i < totalPixels; ++i) {
        h_image[i] = pixelToFloat3(frame.pixels[i]);
    }
    cudaMemcpy(d_image, h_image.data(), imgSize, cudaMemcpyHostToDevice);

    // 3. 提取高亮 (阈值归一化到 0~1)
    float normThres = threshold / 255.0f;
    dim3 block(16, 16);
    dim3 grid((frame.width + block.x - 1) / block.x, (frame.height + block.y - 1) / block.y);
    brightPassKernel <<< grid, block >>> (d_image, d_bright, frame.width, frame.height, normThres);

    // 4. 高斯模糊 (水平 + 垂直)
    std::vector<float> kernel = makeGaussianKernel(blurRadius, sigma);
    float* d_kernel = nullptr;
    cudaMalloc(&d_kernel, kernel.size() * sizeof(float));
    cudaMemcpy(d_kernel, kernel.data(), kernel.size() * sizeof(float), cudaMemcpyHostToDevice);

    // 水平模糊: d_bright -> d_temp
    gaussianBlurHorizontal <<< grid, block >>> (d_bright, d_temp, frame.width, frame.height, d_kernel, blurRadius);
    // 垂直模糊: d_temp -> d_bright
    gaussianBlurVertical <<< grid, block >>> (d_temp, d_bright, frame.width, frame.height, d_kernel, blurRadius);

    cudaFree(d_kernel);

    // 5. 合成: d_image + intensity * d_bright -> d_output
    composeKernel <<< grid, block >>> (d_image, d_bright, d_output, frame.width, frame.height, intensity);

    // 6. 拷贝回主机并转换回 StdPixel
    std::vector<float3> h_output(totalPixels);
    cudaMemcpy(h_output.data(), d_output, imgSize, cudaMemcpyDeviceToHost);
    for (int i = 0; i < totalPixels; ++i) {
        frame.pixels[i] = float3ToPixel(h_output[i]);
    }

    // 7. 释放设备内存
    cudaFree(d_image);
    cudaFree(d_bright);
    cudaFree(d_temp);
    cudaFree(d_output);
}

// ----------------------------- FXAA 核函数 -----------------------------

// 获取像素亮度（0~1）
__device__ inline float getLuma(const float3& color) {
    return 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;
}

// 从纹理（全局内存）中安全读取 float3
__device__ inline float3 texFetch(const float3* img, int x, int y, int width, int height) {
    x = min(max(x, 0), width - 1);
    y = min(max(y, 0), height - 1);
    return img[y * width + x];
}

// FXAA 核心实现（每个像素）
__global__ void fxaaKernel(const float3* input, float3* output,
    int width, int height,
    float edgeThreshold, float edgeThresholdMin,
    float spanMax, float reduceMul, float reduceMin) {

    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    int idx = y * width + x;
    float3 center = input[idx];
    float lumaCenter = getLuma(center);

    // 获取相邻像素亮度
    float lumaNW = getLuma(texFetch(input, x - 1, y - 1, width, height));
    float lumaNE = getLuma(texFetch(input, x + 1, y - 1, width, height));
    float lumaSW = getLuma(texFetch(input, x - 1, y + 1, width, height));
    float lumaSE = getLuma(texFetch(input, x + 1, y + 1, width, height));
    float lumaM = lumaCenter;
    float lumaN = getLuma(texFetch(input, x, y - 1, width, height));
    float lumaS = getLuma(texFetch(input, x, y + 1, width, height));
    float lumaE = getLuma(texFetch(input, x + 1, y, width, height));
    float lumaW = getLuma(texFetch(input, x - 1, y, width, height));

    // 计算最大和最小亮度（用于边缘检测）
    float lumaMin = fminf(lumaCenter,
        fminf(fminf(lumaN, lumaW), fminf(lumaS, lumaE)));
    float lumaMax = fmaxf(lumaCenter,
        fmaxf(fmaxf(lumaN, lumaW), fmaxf(lumaS, lumaE)));

    // 如果局部对比度小于阈值，直接输出原色（平坦区域）
    float lumaRange = lumaMax - lumaMin;
    if (lumaRange < max(edgeThresholdMin, edgeThreshold)) {
        output[idx] = center;
        return;
    }

    // 计算水平方向和垂直方向的梯度
    float luma1 = lumaN + lumaS;
    float luma2 = lumaE + lumaW;
    float luma3 = lumaNW + lumaSE;
    float luma4 = lumaNE + lumaSW;

    float gradHoriz = fabsf(luma2 - luma1 * 0.5f) +
        fabsf(luma4 - luma3 * 0.5f) +
        fabsf(lumaE - lumaW) * 0.5f;
    float gradVert = fabsf(luma1 - luma2 * 0.5f) +
        fabsf(luma3 - luma4 * 0.5f) +
        fabsf(lumaN - lumaS) * 0.5f;

    // 选择边缘方向（水平或垂直）
    bool isHorizontal = gradHoriz >= gradVert;
    float lumaPos, lumaNeg;
    float stepScale;
    if (isHorizontal) {
        // 水平边缘：上下搜索
        stepScale = 1.0f;
        lumaPos = lumaN;
        lumaNeg = lumaS;
    }
    else {
        // 垂直边缘：左右搜索
        stepScale = 1.0f;
        lumaPos = lumaE;
        lumaNeg = lumaW;
    }

    // 计算搜索步长（基于相邻亮度差）
    float lumaDelta = lumaPos - lumaNeg;
    float stepLength = fmaxf(reduceMin, reduceMul * lumaDelta);
    float step = stepScale * stepLength;

    // 沿边缘方向搜索（双向）
    float lumaEndPos = lumaCenter;
    float lumaEndNeg = lumaCenter;
    float stepCountPos = 1.0f;
    float stepCountNeg = 1.0f;

    // 正向搜索
    for (int i = 1; i <= (int)spanMax; ++i) {
        float offset = step * i;
        float luma;
        if (isHorizontal) {
            luma = getLuma(texFetch(input, x, y - offset, width, height));
        }
        else {
            luma = getLuma(texFetch(input, x - offset, y, width, height));
        }
        if (fabsf(luma - lumaCenter) >= lumaDelta) break;
        lumaEndPos = luma;
        stepCountPos = i;
    }
    // 反向搜索
    for (int i = 1; i <= (int)spanMax; ++i) {
        float offset = step * i;
        float luma;
        if (isHorizontal) {
            luma = getLuma(texFetch(input, x, y + offset, width, height));
        }
        else {
            luma = getLuma(texFetch(input, x + offset, y, width, height));
        }
        if (fabsf(luma - lumaCenter) >= lumaDelta) break;
        lumaEndNeg = luma;
        stepCountNeg = i;
    }

    // 计算混合比例（基于两端亮度差异）
    float lumaEndMin = fminf(lumaEndPos, lumaEndNeg);
    float lumaEndMax = fmaxf(lumaEndPos, lumaEndNeg);
    float edgeDir = (lumaEndMax - lumaEndMin) / (2.0f * lumaDelta);
    float blendFactor = 0.5f - edgeDir;

    // 限制混合比例范围
    blendFactor = fminf(fmaxf(blendFactor, 0.0f), 1.0f);

    // 沿边缘方向进行颜色混合
    float3 avgColor;
    if (isHorizontal) {
        // 上下采样
        float3 up = texFetch(input, x, y - 1, width, height);
        float3 down = texFetch(input, x, y + 1, width, height);
        avgColor = make_float3(
            (up.x + down.x) * 0.5f,
            (up.y + down.y) * 0.5f,
            (up.z + down.z) * 0.5f
        );
    }
    else {
        // 左右采样
        float3 left = texFetch(input, x - 1, y, width, height);
        float3 right = texFetch(input, x + 1, y, width, height);
        avgColor = make_float3(
            (left.x + right.x) * 0.5f,
            (left.y + right.y) * 0.5f,
            (left.z + right.z) * 0.5f
        );
    }

    // 最终颜色 = 原色 * (1 - blendFactor) + 平均色 * blendFactor
    float3 finalColor;
    finalColor.x = center.x * (1.0f - blendFactor) + avgColor.x * blendFactor;
    finalColor.y = center.y * (1.0f - blendFactor) + avgColor.y * blendFactor;
    finalColor.z = center.z * (1.0f - blendFactor) + avgColor.z * blendFactor;

    output[idx] = finalColor;
}

// ----------------------------- 主机端 FXAA 函数 -----------------------------
void applyFXAA(Frame& frame,
    float edgeThreshold, float edgeThresholdMin,
    float spanMax, float reduceMul, float reduceMin) {

    if (frame.width <= 0 || frame.height <= 0 || frame.pixels.empty())
        return;

    // 参数钳制
    edgeThreshold = fminf(fmaxf(edgeThreshold, 0.0f), 1.0f);
    edgeThresholdMin = fminf(fmaxf(edgeThresholdMin, 0.0f), 1.0f);
    spanMax = fminf(fmaxf(spanMax, 2.0f), 16.0f);   // 限制搜索范围
    reduceMul = fmaxf(reduceMul, 0.0f);
    reduceMin = fmaxf(reduceMin, 0.0f);

    int totalPixels = frame.width * frame.height;
    size_t imgSize = totalPixels * sizeof(float3);

    // 分配设备内存
    float3* d_input = nullptr, * d_output = nullptr;
    cudaMalloc(&d_input, imgSize);
    cudaMalloc(&d_output, imgSize);
    if (!d_input || !d_output) {
        cudaFree(d_input); cudaFree(d_output);
        return;
    }

    // 主机端转换为 float3
    std::vector<float3> h_image(totalPixels);
    for (int i = 0; i < totalPixels; ++i) {
        h_image[i] = pixelToFloat3(frame.pixels[i]);
    }
    cudaMemcpy(d_input, h_image.data(), imgSize, cudaMemcpyHostToDevice);

    // 启动核函数
    dim3 block(16, 16);
    dim3 grid((frame.width + block.x - 1) / block.x, (frame.height + block.y - 1) / block.y);
    fxaaKernel <<< grid, block >>> (d_input, d_output,
        frame.width, frame.height,
        edgeThreshold, edgeThresholdMin,
        spanMax, reduceMul, reduceMin);

    // 拷贝回主机并转换回 StdPixel
    std::vector<float3> h_output(totalPixels);
    cudaMemcpy(h_output.data(), d_output, imgSize, cudaMemcpyDeviceToHost);
    for (int i = 0; i < totalPixels; ++i) {
        frame.pixels[i] = float3ToPixel(h_output[i]);
    }

    cudaFree(d_input);
    cudaFree(d_output);
    cudaDeviceSynchronize();
}

// ----------------------------- SMAA 辅助函数 -----------------------------

// 获取像素亮度（0~1）
// 获取颜色（float3）的亮度梯度（用于边缘检测）
__device__ inline float getLumaGradient(const float3& c1, const float3& c2) {
    return fabsf(getLuma(c1) - getLuma(c2));
}

// 双线性插值权重计算（用于最终颜色混合）
__device__ inline float2 getBilinearWeight(float d, float x, float y) {
    float2 w = make_float2(0.0f, 0.0f);
    if (d > 0.0f) {
        w.x = x / d;
        w.y = y / d;
    }
    return w;
}

// ----------------------------- SMAA 核函数 -----------------------------

__global__ void smaaKernel(const float3* input, float3* output,
    int width, int height,
    float edgeThreshold, int maxSearchSteps, bool enableDiag) {

    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    int idx = y * width + x;
    float3 center = input[idx];
    float lumaCenter = getLuma(center);

    // 获取周围四个方向的亮度
    float lumaN = getLuma(texFetch(input, x, y - 1, width, height));
    float lumaS = getLuma(texFetch(input, x, y + 1, width, height));
    float lumaE = getLuma(texFetch(input, x + 1, y, width, height));
    float lumaW = getLuma(texFetch(input, x - 1, y, width, height));

    // 计算水平、垂直方向梯度
    float gradH = fabsf(lumaE - lumaW);
    float gradV = fabsf(lumaN - lumaS);

    // 边缘检测阈值：梯度超过阈值则认为存在边缘
    float threshold = edgeThreshold;  // 范围 0~1，实际使用时缩放
    bool isHorizontalEdge = gradH >= gradV && gradH > threshold;
    bool isVerticalEdge = gradV >= gradH && gradV > threshold;

    // 如果没有明显边缘，直接输出原色
    if (!isHorizontalEdge && !isVerticalEdge) {
        output[idx] = center;
        return;
    }

    // 选择边缘方向
    bool horizontal = isHorizontalEdge;  // 水平边缘（上下方向混合）
    float lumaPos, lumaNeg;
    int posOffsetX = 0, posOffsetY = 0, negOffsetX = 0, negOffsetY = 0;
    if (horizontal) {
        lumaPos = lumaN;
        lumaNeg = lumaS;
        posOffsetY = -1;
        negOffsetY = 1;
    }
    else {
        lumaPos = lumaE;
        lumaNeg = lumaW;
        posOffsetX = 1;
        negOffsetX = -1;
    }

    // 计算步长（基于局部对比度自适应）
    float lumaDelta = fabsf(lumaPos - lumaNeg);
    float stepLength = fmaxf(0.125f, lumaDelta * 0.25f);  // 步长调整
    float step = horizontal ? stepLength : stepLength;

    // 沿边缘方向搜索（寻找最佳混合点）
    float bestBlend = 0.5f;
    int steps = 0;
    for (int i = 1; i <= maxSearchSteps; ++i) {
        float offset = step * i;
        float luma;
        if (horizontal) {
            luma = getLuma(texFetch(input, x, y + (offset > 0 ? offset : -offset), width, height));
        }
        else {
            luma = getLuma(texFetch(input, x + (offset > 0 ? offset : -offset), y, width, height));
        }
        float diff = fabsf(luma - lumaCenter);
        if (diff >= lumaDelta) {
            bestBlend = (float)i / (float)(i + 1);
            break;
        }
        steps = i;
    }

    // 可选：对角线模式检测（增强边缘）
    if (enableDiag) {
        float lumaNW = getLuma(texFetch(input, x - 1, y - 1, width, height));
        float lumaNE = getLuma(texFetch(input, x + 1, y - 1, width, height));
        float lumaSW = getLuma(texFetch(input, x - 1, y + 1, width, height));
        float lumaSE = getLuma(texFetch(input, x + 1, y + 1, width, height));
        // 简单对角线混合权重
        float diagWeight = 0.0f;
        if (horizontal) {
            float diagDiffN = fabsf(lumaNW - lumaNE);
            float diagDiffS = fabsf(lumaSW - lumaSE);
            diagWeight = (diagDiffN + diagDiffS) * 0.5f;
        }
        else {
            float diagDiffW = fabsf(lumaNW - lumaSW);
            float diagDiffE = fabsf(lumaNE - lumaSE);
            diagWeight = (diagDiffW + diagDiffE) * 0.5f;
        }
        bestBlend = fminf(fmaxf(bestBlend + diagWeight * 0.1f, 0.0f), 1.0f);
    }

    // 最终颜色混合：沿边缘方向采样两个相邻像素
    float3 samplePos, sampleNeg;
    if (horizontal) {
        samplePos = texFetch(input, x, y - 1, width, height);
        sampleNeg = texFetch(input, x, y + 1, width, height);
    }
    else {
        samplePos = texFetch(input, x + 1, y, width, height);
        sampleNeg = texFetch(input, x - 1, y, width, height);
    }

    float3 finalColor;
    finalColor.x = center.x * (1.0f - bestBlend) + (samplePos.x + sampleNeg.x) * 0.5f * bestBlend;
    finalColor.y = center.y * (1.0f - bestBlend) + (samplePos.y + sampleNeg.y) * 0.5f * bestBlend;
    finalColor.z = center.z * (1.0f - bestBlend) + (samplePos.z + sampleNeg.z) * 0.5f * bestBlend;

    output[idx] = finalColor;
}

// ----------------------------- 主机端 SMAA 函数 -----------------------------
void applySMAA(Frame& frame, float edgeThreshold, int maxSearchSteps, bool enableDiag) {
    if (frame.width <= 0 || frame.height <= 0 || frame.pixels.empty())
        return;

    // 参数钳制
    edgeThreshold = fminf(fmaxf(edgeThreshold, 0.001f), 1.0f);
    maxSearchSteps = min(max(maxSearchSteps, 1), 8);  // 限制最大步数

    int totalPixels = frame.width * frame.height;
    size_t imgSize = totalPixels * sizeof(float3);

    // 分配设备内存
    float3* d_input = nullptr;
    float3* d_output = nullptr;
    cudaMalloc(&d_input, imgSize);
    cudaMalloc(&d_output, imgSize);
    if (!d_input || !d_output) {
        cudaFree(d_input); cudaFree(d_output);
        return;
    }

    // 主机端转换为 float3
    std::vector<float3> h_image(totalPixels);
    for (int i = 0; i < totalPixels; ++i) {
        h_image[i] = pixelToFloat3(frame.pixels[i]);
    }
    cudaMemcpy(d_input, h_image.data(), imgSize, cudaMemcpyHostToDevice);

    // 启动核函数
    dim3 block(16, 16);
    dim3 grid((frame.width + block.x - 1) / block.x, (frame.height + block.y - 1) / block.y);
    smaaKernel <<< grid, block >>> (d_input, d_output,
        frame.width, frame.height,
        edgeThreshold, maxSearchSteps, enableDiag);

    // 拷贝回主机并转换回 StdPixel
    std::vector<float3> h_output(totalPixels);
    cudaMemcpy(h_output.data(), d_output, imgSize, cudaMemcpyDeviceToHost);
    for (int i = 0; i < totalPixels; ++i) {
        frame.pixels[i] = float3ToPixel(h_output[i]);
    }

    cudaFree(d_input);
    cudaFree(d_output);
    cudaDeviceSynchronize();
}

// 双线性采样：从 float3 图像中根据浮点坐标采样，边界 clamp
__device__ inline float3 bilinearSample(const float3* img, float x, float y, int width, int height) {
    // 边界 clamp
    x = fmaxf(0.0f, fminf(x, width - 1.0f));
    y = fmaxf(0.0f, fminf(y, height - 1.0f));

    int x0 = (int)x;
    int y0 = (int)y;
    int x1 = min(x0 + 1, width - 1);
    int y1 = min(y0 + 1, height - 1);

    float fx = x - x0;
    float fy = y - y0;
    float fx1 = 1.0f - fx;
    float fy1 = 1.0f - fy;

    float3 v00 = img[y0 * width + x0];
    float3 v10 = img[y0 * width + x1];
    float3 v01 = img[y1 * width + x0];
    float3 v11 = img[y1 * width + x1];

    float3 result;
    result.x = (v00.x * fx1 + v10.x * fx) * fy1 + (v01.x * fx1 + v11.x * fx) * fy;
    result.y = (v00.y * fx1 + v10.y * fx) * fy1 + (v01.y * fx1 + v11.y * fx) * fy;
    result.z = (v00.z * fx1 + v10.z * fx) * fy1 + (v01.z * fx1 + v11.z * fx) * fy;
    return result;
}
__global__ void lensDistortionKernel(const float3* input, float3* output,
    int width, int height,
    float strength, float centerX, float centerY) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    // 将像素坐标转换为归一化 UV (0~1)
    float u = (float)x / (width - 1);
    float v = (float)y / (height - 1);

    // 计算相对于畸变中心的偏移
    float du = u - centerX;
    float dv = v - centerY;
    float r2 = du * du + dv * dv;
    // 畸变因子: 1 + strength * r2   (strength>0 桶形，<0 枕形)
    float factor = 1.0f + strength * r2;

    // 畸变后的 UV
    float u2 = centerX + du * factor;
    float v2 = centerY + dv * factor;

    // 映射回像素坐标
    float srcX = u2 * (width - 1);
    float srcY = v2 * (height - 1);

    // 双线性采样
    float3 color = bilinearSample(input, srcX, srcY, width, height);
    output[y * width + x] = color;
}
void applyLensDistortion(Frame& frame, float strength, float centerX, float centerY) {
    if (frame.width <= 0 || frame.height <= 0 || frame.pixels.empty())
        return;

    // 参数钳制
    strength = fmaxf(-0.5f, fminf(strength, 0.5f));  // 限制范围，避免过度拉伸
    centerX = fmaxf(0.0f, fminf(centerX, 1.0f));
    centerY = fmaxf(0.0f, fminf(centerY, 1.0f));

    int totalPixels = frame.width * frame.height;
    size_t imgSize = totalPixels * sizeof(float3);

    // 分配设备内存
    float3* d_input = nullptr;
    float3* d_output = nullptr;
    cudaMalloc(&d_input, imgSize);
    cudaMalloc(&d_output, imgSize);
    if (!d_input || !d_output) {
        cudaFree(d_input); cudaFree(d_output);
        return;
    }

    // 主机端转换为 float3
    std::vector<float3> h_image(totalPixels);
    for (int i = 0; i < totalPixels; ++i) {
        h_image[i] = pixelToFloat3(frame.pixels[i]);
    }
    cudaMemcpy(d_input, h_image.data(), imgSize, cudaMemcpyHostToDevice);

    // 启动核函数
    dim3 block(16, 16);
    dim3 grid((frame.width + block.x - 1) / block.x, (frame.height + block.y - 1) / block.y);
    lensDistortionKernel <<< grid, block >>> (d_input, d_output,
        frame.width, frame.height,
        strength, centerX, centerY);

    // 拷贝回主机并转换回 StdPixel
    std::vector<float3> h_output(totalPixels);
    cudaMemcpy(h_output.data(), d_output, imgSize, cudaMemcpyDeviceToHost);
    for (int i = 0; i < totalPixels; ++i) {
        frame.pixels[i] = float3ToPixel(h_output[i]);
    }

    cudaFree(d_input);
    cudaFree(d_output);
    cudaDeviceSynchronize();
}

// ----------------------------- 色差核函数 -----------------------------

__global__ void chromaticAberrationKernel(const float3* input, float3* output,
    int width, int height,
    float strength, int mode,
    float centerX, float centerY) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    // 当前像素坐标
    float px = (float)x;
    float py = (float)y;

    float offsetR, offsetB;
    if (mode == 0) {
        // 径向模式：偏移量从中心向外辐射
        float cx = centerX * (width - 1);
        float cy = centerY * (height - 1);
        float dx = px - cx;
        float dy = py - cy;
        float dist = sqrtf(dx * dx + dy * dy);
        float maxDist = sqrtf(cx * cx + cy * cy);  // 最大可能距离
        float factor = dist / maxDist;             // 0 在中心，1 在角落
        offsetR = strength * factor;
        offsetB = -strength * factor;
    }
    else {
        // 水平模式：所有像素沿水平方向偏移相同量
        offsetR = strength;
        offsetB = -strength;
    }

    // 红通道采样位置（向右偏移）
    float srcXR = px + offsetR;
    float srcYR = py;
    // 蓝通道采样位置（向左偏移）
    float srcXB = px + offsetB;
    float srcYB = py;
    // 绿通道保持原位置
    float srcXG = px;
    float srcYG = py;

    // 双线性采样
    float3 colorR = bilinearSample(input, srcXR, srcYR, width, height);
    float3 colorG = bilinearSample(input, srcXG, srcYG, width, height);
    float3 colorB = bilinearSample(input, srcXB, srcYB, width, height);

    // 组合：R 取自红色采样，G 取自绿色采样，B 取自蓝色采样
    float3 result;
    result.x = colorR.x;
    result.y = colorG.y;
    result.z = colorB.z;

    output[y * width + x] = result;
}

// ----------------------------- 主机端色差函数 -----------------------------
void applyChromaticAberration(Frame& frame, float strength, int mode, float centerX, float centerY) {
    if (frame.width <= 0 || frame.height <= 0 || frame.pixels.empty())
        return;

    // 参数钳制
    strength = fmaxf(0.0f, fminf(strength, 20.0f));   // 限制最大偏移 20 像素
    mode = (mode == 0) ? 0 : 1;                       // 只允许 0 或 1
    centerX = fmaxf(0.0f, fminf(centerX, 1.0f));
    centerY = fmaxf(0.0f, fminf(centerY, 1.0f));

    int totalPixels = frame.width * frame.height;
    size_t imgSize = totalPixels * sizeof(float3);

    // 分配设备内存
    float3* d_input = nullptr;
    float3* d_output = nullptr;
    cudaMalloc(&d_input, imgSize);
    cudaMalloc(&d_output, imgSize);
    if (!d_input || !d_output) {
        cudaFree(d_input); cudaFree(d_output);
        return;
    }

    // 主机端转换为 float3
    std::vector<float3> h_image(totalPixels);
    for (int i = 0; i < totalPixels; ++i) {
        h_image[i] = pixelToFloat3(frame.pixels[i]);
    }
    cudaMemcpy(d_input, h_image.data(), imgSize, cudaMemcpyHostToDevice);

    // 启动核函数
    dim3 block(16, 16);
    dim3 grid((frame.width + block.x - 1) / block.x, (frame.height + block.y - 1) / block.y);
    chromaticAberrationKernel <<< grid, block >>> (d_input, d_output,
        frame.width, frame.height,
        strength, mode, centerX, centerY);

    // 拷贝回主机并转换回 StdPixel
    std::vector<float3> h_output(totalPixels);
    cudaMemcpy(h_output.data(), d_output, imgSize, cudaMemcpyDeviceToHost);
    for (int i = 0; i < totalPixels; ++i) {
        frame.pixels[i] = float3ToPixel(h_output[i]);
    }

    cudaFree(d_input);
    cudaFree(d_output);
    cudaDeviceSynchronize();
}