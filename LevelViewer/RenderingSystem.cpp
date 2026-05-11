// Copyright 2026 MrSeagull. All Rights Reserved.
#include "RenderingSystem.h"
#include <numbers>
#include <filesystem>
constexpr double pi = std::numbers::pi;
static Matrix3D ComputeLocalMatrix(const TransformComponent& transform) {
    Matrix3D mat; // 默认构造单位矩阵
    float rad = transform.Rotation.r * pi / 180.0f;
    mat[0][0] = transform.Scale.x * cos(rad);
    mat[0][1] = -transform.Scale.y * sin(rad);
    mat[0][2] = transform.Location.x;
    mat[1][0] = transform.Scale.x * sin(rad);
    mat[1][1] = transform.Scale.y * cos(rad);
    mat[1][2] = transform.Location.y;
    return mat;
}

// 递归遍历对象，计算世界矩阵
static void TraverseObject(
    ObjectData* obj,
    const Matrix3D& parentWorld,
    std::vector<int> parentDepth,
    std::vector<RenderData>& outRenderObjects)
{
    if (!obj) return;

    // 计算当前对象的世界矩阵和累积深度
    Matrix3D world = parentWorld;
    std::vector<int> depth = parentDepth;

    if (obj->Transform.has_value()) {
        const auto& trans = obj->Transform.value();
        Matrix3D local = ComputeLocalMatrix(trans);
        world = parentWorld * local;
        depth.push_back(trans.Location.z);   // 累积深度

        // 如果对象含有 Picture 组件，生成渲染数据
        if (obj->Picture.has_value() && std::filesystem::exists(obj->Picture->Path)) {
            const auto& pic = obj->Picture.value();

            // 1. 加载纹理
            BMP_Data texture = Read_A_BMP(pic.Path.c_str());

            // 2. 确定实际渲染尺寸（原始纹理尺寸 vs 指定视窗尺寸）
            int imgWidth = texture.width;
            int imgHeight = texture.height;
            if (pic.Size.x > 0) imgWidth = static_cast<int>(pic.Size.x);
            if (pic.Size.y > 0) imgHeight = static_cast<int>(pic.Size.y);

            // 3. 图片局部坐标系中的四个角点（单位矩形，未应用 Picture 的变换）
            Vector3D localPoints[4] = {
                {0.0f, 0.0f, 1.0f},
                {static_cast<float>(imgWidth - 1), 0.0f, 1.0f},
                {static_cast<float>(imgWidth - 1), static_cast<float>(imgHeight - 1), 1.0f},
                {0.0f, static_cast<float>(imgHeight - 1), 1.0f}
            };
            // 顺序：(0,0) (1,0) (1,1) (0,1)

            // 4. 为 Picture 组件构建局部变换矩阵（先缩放/旋转/平移）
            float rad = pic.Rotation.r * pi / 180.0f;
            Matrix3D picLocal;
            picLocal[0][0] = cos(rad);   // 旋转（假设无缩放）
            picLocal[0][1] = -sin(rad);
            picLocal[1][0] = sin(rad);
            picLocal[1][1] = cos(rad);
            picLocal[0][2] = pic.Location.x;
            picLocal[1][2] = pic.Location.y;

            // 5. 组合变换：世界变换 × Picture 局部变换
            Matrix3D finalTransform = world * picLocal;

            // 6. 生成 RenderData
            RenderData renderData;
            renderData.trans = finalTransform;
            renderData.depth = depth;
            renderData.texture = std::move(texture);

            // 变换四个角点到世界坐标
            for (int i = 0; i < 4; ++i) {
                renderData.points[i] = finalTransform * localPoints[i];
            }

            renderData.inverse_trans = finalTransform.inverse();
            outRenderObjects.push_back(std::move(renderData));
        }
    }
    // 递归子对象
    for (auto& [childName, childObj] : obj->objects) {
        TraverseObject(childObj, world, depth, outRenderObjects);
    }
}

void RenderingSystem::RefreshRenderObjects(const LevelData& currentLevel) {
    RenderObjects.clear();
    Matrix3D identity;
    for (const auto& [objName, objPtr] : currentLevel.objects) {
        if (objPtr->parent == nullptr) {
            std::vector<int> defaultDepth;
            TraverseObject(objPtr, identity, defaultDepth, RenderObjects);
        }
    }
}

void RenderingSystem::AABB_Remove(std::vector<RenderData>& renderObjects) {
    for (auto it = renderObjects.begin(); it != renderObjects.end(); ) {
        const auto& points = it->points;
        int x_min = std::min({ points[0].x, points[1].x, points[2].x, points[3].x });
        int x_max = std::max({ points[0].x, points[1].x, points[2].x, points[3].x });
        int y_min = std::min({ points[0].y, points[1].y, points[2].y, points[3].y });
        int y_max = std::max({ points[0].y, points[1].y, points[2].y, points[3].y });

        // 若完全在视口外
        if (x_max < 0 || x_min >= CanvasSize.x || y_max < 0 || y_min >= CanvasSize.y)
            it = renderObjects.erase(it); // erase 返回下一个元素的迭代器
        else
            ++it;
    }
}

void RenderingSystem::SortByDepth(std::vector<RenderData>& renderObjects) {
    std::sort(renderObjects.begin(), renderObjects.end(),
        [](const RenderData& a, const RenderData& b) -> bool {
            const auto& da = a.depth;
            const auto& db = b.depth;
            size_t minSize = std::min(da.size(), db.size());
            for (size_t i = 0; i < minSize; ++i) {
                if (da[i] != db[i]) {
                    return da[i] > db[i];   // 降序：更深的在前
                }
            }
            // 所有公共前缀相等，则较长的数组更深，应排在前面
            return da.size() > db.size();
        });
}

void RenderingSystem::RefreshDepth(std::vector<RenderData>& renderObjects) {
    for (int i = renderObjects.size(); i > 0; --i)
        for (int j = 0; j < 4; ++j)
            renderObjects[i - 1].points[j].z = i;
}

Frame RenderingSystem::Rasterize(std::vector<RenderData>& renderObjects, TextureSamplingMethod samplingMethod, int MSAA_Multiple) {
    Frame result;
    result.width = CanvasSize.x;
    result.height = CanvasSize.y;
    result.pixels.resize(CanvasSize.x * CanvasSize.y, { 0,0,0 });

    // 验证有效性
    if (MSAA_Multiple < 1 || MSAA_Multiple > 4)
        MSAA_Multiple = 1;

    for (RenderData& renderData : renderObjects) {
        Rasterize_An_Object(result, renderData, samplingMethod, 1, MSAA_Multiple);
    }

    return result;
}
Frame RenderingSystem::Rasterize_ANISOTROPIC(std::vector<RenderData>& renderObjects, int anisoLevel, int MSAA_Multiple) {
    Frame result;
    result.width = CanvasSize.x;
    result.height = CanvasSize.y;
    result.pixels.resize(CanvasSize.x * CanvasSize.y, { 0,0,0 });

    // 验证有效性
    if (anisoLevel < 1 || anisoLevel > 16)
        anisoLevel = 1;
    if (MSAA_Multiple < 1 || MSAA_Multiple > 4)
        MSAA_Multiple = 1;

    for (RenderData& renderData : renderObjects) {
        Rasterize_An_Object(result, renderData, SAMPLING_ANISOTROPIC, anisoLevel, MSAA_Multiple);
    }

    return result;
}

Frame RenderingSystem::Render_A_Frame(const LevelData& currentLevel, TextureSamplingMethod samplingMethod, int MSAA_Multiple) {
    RefreshRenderObjects(currentLevel);
    AABB_Remove(RenderObjects);
    SortByDepth(RenderObjects);
    RefreshDepth(RenderObjects);

    return Rasterize(RenderObjects, samplingMethod, MSAA_Multiple);
}
Frame RenderingSystem::Render_A_Frame_ANISOTROPIC(const LevelData& currentLevel, int anisoLevel, int MSAA_Multiple) {
    RefreshRenderObjects(currentLevel);
    AABB_Remove(RenderObjects);
    SortByDepth(RenderObjects);
    RefreshDepth(RenderObjects);

    return Rasterize_ANISOTROPIC(RenderObjects, anisoLevel, MSAA_Multiple);
}
void RenderingSystem::RenderAndPrint(const LevelData& currentLevel, TextureSamplingMethod samplingMethod, int MSAA_Multiple) {
    RefreshRenderObjects(currentLevel);
    AABB_Remove(RenderObjects);
    SortByDepth(RenderObjects);
    RefreshDepth(RenderObjects);

    Frame frame = Rasterize(RenderObjects, samplingMethod, MSAA_Multiple);
    drawFrame(frame);
}
void RenderingSystem::RenderAndPrint_ANISOTROPIC(const LevelData& currentLevel, int anisoLevel, int MSAA_Multiple) {
    RefreshRenderObjects(currentLevel);
    AABB_Remove(RenderObjects);
    SortByDepth(RenderObjects);
    RefreshDepth(RenderObjects);

    Frame frame = Rasterize_ANISOTROPIC(RenderObjects, anisoLevel, MSAA_Multiple);
    drawFrame(frame);
}
void RenderingSystem::Print_A_Frame(const Frame& frame) {
    drawFrame(frame);
}

float RenderingSystem::CalculateThumbnailScaleFactor(Size2DInt& oriCanvasSize) const
{
    if (oriCanvasSize.x <= 0 || oriCanvasSize.y <= 0 ||
        CanvasSize.x <= 0 || CanvasSize.y <= 0)
    {
        return 0.5f;
    }

    float scaleX = static_cast<float>(CanvasSize.x) / static_cast<float>(oriCanvasSize.x);
    float scaleY = static_cast<float>(CanvasSize.y) / static_cast<float>(oriCanvasSize.y);

    float k = (scaleX < scaleY) ? scaleX : scaleY;
    return k;
}

void RenderingSystem::ScaleForThumbnail(std::vector<RenderData>& renderObjects, Size2DInt& oriCanvasSize)
{
    float k = CalculateThumbnailScaleFactor(oriCanvasSize);
    if (fabsf(k - 1.0f) < 1e-6f) return; // 无需缩放

    Matrix3D S;
    S[0][0] = k;   S[0][1] = 0;   S[0][2] = 0;
    S[1][0] = 0;   S[1][1] = k;   S[1][2] = 0;
    S[2][0] = 0;   S[2][1] = 0;   S[2][2] = 1.0f;

    for (auto& rd : renderObjects) {
        // 左乘缩放矩阵，将世界坐标整体缩放
        rd.trans = S * rd.trans;
        rd.inverse_trans = rd.trans.inverse();

        // 更新屏幕空间中的顶点坐标
        for (int i = 0; i < 4; ++i) {
            rd.points[i].x *= k;
            rd.points[i].y *= k;
            // z 保持 1.0f，后续 RefreshDepth 会赋予深度值
        }
    }
}

void RenderingSystem::DrawBorderLine(Frame& frame, const Size2DInt& border, const StdPixel& color) const
{
    if (border.x <= 0 || border.y <= 0) return;

    // 上水平线 (y = 0)
    for (int x = 0; x < border.x && x < frame.width; ++x) {
        frame.pixels[x] = color;
    }

    // 下水平线 (y = border.y - 1)
    if (border.y >= 2) {
        int yIdx = border.y - 1;
        if (yIdx < frame.height) {
            for (int x = 0; x < border.x && x < frame.width; ++x) {
                frame.pixels[yIdx * frame.width + x] = color;
            }
        }
    }

    // 左垂直线 (x = 0)
    for (int y = 0; y < border.y && y < frame.height; ++y) {
        frame.pixels[y * frame.width] = color;
    }

    // 右垂直线 (x = border.x - 1)
    if (border.x >= 2) {
        int xIdx = border.x - 1;
        if (xIdx < frame.width) {
            for (int y = 0; y < border.y && y < frame.height; ++y) {
                frame.pixels[y * frame.width + xIdx] = color;
            }
        }
    }
}

void RenderingSystem::RenderThumbnailAndPrint(const LevelData& currentLevel, Size2DInt& oriCanvasSize,
    TextureSamplingMethod samplingMethod,
    int MSAA_Multiple) {
    RefreshRenderObjects(currentLevel);

    ScaleForThumbnail(RenderObjects, oriCanvasSize);

    AABB_Remove(RenderObjects);
    SortByDepth(RenderObjects);
    RefreshDepth(RenderObjects);

    Frame frame = Rasterize(RenderObjects, samplingMethod, MSAA_Multiple);

    float k = CalculateThumbnailScaleFactor(oriCanvasSize);
    Size2DInt scaledBorder;
    scaledBorder.x = static_cast<int>(oriCanvasSize.x * k + 0.5f);
    scaledBorder.y = static_cast<int>(oriCanvasSize.y * k + 0.5f);
    StdPixel borderColor = { 255, 0, 0 };
    DrawBorderLine(frame, scaledBorder, borderColor);

    drawFrame(frame);
}