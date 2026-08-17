#include "ThreeDCanvasWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
constexpr float kOrbitDegreesPerViewport{180.0F};

float OrbitDegreesForPointerDelta(
    const int deltaPixels,
    const int viewportPixels)
{
    return static_cast<float>(deltaPixels) * kOrbitDegreesPerViewport
        / static_cast<float>((std::max)(viewportPixels, 1));
}
}

ThreeDCanvasWidget::ThreeDCanvasWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(640, 400);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void ThreeDCanvasWidget::SetCameraChangedCallback(
    std::function<void()> callback)
{
    m_cameraChangedCallback = std::move(callback);
}

void ThreeDCanvasWidget::SetImage(const QImage& image)
{
    m_image = image;
    update();
}

void ThreeDCanvasWidget::ClearImage()
{
    m_image = {};
    update();
}

bool ThreeDCanvasWidget::HasImage() const
{
    return !m_image.isNull();
}

void ThreeDCanvasWidget::SetSceneBounds(const CameraBounds& bounds)
{
    m_sceneBounds = bounds;
    m_hasSceneBounds = true;
    const float extentX = (std::max)(bounds.maxX - bounds.minX, 1.0F);
    const float extentY = (std::max)(bounds.maxY - bounds.minY, 1.0F);
    const float extentZ = (std::max)(bounds.maxZ - bounds.minZ, 1.0F);
    m_panMmPerPixel = 1.25F * (std::max)({extentX, extentY, extentZ})
        / static_cast<float>((std::max)(1, (std::min)(width(), height())));
    m_camera.Fit(
        bounds,
        static_cast<std::uint32_t>((std::max)(width(), 1)),
        static_cast<std::uint32_t>((std::max)(height(), 1)));
}

void ThreeDCanvasWidget::FitScene()
{
    if (!m_hasSceneBounds)
    {
        return;
    }
    SetSceneBounds(m_sceneBounds);
    NotifyCameraChanged();
}

void ThreeDCanvasWidget::Orbit(
    const float yawDeltaDeg,
    const float pitchDeltaDeg)
{
    m_camera.Orbit(yawDeltaDeg, pitchDeltaDeg);
    NotifyCameraChanged();
}

void ThreeDCanvasWidget::Pan(const float rightMm, const float upMm)
{
    m_camera.Pan(rightMm, upMm);
    NotifyCameraChanged();
}

void ThreeDCanvasWidget::ZoomAtCursor(
    const float wheelSteps,
    const float normalizedX,
    const float normalizedY)
{
    m_camera.ZoomAtCursor(wheelSteps, normalizedX, normalizedY);
    NotifyCameraChanged();
}

void ThreeDCanvasWidget::SetPreset(const CameraPreset preset)
{
    m_camera.SetPreset(preset);
    NotifyCameraChanged();
}

void ThreeDCanvasWidget::SetProjection(
    const slicer::render::Projection projection)
{
    if (m_camera.ProjectionMode() == projection)
    {
        return;
    }
    m_camera.SetProjection(projection);
    NotifyCameraChanged();
}

const CameraController& ThreeDCanvasWidget::Camera() const
{
    return m_camera;
}

QSize ThreeDCanvasWidget::RenderSize() const
{
    return {(std::max)(width(), 800), (std::max)(height(), 480)};
}

void ThreeDCanvasWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(43, 45, 49));
    if (m_image.isNull())
    {
        painter.setPen(QColor(205, 208, 214));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("尚未加载 3D 场景"));
        return;
    }
    const QSize fitted = m_image.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect target(
        (width() - fitted.width()) / 2,
        (height() - fitted.height()) / 2,
        fitted.width(),
        fitted.height());
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(target, m_image);
}

void ThreeDCanvasWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePosition = event->pos();
    m_orbiting = event->button() == Qt::LeftButton;
    m_panning = event->button() == Qt::MiddleButton
        || event->button() == Qt::RightButton;
    if (m_orbiting || m_panning)
    {
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ThreeDCanvasWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_orbiting && !m_panning)
    {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPoint delta = event->pos() - m_lastMousePosition;
    m_lastMousePosition = event->pos();
    if (m_orbiting)
    {
        Orbit(
            OrbitDegreesForPointerDelta(delta.x(), width()),
            OrbitDegreesForPointerDelta(delta.y(), height()));
    }
    else
    {
        Pan(-static_cast<float>(delta.x()) * m_panMmPerPixel,
            static_cast<float>(delta.y()) * m_panMmPerPixel);
    }
    event->accept();
}

void ThreeDCanvasWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_orbiting = false;
    }
    if (event->button() == Qt::MiddleButton
        || event->button() == Qt::RightButton)
    {
        m_panning = false;
    }
    if (!m_orbiting && !m_panning)
    {
        unsetCursor();
    }
    event->accept();
}

void ThreeDCanvasWidget::wheelEvent(QWheelEvent* event)
{
    const QPointF position = event->position();
    const float normalizedX = width() > 0
        ? static_cast<float>(2.0 * position.x() / width() - 1.0) : 0.0F;
    const float normalizedY = height() > 0
        ? static_cast<float>(1.0 - 2.0 * position.y() / height()) : 0.0F;
    ZoomAtCursor(
        static_cast<float>(event->angleDelta().y()) / 120.0F,
        normalizedX,
        normalizedY);
    event->accept();
}

void ThreeDCanvasWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_camera.SetViewportSize(
        static_cast<std::uint32_t>((std::max)(event->size().width(), 1)),
        static_cast<std::uint32_t>((std::max)(event->size().height(), 1)));
    if (m_hasSceneBounds && HasImage())
    {
        NotifyCameraChanged();
    }
}

void ThreeDCanvasWidget::NotifyCameraChanged()
{
    if (m_cameraChangedCallback)
    {
        m_cameraChangedCallback();
    }
    update();
}
