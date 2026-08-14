#pragma once

#include "../render/IRenderBackend.h"

#include <cstdint>

/** @brief 枚举七个 Stage 14E 摄像机预设。 */
enum class CameraPreset
{
    Top,
    Bottom,
    Front,
    Back,
    Left,
    Right,
    Isometric
};

/** @brief 用于适应本地相机的轴对齐世界边界。 */
struct CameraBounds final
{
    float minX{0.0F};
    float minY{0.0F};
    float minZ{0.0F};
    float maxX{1.0F};
    float maxY{1.0F};
    float maxZ{1.0F};
};

/**
 * @brief 持有全部宿主本地 3D 相机状态，不依赖切片模块。
 */
class CameraController final
{
public:
    /** @brief 创建正交等距相机。 */
    CameraController();

    /**
     * @brief 使相机适合世界边界和视口尺寸。
     * @param bounds 世界空间场景以毫米为单位。
     * @param viewportWidthPx 视口宽度（以像素为单位）。
     * @param viewportHeightPx 视口高度（以像素为单位）。
     */
    void Fit(
        const CameraBounds& bounds,
        std::uint32_t viewportWidthPx,
        std::uint32_t viewportHeightPx);

    /**
     * @brief 更新视口尺寸而不更改当前姿势。
     * @param viewportWidthPx 视口宽度（以像素为单位）。
     * @param viewportHeightPx 视口高度（以像素为单位）。
     */
    void SetViewportSize(
        std::uint32_t viewportWidthPx,
        std::uint32_t viewportHeightPx);

    /** @brief 应用局部轨道增量（以度为单位）。 */
    void Orbit(float yawDeltaDeg, float pitchDeltaDeg);

    /** @brief 应用局部相机平面平移（以毫米为单位）。 */
    void Pan(float rightMm, float upMm);

    /**
     * @brief 围绕标准化光标位置进行缩放。
     * @param wheelSteps 正值放大。
     * @param normalizedX 光标 X 在 [-1, 1] 范围内。
     * @param normalizedY 光标 Y 在 [-1, 1] 范围内。
     */
    void ZoomAtCursor(
        float wheelSteps,
        float normalizedX,
        float normalizedY);

    /** @brief 应用七个冻结摄像机预设之一。 */
    void SetPreset(CameraPreset preset);

    /** @brief 选择正交或透视投影。 */
    void SetProjection(slicer::render::Projection projection);

    /** @brief 返回当前的投影类型。 */
    [[nodiscard]] slicer::render::Projection ProjectionMode() const;

    /**
     * @brief 为当前视口构建后端中性相机矩阵。
     * @return 行主视图和投影矩阵。
     */
    [[nodiscard]] slicer::render::CameraDesc BuildCamera() const;

private:
    float m_targetX{0.0F};
    float m_targetY{0.0F};
    float m_targetZ{0.0F};
    float m_yawDeg{-45.0F};
    float m_pitchDeg{35.264F};
    float m_distanceMm{100.0F};
    float m_orthographicHeightMm{100.0F};
    std::uint32_t m_viewportWidthPx{1U};
    std::uint32_t m_viewportHeightPx{1U};
    slicer::render::Projection m_projection{
        slicer::render::Projection::Orthographic};
};
