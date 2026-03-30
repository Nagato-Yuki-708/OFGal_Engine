// Copyright 2026 MrSeagull. All Rights Reserved.
// Sequential Rendering Pipeline (顺序渲染管线：从后向前渲染)
#pragma once
#define NOMINMAX
#include "SharedTypes.h"
// #include "RenderingSystem.h"

#ifdef __cplusplus
extern "C" {
#endif

    BMP_Data ForceResizeImage_Bicubic(const BMP_Data& src, const Size2DInt& dstSize);

    void Rasterize_An_Object(Frame& frame, const RenderData& obj, const int& MSAA_Multiple);

#ifdef __cplusplus
}
#endif