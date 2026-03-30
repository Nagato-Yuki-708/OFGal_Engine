// Copyright 2026 MrSeagull. All Rights Reserved.
#include "SRP.cuh"
#include <cuda_runtime.h>
#include <cmath>
#include <algorithm>

// 设备端：矩阵乘向量（3x3 * 3x1）
__device__ Vector3D mulMatrixVector(const Matrix3D& mat, const Vector3D& vec) {
    Vector3D result;
    result.x = mat.m[0][0] * vec.x + mat.m[0][1] * vec.y + mat.m[0][2] * vec.z;
    result.y = mat.m[1][0] * vec.x + mat.m[1][1] * vec.y + mat.m[1][2] * vec.z;
    result.z = mat.m[2][0] * vec.x + mat.m[2][1] * vec.y + mat.m[2][2] * vec.z;
    return result;
}

// 三次卷积核权重计算 (a = -0.5)
__device__ float cubicWeight(float x) {
    float ax = fabsf(x);
    if (ax <= 1.0f) {
        return (1.5f * ax - 2.5f) * ax * ax + 1.0f;
    }
    else if (ax < 2.0f) {
        return ((-0.5f * ax + 2.5f) * ax - 4.0f) * ax + 2.0f;
    }
    else {
        return 0.0f;
    }
}

// 双三次插值内核
__global__ void resizeBicubicKernel(const BMP_Pixel* src, int srcWidth, int srcHeight,
    BMP_Pixel* dst, int dstWidth, int dstHeight) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= dstWidth || y >= dstHeight) return;

    // 目标像素对应源图像中的浮点坐标（保持中心对齐）
    float sx = (x + 0.5f) * (srcWidth / (float)dstWidth) - 0.5f;
    float sy = (y + 0.5f) * (srcHeight / (float)dstHeight) - 0.5f;

    // 整数坐标和小数部分
    int ix = floorf(sx);
    int iy = floorf(sy);
    float dx = sx - ix;
    float dy = sy - iy;

    // 累加器
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
    float totalWeight = 0.0f;

    // 遍历4x4邻域 (i = -1..2, j = -1..2)
    for (int i = -1; i <= 2; ++i) {
        int srcX = ix + i;
        // 边界处理：clamp到有效范围
        srcX = max(0, min(srcX, srcWidth - 1));
        float wx = cubicWeight(dx - i);  // 注意：公式中使用 dx - i

        for (int j = -1; j <= 2; ++j) {
            int srcY = iy + j;
            srcY = max(0, min(srcY, srcHeight - 1));
            float wy = cubicWeight(dy - j);
            float w = wx * wy;

            const BMP_Pixel& p = src[srcY * srcWidth + srcX];
            r += p.red * w;
            g += p.green * w;
            b += p.blue * w;
            a += p.alpha * w;
            totalWeight += w;
        }
    }

    // 归一化（权重和可能不是1，但通常非常接近1；这里做归一化保证颜色准确）
    if (totalWeight > 1e-6f) {
        r /= totalWeight;
        g /= totalWeight;
        b /= totalWeight;
        a /= totalWeight;
    }

    // 钳位到[0,255]并存入目标
    BMP_Pixel& out = dst[y * dstWidth + x];
    out.red = (unsigned char)(max(0.0f, min(255.0f, r)));
    out.green = (unsigned char)(max(0.0f, min(255.0f, g)));
    out.blue = (unsigned char)(max(0.0f, min(255.0f, b)));
    out.alpha = (unsigned char)(max(0.0f, min(255.0f, a)));
}

// 主机端函数：将源图像缩放到目标尺寸
extern "C" BMP_Data ForceResizeImage_Bicubic(const BMP_Data& src, const Size2DInt& dstSize) {
    BMP_Data dst;
    dst.width = dstSize.x;
    dst.height = dstSize.y;
    dst.pixels.resize(dst.width * dst.height);

    // 如果源图像为空或目标尺寸无效，直接返回空图像
    if (src.pixels.empty() || dst.width <= 0 || dst.height <= 0) {
        return dst;
    }

    // 设备内存分配
    BMP_Pixel* d_src = nullptr, * d_dst = nullptr;
    size_t srcBytes = src.pixels.size() * sizeof(BMP_Pixel);
    size_t dstBytes = dst.pixels.size() * sizeof(BMP_Pixel);

    cudaMalloc(&d_src, srcBytes);
    cudaMalloc(&d_dst, dstBytes);

    // 拷贝源图像到设备
    cudaMemcpy(d_src, src.pixels.data(), srcBytes, cudaMemcpyHostToDevice);

    // 配置内核启动参数
    dim3 blockSize(16, 16);  // 可根据硬件调整
    dim3 gridSize((dst.width + blockSize.x - 1) / blockSize.x,
        (dst.height + blockSize.y - 1) / blockSize.y);

    // 启动内核
    resizeBicubicKernel << <gridSize, blockSize >> > (d_src, src.width, src.height,
        d_dst, dst.width, dst.height);

    // 拷贝结果回主机
    cudaMemcpy(dst.pixels.data(), d_dst, dstBytes, cudaMemcpyDeviceToHost);

    // 释放设备内存
    cudaFree(d_src);
    cudaFree(d_dst);

    // 检查CUDA错误
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        BMP_Data empty;
        return empty;
    }

    return dst;
}

__device__ bool isPointInTriangle(float px, float py,
    float ax, float ay,
    float bx, float by,
    float cx, float cy) {

    float abx = bx - ax, aby = by - ay;
    float apx = px - ax, apy = py - ay;
    float cross1 = abx * apy - aby * apx;

    float bcx = cx - bx, bcy = cy - by;
    float bpx = px - bx, bpy = py - by;
    float cross2 = bcx * bpy - bcy * bpx;

    float cax = ax - cx, cay = ay - cy;
    float cpx = px - cx, cpy = py - cy;
    float cross3 = cax * cpy - cay * cpx;

    // 允许边界上的点（包含零）
    bool hasNeg = (cross1 < 0) || (cross2 < 0) || (cross3 < 0);
    bool hasPos = (cross1 > 0) || (cross2 > 0) || (cross3 > 0);
    return !(hasNeg && hasPos);
}

// 内核函数：并行处理四边形覆盖的像素
__global__ void RasterizeQuadKernel(
    unsigned char* framePixels,   // 帧缓冲（RGB，每个像素3字节）
    int width, int height,
    Matrix3D inverse_trans,
    Vector3D points[4],           // 四个屏幕空间顶点（已拷贝到设备）
    const unsigned char* texPixels, // 纹理像素数据，四通道RGBA
    int texWidth, int texHeight,
    int msaaMultiple)             // MSAA倍数
{
    // 获取当前线程对应的像素坐标（全局索引）
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    // 边界检查：超出屏幕范围则退出
    if (x >= width || y >= height) return;

    // 像素中心坐标
    float px = x + 0.5f;
    float py = y + 0.5f;

    // 将四边形拆成两个三角形： (0,1,2) 和 (0,2,3)
    Vector3D tri1[3] = { points[0], points[1], points[2] };
    Vector3D tri2[3] = { points[0], points[2], points[3] };

    // 判断点是否在任一三角形内
    bool inside = isPointInTriangle(px, py,
        tri1[0].x, tri1[0].y,
        tri1[1].x, tri1[1].y,
        tri1[2].x, tri1[2].y) ||
        isPointInTriangle(px, py,
            tri2[0].x, tri2[0].y,
            tri2[1].x, tri2[1].y,
            tri2[2].x, tri2[2].y);

    if (inside) {
        unsigned char r = 0, g = 0, b = 0;

        Vector3D P = { px, py, 1 };
        Vector3D TargetPixel = mulMatrixVector(inverse_trans, P);

        int i = TargetPixel.x;
        int j = TargetPixel.y;

        if (i < 0) i = 0;
        if (i >= texWidth) i = texWidth - 1;
        if (j < 0) j = 0;
        if (j >= texHeight) j = texHeight - 1;

        r = texPixels[(j * texWidth + i) * 4 + 0];
        g = texPixels[(j * texWidth + i) * 4 + 1];
        b = texPixels[(j * texWidth + i) * 4 + 2];

        // 写入帧缓冲
        framePixels[(y * width + x) * 3 + 0];
        framePixels[(y * width + x) * 3 + 1];
        framePixels[(y * width + x) * 3 + 2];
    }
}

extern "C" void Rasterize_An_Object(Frame& frame, const RenderData& obj, const int& MSAA_Multiple) {
    const int width = frame.width;
    const int height = frame.height;

    const Vector3D* p = obj.points;  // 主机端顶点指针

    // 计算 AABB
    float minX = min(p[0].x, min(p[1].x, min(p[2].x, p[3].x)));
    float maxX = max(p[0].x, max(p[1].x, max(p[2].x, p[3].x)));
    float minY = min(p[0].y, min(p[1].y, min(p[2].y, p[3].y)));
    float maxY = max(p[0].y, max(p[1].y, max(p[2].y, p[3].y)));

    int xStart = max(0, (int)floor(minX));
    int xEnd = min(width - 1, (int)ceil(maxX));
    int yStart = max(0, (int)floor(minY));
    int yEnd = min(height - 1, (int)ceil(maxY));
    int roiWidth = xEnd - xStart + 1;
    int roiHeight = yEnd - yStart + 1;

    if (roiWidth <= 0 || roiHeight <= 0) return;  // 无有效像素

    dim3 blockSize(16, 16);
    dim3 gridSize((roiWidth + blockSize.x - 1) / blockSize.x,
        (roiHeight + blockSize.y - 1) / blockSize.y);

    // ----- 分配设备内存并拷贝数据 -----
    // 1. 帧缓冲
    unsigned char* d_frame = nullptr;
    size_t frameBytes = frame.pixels.size() * sizeof(StdPixel);
    cudaMalloc(&d_frame, frameBytes);
    cudaMemcpy(d_frame, frame.pixels.data(), frameBytes, cudaMemcpyHostToDevice);

    // 2. 纹理数据 (注意 const 处理)
    const unsigned char* d_tex = nullptr;
    size_t texBytes = obj.texture.pixels.size() * sizeof(BMP_Pixel);
    cudaMalloc((void**)&d_tex, texBytes);
    cudaMemcpy((void*)d_tex, obj.texture.pixels.data(), texBytes, cudaMemcpyHostToDevice);

    // 3. 顶点数组 (4个 Vector3D)
    Vector3D* d_points = nullptr;
    size_t pointsBytes = 4 * sizeof(Vector3D);
    cudaMalloc(&d_points, pointsBytes);
    cudaMemcpy(d_points, obj.points, pointsBytes, cudaMemcpyHostToDevice);

    // ----- 启动内核 -----
    RasterizeQuadKernel << <gridSize, blockSize >> > (
        d_frame, width, height,
        obj.inverse_trans,          // inverse_trans 已在设备端？如果是主机端，也需要拷贝
        d_points,
        d_tex,
        obj.texture.width, obj.texture.height,
        MSAA_Multiple
        );

    // ----- 错误检查 -----
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("Kernel launch error: %s\n", cudaGetErrorString(err));
    }
    cudaDeviceSynchronize();
    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        printf("Kernel execution error: %s\n", cudaGetErrorString(err));
    }

    // ----- 拷贝结果回主机 -----
    cudaMemcpy(frame.pixels.data(), d_frame, frameBytes, cudaMemcpyDeviceToHost);

    // ----- 释放设备内存 -----
    cudaFree(d_frame);
    cudaFree((void*)d_tex);
    cudaFree(d_points);
}