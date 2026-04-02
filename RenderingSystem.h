// Copyright 2026 MrSeagull. All Rights Reserved.

#pragma once
#define NOMINMAX
#include "BMP_Reader.h"
#include "cmdDrawer.h"
#include "SRP.cuh"
#include "FrameProcessing.cuh"
#include "SharedTypes.h"
#include "_EventBus.h"
#include <cmath>

class RenderingSystem {

    Size2DInt CanvasSize;       //最大画布大小
    std::vector<RenderData> RenderObjects;

public:
    RenderingSystem(const RenderingSystem&) = delete;
    RenderingSystem& operator=(const RenderingSystem&) = delete;
    static RenderingSystem& getInstance() {
        static RenderingSystem instance;
        return instance;
    }
    Frame Render_A_Frame(const LevelData& currentLevel, TextureSamplingMethod samplingMethod = SAMPLING_BICUBIC, int MSAA_Multiple = 1);
    Frame Render_A_Frame_ANISOTROPIC(const LevelData& currentLevel, int anisoLevel = 1, int MSAA_Multiple = 1);
    void RenderAndPrint(const LevelData& currentLevel, TextureSamplingMethod samplingMethod = SAMPLING_BICUBIC, int MSAA_Multiple = 1);
    void RenderAndPrint_ANISOTROPIC(const LevelData& currentLevel, int anisoLevel = 1, int MSAA_Multiple = 1);
    void Print_A_Frame(const Frame& frame);
private:
    RenderingSystem() {
        
        initializeConsoleDrawer();
        CanvasSize = getMaxCanvasSize();
        //CanvasSize.x = 1267; CanvasSize.y = 737;
        
        _EventBus::getInstance().subscribe_RenderAndPrint(
            [this](const LevelData& level, TextureSamplingMethod method, int msaa) {
                this->RenderAndPrint(level, method, msaa);
            }
        );
        _EventBus::getInstance().subscribe_RenderAndPrint_ANISOTROPIC(
            [this](const LevelData& level, int anisoLevel, int msaa) {
                this->RenderAndPrint_ANISOTROPIC(level, anisoLevel, msaa);
            }
        );

        Frame test = Render_A_Frame(_EventBus::getInstance().publish_ReadLevelData("E:\\Projects\\C++Projects\\OFGal_Engine\\TestLevel1.level"));
        //applyBloom(test);
        //applyFXAA(test, 0.2f, 0.08f, 10.0f, 0.125f, 0.01f);
        //applySMAA(test, 0.08f, 6, true);
        //applyLensDistortion(test, -0.15f);
        //applyChromaticAberration(test, 5.0f, 1);
        Print_A_Frame(test);
    }
    ~RenderingSystem() = default;

    void RefreshRenderObjects(const LevelData& currentLevel);
    void AABB_Remove(std::vector<RenderData>& renderObjects);
    void SortByDepth(std::vector<RenderData>& renderObjects);
    void RefreshDepth(std::vector<RenderData>& renderObjects);
    Frame Rasterize(std::vector<RenderData>& renderObjects, TextureSamplingMethod samplingMethod = SAMPLING_BICUBIC, int MSAA_Multiple = 1);
    Frame Rasterize_ANISOTROPIC(std::vector<RenderData>& renderObjects, int anisoLevel = 1, int MSAA_Multiple = 1);
};