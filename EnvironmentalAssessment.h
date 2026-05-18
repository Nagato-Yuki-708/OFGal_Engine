// Copyright 2026 Nagato-Yuki-708. All Rights Reserved.
#pragma once
#include <cuda_runtime_api.h>
#include <cstdlib>
#include <filesystem>

/// @brief 判断当前电脑是否拥有可供本软件使用的独立 NVIDIA 显卡
/// @return true  存在至少一块独立的 NVIDIA GPU
///         false 无可用 NVIDIA GPU，或仅存在集成显卡，或 CUDA 初始化失败
/// @note  调用此函数前程序必须已成功链接 CUDA Runtime 库。
///        如果希望在无 NVIDIA 驱动的环境中避免启动崩溃，
///        请采用动态加载 cudart 的方式（如 dlopen/LoadLibrary）。
bool HasDiscreteNvidiaGPU()
{
    int deviceCount = 0;

    // 获取 CUDA 设备数量
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (err != cudaSuccess || deviceCount == 0) {
        // 没有任何 CUDA 设备，或驱动未正常工作
        return false;
    }

    // 遍历所有设备，寻找非集成（独立）GPU
    for (int i = 0; i < deviceCount; ++i) {
        cudaDeviceProp prop;
        err = cudaGetDeviceProperties(&prop, i);
        if (err != cudaSuccess) {
            continue;   // 跳过无法获取属性的设备
        }

        // 通过 integrated 字段判断是否为集成显卡
        // integrated == 0 表示独立显卡（或非 APU 板载 GPU）
        if (prop.integrated == 0) {
            return true;   // 找到至少一个独立 GPU
        }
    }

    // 所有设备均为集成显卡（如 Intel 核显、AMD APU 的集成部分），
    // 或者根本没有 CUDA 设备（已在前面处理）
    return false;
}

bool IsRunningInWindowsTerminal()
{
    char* wtSession = nullptr;
    size_t len = 0;
    errno_t err = _dupenv_s(&wtSession, &len, "WT_SESSION");
    if (err == 0 && wtSession != nullptr && wtSession[0] != '\0') {
        free(wtSession);   // 释放分配的内存
        return true;
    }
    free(wtSession);       // 即使为空也需释放（_dupenv_s 可能分配了空串的内存）
    return false;
}

/// @brief 检查给定路径是否为一个存在的目录
/// @param path 要验证的路径字符串
/// @return true  路径非空，且对应一个存在的目录
///         false 路径为空，或不存在，或是一个文件
bool IsValidDirectory(const std::string& path)
{
    if (path.empty())
        return false;

    std::error_code ec;                     // 使用 error_code 避免异常
    auto status = std::filesystem::status(path, ec);
    if (ec)                                 // 发生错误（如权限不足）
        return false;

    return std::filesystem::exists(status) && std::filesystem::is_directory(status);
}