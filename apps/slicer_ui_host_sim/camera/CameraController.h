#pragma once

#include "../render/IRenderBackend.h"

#include <cstdint>

/** @brief Enumerates the seven Stage 14E camera presets. */
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

/** @brief Axis-aligned world bounds used to fit the local camera. */
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
 * @brief Owns all host-local 3D camera state without module dependencies.
 */
class CameraController final
{
public:
    /** @brief Creates an orthographic isometric camera. */
    CameraController();

    /**
     * @brief Fits the camera to world bounds and viewport dimensions.
     * @param bounds World-space scene bounds in millimetres.
     * @param viewportWidthPx Viewport width in pixels.
     * @param viewportHeightPx Viewport height in pixels.
     */
    void Fit(
        const CameraBounds& bounds,
        std::uint32_t viewportWidthPx,
        std::uint32_t viewportHeightPx);

    /** @brief Applies local orbit deltas in degrees. */
    void Orbit(float yawDeltaDeg, float pitchDeltaDeg);

    /** @brief Applies local camera-plane translation in millimetres. */
    void Pan(float rightMm, float upMm);

    /**
     * @brief Zooms around a normalized cursor position.
     * @param wheelSteps Positive values zoom in.
     * @param normalizedX Cursor X in the range [-1, 1].
     * @param normalizedY Cursor Y in the range [-1, 1].
     */
    void ZoomAtCursor(
        float wheelSteps,
        float normalizedX,
        float normalizedY);

    /** @brief Applies one of the seven frozen camera presets. */
    void SetPreset(CameraPreset preset);

    /** @brief Selects orthographic or perspective projection. */
    void SetProjection(slicer::render::Projection projection);

    /** @brief Returns the current projection type. */
    [[nodiscard]] slicer::render::Projection ProjectionMode() const;

    /**
     * @brief Builds backend-neutral camera matrices for the current viewport.
     * @return Row-major view and projection matrices.
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
