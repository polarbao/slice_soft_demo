#pragma once

#include "camera/CameraController.h"

#include <QImage>
#include <QPoint>
#include <QWidget>

#include <functional>

/**
 * @brief Displays host-local three-dimensional frames and owns camera input.
 */
class ThreeDCanvasWidget final : public QWidget
{
public:
    /** @brief Creates an empty three-dimensional image canvas. */
    explicit ThreeDCanvasWidget(QWidget* parent = nullptr);

    /**
     * @brief Installs the callback used to render after local camera changes.
     * @param callback Host callback that must not request new ViewData.
     */
    void SetCameraChangedCallback(std::function<void()> callback);

    /**
     * @brief Replaces the visible frame with a complete RGBA image.
     * @param image Host-local renderer output.
     */
    void SetImage(const QImage& image);

    /** @brief Clears the visible frame after refresh or render failure. */
    void ClearImage();

    /** @brief Returns whether a complete frame is currently retained. */
    [[nodiscard]] bool HasImage() const;

    /**
     * @brief Fits the local camera to authoritative world bounds.
     * @param bounds World-space bounds in millimetres.
     */
    void SetSceneBounds(const CameraBounds& bounds);

    /** @brief Refits the local camera to the retained scene bounds. */
    void FitScene();

    /** @brief Applies orbit deltas in degrees and requests a local redraw. */
    void Orbit(float yawDeltaDeg, float pitchDeltaDeg);

    /** @brief Applies camera-plane pan deltas in millimetres. */
    void Pan(float rightMm, float upMm);

    /**
     * @brief Applies cursor-centred zoom and requests a local redraw.
     * @param wheelSteps Positive values zoom in.
     * @param normalizedX Cursor X in [-1, 1].
     * @param normalizedY Cursor Y in [-1, 1].
     */
    void ZoomAtCursor(float wheelSteps, float normalizedX, float normalizedY);

    /** @brief Applies one of the seven frozen camera presets. */
    void SetPreset(CameraPreset preset);

    /** @brief Selects orthographic or perspective local projection. */
    void SetProjection(slicer::render::Projection projection);

    /** @brief Returns the current host-local camera controller. */
    [[nodiscard]] const CameraController& Camera() const;

    /** @brief Returns a stable raster target size for local rendering. */
    [[nodiscard]] QSize RenderSize() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void NotifyCameraChanged();

    QImage m_image;
    CameraController m_camera;
    CameraBounds m_sceneBounds;
    QPoint m_lastMousePosition;
    std::function<void()> m_cameraChangedCallback;
    float m_panMmPerPixel{0.1F};
    bool m_hasSceneBounds{false};
    bool m_orbiting{false};
    bool m_panning{false};
};
