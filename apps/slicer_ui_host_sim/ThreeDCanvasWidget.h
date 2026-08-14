#pragma once

#include "camera/CameraController.h"

#include <QImage>
#include <QPoint>
#include <QWidget>

#include <functional>

/**
 * @brief 显示宿主本地三维帧并处理相机输入。
 */
class ThreeDCanvasWidget final : public QWidget
{
public:
    /** @brief 创建一个空的三维图像画布。 */
    explicit ThreeDCanvasWidget(QWidget* parent = nullptr);

    /**
     * @brief 安装用于在本地相机更改后渲染的回调。
     * @param callback 不得请求新 ViewData 的宿主回调。
     */
    void SetCameraChangedCallback(std::function<void()> callback);

    /**
     * @brief 用完整的 RGBA 图像替换可见帧。
     * @param image 宿主本地渲染输出。
     */
    void SetImage(const QImage& image);

    /** @brief 刷新或渲染失败后清除可见帧。 */
    void ClearImage();

    /** @brief 返回当前是否保留完整的帧。 */
    [[nodiscard]] bool HasImage() const;

    /**
     * @brief 使本地相机适应权威的世界范围。
     * @param bounds 世界空间边界以毫米为单位。
     */
    void SetSceneBounds(const CameraBounds& bounds);

    /** @brief 将本地摄像机重新调整到保留的场景边界。 */
    void FitScene();

    /** @brief 应用轨道增量（以度为单位）并请求局部重画。 */
    void Orbit(float yawDeltaDeg, float pitchDeltaDeg);

    /** @brief 应用相机平面内的平移增量，单位为毫米。 */
    void Pan(float rightMm, float upMm);

    /**
     * @brief 应用以光标为中心的缩放并请求本地重画。
     * @param wheelSteps 正值放大。
     * @param normalizedX 光标 X 在 [-1, 1] 中。
     * @param normalizedY 光标 Y 在 [-1, 1] 中。
     */
    void ZoomAtCursor(float wheelSteps, float normalizedX, float normalizedY);

    /** @brief 应用七个冻结摄像机预设之一。 */
    void SetPreset(CameraPreset preset);

    /** @brief 选择正交或透视局部投影。 */
    void SetProjection(slicer::render::Projection projection);

    /** @brief 返回当前宿主本地相机控制器。 */
    [[nodiscard]] const CameraController& Camera() const;

    /** @brief 返回本地渲染的稳定光栅目标大小。 */
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
