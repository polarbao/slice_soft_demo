#include "ModelTopViewWidget.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace
{

constexpr int kHeaderHeight{52};
constexpr int kCanvasPadding{34};
constexpr int kMaximumSurfaceCacheEntries{64};

QString Utf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<int>(value.size()));
}

double NiceGridStep(const double worldSpan)
{
    if (!(worldSpan > 0.0) || !std::isfinite(worldSpan))
    {
        return 1.0;
    }
    const double target = worldSpan / 10.0;
    const double magnitude =
        std::pow(10.0, std::floor(std::log10(target)));
    const double normalized = target / magnitude;
    const double factor = normalized <= 1.0
        ? 1.0
        : normalized <= 2.0 ? 2.0 : normalized <= 5.0 ? 5.0 : 10.0;
    return factor * magnitude;
}

QString StateText(const SceneDocumentState state)
{
    switch (state)
    {
    case SceneDocumentState::Unloaded:
        return QStringLiteral("尚未加载模型");
    case SceneDocumentState::Loading:
        return QStringLiteral("正在加载模型...");
    case SceneDocumentState::Ready:
        return QStringLiteral("模型俯视已就绪");
    case SceneDocumentState::Blocked:
        return QStringLiteral("模型预检未通过，仅允许查看");
    case SceneDocumentState::Failed:
        return QStringLiteral("模型加载失败");
    case SceneDocumentState::Cancelled:
        return QStringLiteral("模型加载已取消");
    }
    return QStringLiteral("未知状态");
}

double Cross(
    const slicer_core::SceneViewPoint& a,
    const slicer_core::SceneViewPoint& b,
    const slicer_core::SceneViewPoint& point)
{
    return (b.xmm - a.xmm) * (point.ymm - a.ymm)
        - (b.ymm - a.ymm) * (point.xmm - a.xmm);
}

bool ContainsPoint(
    const slicer_core::SceneViewTriangle& triangle,
    const slicer_core::SceneViewPoint& point)
{
    const double first = Cross(triangle.a, triangle.b, point);
    const double second = Cross(triangle.b, triangle.c, point);
    const double third = Cross(triangle.c, triangle.a, point);
    const bool hasNegative =
        first < -1.0e-12 || second < -1.0e-12 || third < -1.0e-12;
    const bool hasPositive =
        first > 1.0e-12 || second > 1.0e-12 || third > 1.0e-12;
    return !(hasNegative && hasPositive)
        && std::abs(Cross(triangle.a, triangle.b, triangle.c))
            > 1.0e-12;
}

std::optional<slicer_core::SceneViewBounds> VisibleBounds(
    const SceneDocument& document)
{
    slicer_core::SceneViewBounds bounds{
        {std::numeric_limits<double>::max(),
         std::numeric_limits<double>::max()},
        {std::numeric_limits<double>::lowest(),
         std::numeric_limits<double>::lowest()}};
    bool found{false};
    for (const SceneDocumentItem& item : document.Items())
    {
        if (!item.instance.visible
            || !item.geometry.has_value()
            || item.geometry->triangles.empty())
        {
            continue;
        }
        found = true;
        bounds.min.xmm = std::min(
            bounds.min.xmm,
            item.geometry->worldboundsmm.min.xmm);
        bounds.min.ymm = std::min(
            bounds.min.ymm,
            item.geometry->worldboundsmm.min.ymm);
        bounds.max.xmm = std::max(
            bounds.max.xmm,
            item.geometry->worldboundsmm.max.xmm);
        bounds.max.ymm = std::max(
            bounds.max.ymm,
            item.geometry->worldboundsmm.max.ymm);
    }
    return found
        ? std::optional<slicer_core::SceneViewBounds>{bounds}
        : std::nullopt;
}

}  // namespace

ModelTopViewWidget::ModelTopViewWidget(
    SceneDocument* document,
    SceneSelectionModel* selectionModel,
    QWidget* parent)
    : QWidget(parent),
      m_document(document),
      m_selectionModel(selectionModel)
{
    Q_ASSERT(m_document != nullptr);
    Q_ASSERT(m_selectionModel != nullptr);
    setObjectName(QStringLiteral("modelTopViewWidget"));
    setMinimumSize(360, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    connect(
        m_document,
        &SceneDocument::SigChanged,
        this,
        &ModelTopViewWidget::OnDocumentChanged);
    connect(
        m_selectionModel,
        &SceneSelectionModel::SigSelectionChanged,
        this,
        &ModelTopViewWidget::OnSelectionChanged);
}

void ModelTopViewWidget::FitToView()
{
    update();
}

bool ModelTopViewWidget::HasRenderableGeometry() const
{
    return VisibleBounds(*m_document).has_value();
}

void ModelTopViewWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(247, 248, 249));
    DrawStatus(painter);

    const Camera camera = BuildCamera();
    painter.fillRect(camera.viewport, Qt::white);
    painter.setPen(QPen(QColor(205, 210, 214), 1.0));
    painter.drawRect(camera.viewport);
    DrawGrid(painter, camera);
    DrawGeometry(painter, camera);
}

void ModelTopViewWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton
        || !HasRenderableGeometry())
    {
        QWidget::mousePressEvent(event);
        return;
    }

    const Camera camera = BuildCamera();
    const slicer_core::SceneViewPoint worldPoint =
        ScreenToWorld(event->pos(), camera);
    QString selectedInstance;
    for (auto item = m_document->Items().rbegin();
         item != m_document->Items().rend();
         ++item)
    {
        if (!item->instance.visible || !item->geometry.has_value())
        {
            continue;
        }
        const auto& bounds = item->geometry->worldboundsmm;
        QRectF screenBounds(
            WorldToScreen(bounds.min, camera),
            WorldToScreen(bounds.max, camera));
        if (!screenBounds.normalized().contains(event->pos()))
        {
            continue;
        }
        const bool intersectsGeometry = std::any_of(
            item->geometry->triangles.begin(),
            item->geometry->triangles.end(),
            [&worldPoint](
                const slicer_core::SceneViewTriangle& triangle)
            {
                return ContainsPoint(triangle, worldPoint);
            });
        if (intersectsGeometry)
        {
            selectedInstance = QString::fromStdString(
                item->instance.instanceid);
            break;
        }
    }
    if (selectedInstance.isEmpty())
    {
        m_selectionModel->Clear();
    }
    else
    {
        m_selectionModel->SetSelectedInstance(selectedInstance);
        emit SigInstanceSelected(selectedInstance);
    }
    QWidget::mousePressEvent(event);
}

void ModelTopViewWidget::OnDocumentChanged()
{
    if (!m_document->Geometry().has_value())
    {
        m_selectionModel->Clear();
    }
    if (m_surfaceCache.size() > kMaximumSurfaceCacheEntries)
    {
        m_surfaceCache.clear();
    }
    update();
}

void ModelTopViewWidget::OnSelectionChanged(const QString& instanceId)
{
    Q_UNUSED(instanceId);
    update();
}

ModelTopViewWidget::Camera ModelTopViewWidget::BuildCamera() const
{
    Camera camera;
    camera.viewport = QRectF(rect()).adjusted(
        kCanvasPadding,
        kHeaderHeight,
        -kCanvasPadding,
        -kCanvasPadding);
    if (!HasRenderableGeometry()
        || camera.viewport.width() <= 1.0
        || camera.viewport.height() <= 1.0)
    {
        return camera;
    }

    const std::optional<slicer_core::SceneViewBounds> visibleBounds =
        VisibleBounds(*m_document);
    if (!visibleBounds.has_value())
    {
        return camera;
    }
    const auto& bounds = visibleBounds.value();
    const double width = bounds.max.xmm - bounds.min.xmm;
    const double height = bounds.max.ymm - bounds.min.ymm;
    camera.scale = std::max(
        1.0e-9,
        std::min(
            camera.viewport.width() / width,
            camera.viewport.height() / height)
            * 0.90);
    camera.centerxmm = (bounds.min.xmm + bounds.max.xmm) * 0.5;
    camera.centerymm = (bounds.min.ymm + bounds.max.ymm) * 0.5;
    return camera;
}

QPointF ModelTopViewWidget::WorldToScreen(
    const slicer_core::SceneViewPoint& point,
    const Camera& camera) const
{
    return {
        camera.viewport.center().x()
            + (point.xmm - camera.centerxmm) * camera.scale,
        camera.viewport.center().y()
            - (point.ymm - camera.centerymm) * camera.scale,
    };
}

slicer_core::SceneViewPoint ModelTopViewWidget::ScreenToWorld(
    const QPointF& point,
    const Camera& camera) const
{
    return {
        camera.centerxmm
            + (point.x() - camera.viewport.center().x())
                / camera.scale,
        camera.centerymm
            - (point.y() - camera.viewport.center().y())
                / camera.scale,
    };
}

void ModelTopViewWidget::DrawGrid(
    QPainter& painter,
    const Camera& camera) const
{
    if (!HasRenderableGeometry())
    {
        return;
    }

    const std::optional<slicer_core::SceneViewBounds> visibleBounds =
        VisibleBounds(*m_document);
    if (!visibleBounds.has_value())
    {
        return;
    }
    const auto& bounds = visibleBounds.value();
    const double width = bounds.max.xmm - bounds.min.xmm;
    const double height = bounds.max.ymm - bounds.min.ymm;
    const double step = NiceGridStep(std::max(width, height));
    const double leftWorld = camera.centerxmm
        - camera.viewport.width() / (2.0 * camera.scale);
    const double rightWorld = camera.centerxmm
        + camera.viewport.width() / (2.0 * camera.scale);
    const double bottomWorld = camera.centerymm
        - camera.viewport.height() / (2.0 * camera.scale);
    const double topWorld = camera.centerymm
        + camera.viewport.height() / (2.0 * camera.scale);

    painter.save();
    painter.setClipRect(camera.viewport);
    painter.setPen(QPen(QColor(231, 234, 236), 1.0));
    for (double x = std::floor(leftWorld / step) * step;
         x <= rightWorld;
         x += step)
    {
        const QPointF from = WorldToScreen({x, bottomWorld}, camera);
        const QPointF to = WorldToScreen({x, topWorld}, camera);
        painter.drawLine(from, to);
    }
    for (double y = std::floor(bottomWorld / step) * step;
         y <= topWorld;
         y += step)
    {
        const QPointF from = WorldToScreen({leftWorld, y}, camera);
        const QPointF to = WorldToScreen({rightWorld, y}, camera);
        painter.drawLine(from, to);
    }

    const QPointF origin = WorldToScreen({0.0, 0.0}, camera);
    painter.setPen(QPen(QColor(214, 71, 62), 1.5));
    painter.drawLine(
        QPointF(camera.viewport.left(), origin.y()),
        QPointF(camera.viewport.right(), origin.y()));
    painter.setPen(QPen(QColor(45, 134, 89), 1.5));
    painter.drawLine(
        QPointF(origin.x(), camera.viewport.top()),
        QPointF(origin.x(), camera.viewport.bottom()));
    painter.restore();

    painter.setPen(QColor(97, 102, 106));
    painter.drawText(
        camera.viewport.adjusted(6.0, 6.0, -6.0, -6.0),
        Qt::AlignRight | Qt::AlignBottom,
        QStringLiteral("+X 向右  +Y 向上  网格 %1 mm")
            .arg(step, 0, 'g', 4));
}

void ModelTopViewWidget::DrawGeometry(
    QPainter& painter,
    const Camera& camera) const
{
    if (!HasRenderableGeometry())
    {
        painter.setPen(QColor(112, 117, 121));
        painter.drawText(
            camera.viewport,
            Qt::AlignCenter,
            StateText(m_document->State()));
        return;
    }

    painter.save();
    painter.setClipRect(camera.viewport);
    bool hasBlocked{false};
    for (const SceneDocumentItem& item : m_document->Items())
    {
        if (!item.instance.visible || !item.geometry.has_value())
        {
            continue;
        }
        const slicer_core::SceneViewGeometry& geometry =
            item.geometry.value();
        const bool blocked =
            geometry.admissionstatus
            == slicer_core::SceneViewAdmissionStatus::Blocked;
        const bool selected =
            m_selectionModel->SelectedInstance()
            == Utf8(geometry.instanceid);
        hasBlocked = hasBlocked || blocked;

        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(Qt::NoPen);
        const auto& bounds = geometry.worldboundsmm;
        QRectF screenBounds(
            WorldToScreen(
                {bounds.min.xmm, bounds.max.ymm},
                camera),
            WorldToScreen(
                {bounds.max.xmm, bounds.min.ymm},
                camera));
        const QImage surfaceImage =
            SurfaceImage(geometry.surfacepreview);
        if (!surfaceImage.isNull())
        {
            painter.setRenderHint(
                QPainter::SmoothPixmapTransform,
                true);
            painter.drawImage(
                screenBounds.normalized(),
                surfaceImage);
        }
        else
        {
            painter.setBrush(
                blocked
                    ? QColor(222, 168, 166)
                    : QColor(66, 144, 139));
            for (const auto& triangle : geometry.triangles)
            {
                QPolygonF polygon;
                polygon.reserve(3);
                polygon << WorldToScreen(triangle.a, camera)
                        << WorldToScreen(triangle.b, camera)
                        << WorldToScreen(triangle.c, camera);
                painter.drawPolygon(polygon);
            }
        }

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(
            QPen(
                selected
                    ? QColor(20, 104, 190)
                    : blocked ? QColor(184, 45, 45)
                              : QColor(31, 83, 80),
                selected ? 3.0 : 2.0,
                item.instance.locked
                    ? Qt::DashLine
                    : Qt::SolidLine));
        painter.drawRect(screenBounds.normalized());
    }
    painter.restore();

    if (hasBlocked)
    {
        painter.setPen(QColor(156, 35, 35));
        painter.drawText(
            camera.viewport.adjusted(10.0, 10.0, -10.0, -10.0),
            Qt::AlignLeft | Qt::AlignTop,
            QStringLiteral("BLOCKED：仅可查看，不代表生产准入"));
    }
}

QImage ModelTopViewWidget::SurfaceImage(
    const slicer_core::SceneViewSurfacePreview& preview) const
{
    if (!preview.IsValid()
        || preview.contenthash.empty())
    {
        return {};
    }
    const QString cacheKey =
        QString::fromStdString(preview.contenthash);
    const auto cached = m_surfaceCache.constFind(cacheKey);
    if (cached != m_surfaceCache.constEnd())
    {
        return cached.value();
    }
    const QImage source(
        preview.rgba.data(),
        preview.width,
        preview.height,
        preview.width * 4,
        QImage::Format_RGBA8888);
    const QImage image = source.copy();
    m_surfaceCache.insert(cacheKey, image);
    return image;
}

void ModelTopViewWidget::DrawStatus(QPainter& painter) const
{
    painter.setPen(QColor(36, 40, 43));
    painter.drawText(
        QRectF(12.0, 6.0, width() - 24.0, 20.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        StateText(m_document->State()));

    QString detail = m_document->ModelPath();
    if (m_document->Geometry().has_value())
    {
        const auto& geometry = m_document->Geometry().value();
        detail = QStringLiteral(
                     "%1  scene=%2  instance=%3  revision=%4/%5  count=%6")
                     .arg(detail)
                     .arg(Utf8(geometry.sceneid))
                     .arg(Utf8(geometry.instanceid))
                     .arg(geometry.scenerevision)
                     .arg(geometry.transformrevision)
                     .arg(m_document->InstanceCount());
    }
    else if (m_document->State() == SceneDocumentState::Failed)
    {
        detail = QStringLiteral("%1：%2")
                     .arg(detail, m_document->Error());
    }

    const QFontMetrics metrics(painter.font());
    painter.setPen(QColor(93, 98, 102));
    painter.drawText(
        QRectF(12.0, 27.0, width() - 24.0, 20.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        metrics.elidedText(
            detail,
            Qt::ElideMiddle,
            std::max(0, width() - 24)));
}
