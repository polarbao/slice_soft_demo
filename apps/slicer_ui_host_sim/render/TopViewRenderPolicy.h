#pragma once

#include "../ModuleClient.h"

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QPolygonF>
#include <QSize>
#include <QString>
#include <QVector>

#include <array>

/** @brief Local XY bounds carried by one top-view preview. */
struct TopViewBounds final
{
    double minX{0.0};
    double minY{0.0};
    double maxX{0.0};
    double maxY{0.0};
};

/** @brief One host-owned top-view instance and its decoded surface preview. */
struct TopViewInstance final
{
    QString instanceId;
    QString previewIdentity;
    QString appearanceIdentity;
    QString textureStatus;
    TopViewBounds localBoundsMm;
    std::array<double, 16> worldMatrix{};
    QImage surfacePreview;
};

/** @brief Immutable-by-convention local rendering snapshot for one scene revision. */
struct TopViewFrame final
{
    QString viewDataIdentity;
    quint64 sceneRevision{0};
    quint64 localRevision{0};
    QVector<TopViewInstance> instances;
};

/**
 * @brief Fetches frozen top ViewData and renders it without implementation links.
 *
 * Preview pixels are cached by previewIdentity. Instance world matrices remain
 * outside that cache so host-local movement never invalidates texture pixels.
 */
class TopViewRenderPolicy final
{
public:
    /**
     * @brief Creates a policy over one loaded public module client.
     * @param client Runtime-loaded public ABI client.
     */
    explicit TopViewRenderPolicy(ModuleClient& client);

    /**
     * @brief Refreshes top ViewData for an authoritative scene revision.
     * @param sceneHandle Module-owned scene handle.
     * @param sceneRevision Expected authoritative revision.
     * @param frame Receives decoded previews and local placement data.
     * @param error Receives a fail-closed DTO or blob diagnostic.
     * @return True when every required surface preview is available.
     */
    bool Refresh(
        quint64 sceneHandle,
        quint64 sceneRevision,
        TopViewFrame* frame,
        QString* error);

    /**
     * @brief Renders a host-local orthographic top frame.
     * @param frame Previously decoded local frame.
     * @param canvasSize Requested output pixel size.
     * @return Display-only RGBA image, or null when input is invalid.
     * @note Use RenderInto for retained high-frequency repainting.
     */
    QImage Render(
        const TopViewFrame& frame,
        const QSize& canvasSize) const;

    /**
     * @brief Renders into a caller-owned image for retained high-FPS repaint.
     * @param frame Previously decoded local frame.
     * @param output Persistent RGBA image whose size defines the viewport.
     * @return True when a complete textured frame was rendered.
     * @note Call from the owning UI thread; no public module call is made.
     */
    bool RenderInto(
        const TopViewFrame& frame,
        QImage* output) const;

    /**
     * @brief Returns the number of decoded preview cache entries.
     * @return Current preview cache size.
     */
    int CachedPreviewCount() const;

    /**
     * @brief Returns the number of blob chunks fetched through the public ABI.
     * @return Monotonic chunk read count.
     */
    quint64 BlobReadCount() const;

private:
    bool ExecuteJson(
        const QJsonObject& request,
        QJsonObject* result,
        QString* error);
    bool DecodeInstance(
        const QJsonObject& value,
        const QHash<QString, bool>& appearances,
        TopViewInstance* instance,
        QString* error);
    bool LoadPreview(
        const QJsonObject& descriptor,
        QImage* image,
        QString* error);
    bool ReadBlob(
        const QString& blobId,
        int chunkCount,
        qint64 totalBytes,
        QByteArray* bytes,
        QString* error);

    ModuleClient& m_client;
    QHash<QString, QImage> m_previewCache;
    mutable QString m_renderPlanIdentity;
    mutable quint64 m_renderPlanLocalRevision{0};
    mutable QSize m_renderPlanCanvasSize;
    mutable QVector<QPolygonF> m_renderPlanDestinations;
    quint64 m_blobReadCount{0};
};
