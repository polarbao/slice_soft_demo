#include "TopViewRenderPolicy.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QRectF>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
constexpr double kCanvasPadding{34.0};
constexpr double kHeaderHeight{52.0};

QPointF TransformPoint(
    const std::array<double, 16>& matrix,
    double x,
    double y)
{
    return {
        matrix.at(0) * x + matrix.at(1) * y + matrix.at(3),
        matrix.at(4) * x + matrix.at(5) * y + matrix.at(7)};
}

QPolygonF WorldCorners(const TopViewInstance& instance)
{
    const TopViewBounds& bounds = instance.localBoundsMm;
    QPolygonF result;
    result << TransformPoint(instance.worldMatrix, bounds.minX, bounds.maxY)
           << TransformPoint(instance.worldMatrix, bounds.maxX, bounds.maxY)
           << TransformPoint(instance.worldMatrix, bounds.maxX, bounds.minY)
           << TransformPoint(instance.worldMatrix, bounds.minX, bounds.minY);
    return result;
}
}

TopViewRenderPolicy::TopViewRenderPolicy(ModuleClient& client)
    : m_client(client)
{
}

bool TopViewRenderPolicy::Refresh(
    quint64 sceneHandle,
    quint64 sceneRevision,
    TopViewFrame* frame,
    QString* error)
{
    if (frame == nullptr || sceneHandle == 0 || sceneRevision == 0)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("俯视刷新缺少有效场景身份。");
        }
        return false;
    }

    const QJsonObject request{
        {QStringLiteral("capability"), QStringLiteral("scene.get_viewdata")},
        {QStringLiteral("operation"), QStringLiteral("query")},
        {QStringLiteral("sceneHandle"), static_cast<qint64>(sceneHandle)},
        {QStringLiteral("expectedSceneRevision"),
         static_cast<qint64>(sceneRevision)},
        {QStringLiteral("viewMode"), QStringLiteral("top")},
        {QStringLiteral("texturePolicy"),
         QStringLiteral("require_if_present")},
        {QStringLiteral("lod"), QStringLiteral("auto")},
        {QStringLiteral("meshTransform"), QStringLiteral("local")},
        {QStringLiteral("maxBytes"), 64 * 1024 * 1024},
        {QStringLiteral("content"), QJsonArray{
             QStringLiteral("bbox"),
             QStringLiteral("outline"),
             QStringLiteral("surface_preview"),
             QStringLiteral("appearance")}}};

    QJsonObject result;
    if (!ExecuteJson(request, &result, error)
        || !result.value(QStringLiteral("ok")).toBool())
    {
        if (error != nullptr && error->isEmpty())
        {
            *error = QString::fromUtf8(
                QJsonDocument(result).toJson(QJsonDocument::Compact));
        }
        return false;
    }

    const quint64 returnedRevision = static_cast<quint64>(
        result.value(QStringLiteral("sceneRevision")).toDouble());
    const QString identity = result.value(
        QStringLiteral("viewdataIdentity")).toString();
    if (returnedRevision != sceneRevision
        || identity.isEmpty()
        || result.value(QStringLiteral("viewMode")).toString()
            != QStringLiteral("top"))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("模块返回了不匹配的 top ViewData 身份。");
        }
        return false;
    }

    QHash<QString, bool> appearances;
    for (const QJsonValue& value : result.value(
             QStringLiteral("appearances")).toArray())
    {
        const QString appearanceIdentity = value.toObject().value(
            QStringLiteral("appearanceIdentity")).toString();
        if (appearanceIdentity.isEmpty()
            || appearances.contains(appearanceIdentity))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("ViewData 外观身份缺失或重复。");
            }
            return false;
        }
        appearances.insert(appearanceIdentity, true);
    }

    TopViewFrame decoded;
    decoded.viewDataIdentity = identity;
    decoded.sceneRevision = returnedRevision;
    const QJsonArray instances = result.value(
        QStringLiteral("instances")).toArray();
    if (instances.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("top ViewData 未返回任何实例。");
        }
        return false;
    }
    decoded.instances.reserve(instances.size());
    for (const QJsonValue& value : instances)
    {
        TopViewInstance instance;
        if (!DecodeInstance(
                value.toObject(), appearances, &instance, error))
        {
            return false;
        }
        decoded.instances.push_back(std::move(instance));
    }
    *frame = std::move(decoded);
    return true;
}

QImage TopViewRenderPolicy::Render(
    const TopViewFrame& frame,
    const QSize& canvasSize) const
{
    QImage output(canvasSize, QImage::Format_RGBA8888);
    if (!RenderInto(frame, &output))
    {
        return {};
    }
    return output;
}

bool TopViewRenderPolicy::RenderInto(
    const TopViewFrame& frame,
    QImage* output) const
{
    if (output == nullptr
        || output->width() <= 0
        || output->height() <= 0
        || frame.instances.isEmpty())
    {
        return false;
    }
    if (output->format() != QImage::Format_RGBA8888)
    {
        *output = QImage(output->size(), QImage::Format_RGBA8888);
    }
    const QSize canvasSize = output->size();

    const QRectF renderViewport(
        kCanvasPadding,
        kHeaderHeight,
        canvasSize.width() - 2.0 * kCanvasPadding,
        canvasSize.height() - kHeaderHeight - kCanvasPadding);
    if (renderViewport.width() <= 1.0
        || renderViewport.height() <= 1.0)
    {
        return false;
    }

    const bool cachedPlan = !frame.viewDataIdentity.isEmpty()
        && m_renderPlanIdentity == frame.viewDataIdentity
        && m_renderPlanLocalRevision == frame.localRevision
        && m_renderPlanCanvasSize == canvasSize
        && m_renderPlanDestinations.size() == frame.instances.size();
    if (!cachedPlan)
    {
        double minX = (std::numeric_limits<double>::max)();
        double minY = (std::numeric_limits<double>::max)();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();
        QVector<QPolygonF> worldPolygons;
        worldPolygons.reserve(frame.instances.size());
        for (const TopViewInstance& instance : frame.instances)
        {
            const QPolygonF polygon = WorldCorners(instance);
            for (const QPointF& point : polygon)
            {
                minX = (std::min)(minX, point.x());
                minY = (std::min)(minY, point.y());
                maxX = (std::max)(maxX, point.x());
                maxY = (std::max)(maxY, point.y());
            }
            worldPolygons.push_back(polygon);
        }
        const double width = maxX - minX;
        const double height = maxY - minY;
        if (!(width > 0.0) || !(height > 0.0))
        {
            return false;
        }
        const double scale = 0.90 * (std::min)(
            renderViewport.width() / width,
            renderViewport.height() / height);
        const QPointF center(
            (minX + maxX) * 0.5,
            (minY + maxY) * 0.5);
        m_renderPlanDestinations.clear();
        m_renderPlanDestinations.reserve(frame.instances.size());
        for (const QPolygonF& worldPolygon : worldPolygons)
        {
            QPolygonF destination;
            destination.reserve(worldPolygon.size());
            for (const QPointF& world : worldPolygon)
            {
                destination << QPointF(
                    renderViewport.center().x()
                        + (world.x() - center.x()) * scale,
                    renderViewport.center().y()
                        - (world.y() - center.y()) * scale);
            }
            m_renderPlanDestinations.push_back(destination);
        }
        m_renderPlanIdentity = frame.viewDataIdentity;
        m_renderPlanLocalRevision = frame.localRevision;
        m_renderPlanCanvasSize = canvasSize;
    }

    output->fill(QColor(43, 45, 49));
    QPainter painter(output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (int index = 0; index < frame.instances.size(); ++index)
    {
        const TopViewInstance& instance = frame.instances.at(index);
        const QPolygonF& destination = m_renderPlanDestinations.at(index);
        if (destination.size() != 4)
        {
            continue;
        }
        const QPointF topLeft = destination.at(0);
        const QPointF topRight = destination.at(1);
        const QPointF bottomLeft = destination.at(3);
        const double sourceWidth = instance.surfacePreview.width();
        const double sourceHeight = instance.surfacePreview.height();
        const bool axisAligned =
            std::abs(topLeft.y() - topRight.y()) < 1.0e-9
            && std::abs(topLeft.x() - bottomLeft.x()) < 1.0e-9;
        if (axisAligned)
        {
            painter.drawImage(
                QRectF(topLeft, destination.at(2)).normalized(),
                instance.surfacePreview);
        }
        else
        {
            const QTransform transform(
                (topRight.x() - topLeft.x()) / sourceWidth,
                (topRight.y() - topLeft.y()) / sourceWidth,
                (bottomLeft.x() - topLeft.x()) / sourceHeight,
                (bottomLeft.y() - topLeft.y()) / sourceHeight,
                topLeft.x(),
                topLeft.y());
            painter.save();
            painter.setTransform(transform);
            painter.drawImage(QPointF(0.0, 0.0), instance.surfacePreview);
            painter.restore();
        }
        painter.setPen(QPen(QColor(23, 25, 28), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(destination);
    }
    return true;
}

int TopViewRenderPolicy::CachedPreviewCount() const
{
    return m_previewCache.size();
}

quint64 TopViewRenderPolicy::BlobReadCount() const
{
    return m_blobReadCount;
}
