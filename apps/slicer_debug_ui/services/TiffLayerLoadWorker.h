#pragma once

#include "slicer_core/preview/TiffLayerSource.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <memory>

using TiffLayerBufferPtr =
    std::shared_ptr<const slicer_core::RgbwsvLayerBuffer>;

Q_DECLARE_METATYPE(TiffLayerBufferPtr)

/**
 * @brief Asynchronously load manifest-authoritative production TIFF layers.
 */
class TiffLayerLoadWorker final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct an asynchronous loader around a shared core source.
     * @param source Shared TIFF layer source and cache.
     * @param parent QObject owner.
     */
    explicit TiffLayerLoadWorker(
        std::shared_ptr<slicer_core::TiffLayerSource> source,
        QObject* parent = nullptr);
    ~TiffLayerLoadWorker() override;

    /**
     * @brief Validate and activate one production package manifest.
     * @param manifestPath Path to package manifest.json.
     * @return True when indexing succeeds.
     */
    bool IndexPackage(const QString& manifestPath);

    /**
     * @brief Request one exact manifest layer on the global thread pool.
     * @param layerIndex Real manifest layer index.
     * @param consumerId Stable preview consumer identity.
     * @return Monotonic request generation.
     */
    quint64 RequestLayer(
        int layerIndex,
        const QString& consumerId);

    /**
     * @brief Cancel all logical requests and invalidate their generation.
     */
    void Cancel();

    /**
     * @brief Return the newest logical request generation.
     * @return Monotonic generation.
     */
    quint64 Generation() const;

signals:
    void SigLayerLoaded(
        quint64 generation,
        const QString& consumerId,
        int layerIndex,
        TiffLayerBufferPtr buffer,
        bool cacheHit);
    void SigLayerLoadFailed(
        quint64 generation,
        const QString& consumerId,
        int layerIndex,
        const QString& errorCode,
        const QString& message);

private:
    struct CallbackState;

    void EmitFailure(
        quint64 generation,
        const QString& consumerId,
        int layerIndex,
        const QString& errorCode,
        const QString& message);

    std::shared_ptr<slicer_core::TiffLayerSource> m_source;
    std::shared_ptr<CallbackState> m_callbackState;
    std::shared_ptr<std::atomic_bool> m_activeCancellation;
    quint64 m_generation{0U};
};
