#include "TopViewCanvasWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kMinimumZoom{0.25};
constexpr double kMaximumZoom{16.0};
constexpr double kWheelZoomBase{1.0015};
}

TopViewCanvasWidget::TopViewCanvasWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(640, 400);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setToolTip(QStringLiteral("滚轮缩放；按住鼠标中键平移；双击恢复适配"));
}

void TopViewCanvasWidget::SetImage(const QImage& image)
{
    m_image = image;
    ResetView();
}

void TopViewCanvasWidget::ClearImage()
{
    m_image = {};
    ResetView();
}

void TopViewCanvasWidget::ResetView()
{
    m_zoomFactor = 1.0;
    m_panOffset = {};
    m_panning = false;
    setCursor(Qt::OpenHandCursor);
    update();
}

void TopViewCanvasWidget::ZoomAt(
    const double factor,
    const QPointF& anchor)
{
    if (!std::isfinite(factor) || !(factor > 0.0) || m_image.isNull())
    {
        return;
    }
    const double previousZoom = m_zoomFactor;
    m_zoomFactor = std::clamp(
        previousZoom * factor, kMinimumZoom, kMaximumZoom);
    if (std::abs(m_zoomFactor - previousZoom) <= 1.0e-12)
    {
        return;
    }
    const QPointF canvasCenter(
        static_cast<double>(width()) * 0.5,
        static_cast<double>(height()) * 0.5);
    const QPointF imagePoint =
        (anchor - canvasCenter - m_panOffset) / previousZoom;
    m_panOffset = anchor - canvasCenter - imagePoint * m_zoomFactor;
    update();
}

void TopViewCanvasWidget::PanBy(const QPointF& delta)
{
    if (m_image.isNull())
    {
        return;
    }
    m_panOffset += delta;
    update();
}

bool TopViewCanvasWidget::HasImage() const
{
    return !m_image.isNull();
}

double TopViewCanvasWidget::ZoomFactor() const
{
    return m_zoomFactor;
}

QPointF TopViewCanvasWidget::PanOffset() const
{
    return m_panOffset;
}

void TopViewCanvasWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(43, 45, 49));
    if (m_image.isNull())
    {
        painter.setPen(QColor(210, 213, 218));
        painter.drawText(
            rect(), Qt::AlignCenter, QStringLiteral("尚未加载俯视场景"));
        return;
    }

    const double fitScale = (std::min)(
        static_cast<double>(width()) / m_image.width(),
        static_cast<double>(height()) / m_image.height());
    const double scale = fitScale * m_zoomFactor;
    const QSizeF targetSize(
        m_image.width() * scale,
        m_image.height() * scale);
    const QPointF center(
        static_cast<double>(width()) * 0.5,
        static_cast<double>(height()) * 0.5);
    const QRectF target(center + m_panOffset
            - QPointF(targetSize.width() * 0.5, targetSize.height() * 0.5),
        targetSize);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(target, m_image);
}

void TopViewCanvasWidget::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0)
    {
        event->ignore();
        return;
    }
    ZoomAt(std::pow(kWheelZoomBase, delta), event->position());
    event->accept();
}

void TopViewCanvasWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton && HasImage())
    {
        m_panning = true;
        m_lastMousePosition = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TopViewCanvasWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning)
    {
        const QPoint currentPosition = event->pos();
        PanBy(currentPosition - m_lastMousePosition);
        m_lastMousePosition = currentPosition;
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void TopViewCanvasWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton && m_panning)
    {
        m_panning = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void TopViewCanvasWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && HasImage())
    {
        ResetView();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}
