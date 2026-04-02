// Copyright 2026 MrSeagull. All Rights Reserved.
// 含有基础的后处理函数
#pragma once
#include "SharedTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bloom 泛光函数
// frame   : 输入输出帧（LDR RGB888）
// threshold : 亮度阈值（0～255），只有亮度高于此值的像素产生泛光
// intensity : 泛光强度（0.0～2.0），控制光晕叠加程度
// blurRadius : 高斯模糊半径（像素，推荐 3～15）
// sigma      : 高斯标准差（若 <=0 则自动取 blurRadius/3）
    void applyBloom(Frame& frame,
        float threshold = 220.0f,
        float intensity = 0.8f,
        int blurRadius = 4,
        float sigma = -1.0f);

// FXAA 抗锯齿函数
// frame          : 输入输出帧（LDR RGB888）
// edgeThreshold  : 边缘检测阈值（0.0~1.0），越高越敏感，默认 0.166（对应 0~255 的约 42）
// edgeThresholdMin: 最小边缘阈值，防止噪声，默认 0.05
// spanMax        : 最大搜索像素距离，默认 8.0
// reduceMul      : 方向搜索步长缩减系数，默认 1.0/8.0
// reduceMin      : 步长最小值，默认 1.0/128.0
    void applyFXAA(Frame& frame,
        float edgeThreshold = 0.166f,
        float edgeThresholdMin = 0.05f,
        float spanMax = 8.0f,
        float reduceMul = 1.0f / 8.0f,
        float reduceMin = 1.0f / 128.0f);

// SMAA 抗锯齿函数（1x 版本）
// frame          : 输入输出帧（LDR RGB888）
// edgeThreshold  : 边缘检测阈值（0.0~1.0），默认 0.05，值越高检测越敏感
// maxSearchSteps : 最大搜索步数（用于局部对比度自适应），默认 4
// enableDiag     : 是否启用对角线模式检测，默认 true
    void applySMAA(Frame& frame,
        float edgeThreshold = 0.05f,
        int maxSearchSteps = 4,
        bool enableDiag = true);

// 镜头畸变（桶形/枕形）
// frame    : 输入输出帧（LDR RGB888）
// strength : 畸变强度，正值 → 桶形畸变（中心向外凸），负值 → 枕形畸变（边缘向内凹），范围建议 -0.3 ~ 0.3
// centerX  : 畸变中心 X 坐标（归一化 0~1，0.5 为图像中心）
// centerY  : 畸变中心 Y 坐标（归一化 0~1，0.5 为图像中心）
    void applyLensDistortion(Frame& frame, float strength = 0.0f, float centerX = 0.5f, float centerY = 0.5f);

// 色差（Chromatic Aberration）
// frame    : 输入输出帧（LDR RGB888）
// strength : 偏移强度（像素），建议范围 0~10，值越大分离越明显
// mode     : 偏移模式，0 = 径向（从中心向外），1 = 水平（水平方向偏移）
// centerX  : 径向模式下的中心 X 坐标（归一化 0~1）
// centerY  : 径向模式下的中心 Y 坐标（归一化 0~1）
    void applyChromaticAberration(Frame& frame, float strength = 2.0f, int mode = 0, float centerX = 0.5f, float centerY = 0.5f);

#ifdef __cplusplus
}
#endif