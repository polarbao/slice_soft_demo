#pragma once

#include "../services/LayerPreviewDataProvider.h"
#include "../services/PackageLoader.h"
#include "../services/TiffLayerLoadWorker.h"

#include "slicer_core/preview/MaterialPreviewComposer.h"

#include <QColor>
#include <QImage>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QVector>
#include <QWidget>

#include <memory>

class QComboBox;

/**
 * @brief TIFF-native RGBWSV production layer preview.
 */
class LayerPreviewPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the production layer preview panel.
     * @param parent Qt parent widget.
     */
    explicit LayerPreviewPanel(QWidget* parent = nullptr);
    ~LayerPreviewPanel() override;

    /**
     * @brief Load a manifest-authoritative production package.
     * @param package Package summary produced by PackageLoader.
     */
    void LoadPackage(const PackageSummary& package);

    /**
     * @brief Return the number of manifest-listed production layers.
     * @return Production layer count.
     */
    int LayerCount() const;

    /**
     * @brief Return the available TIFF-native material preview mode ids.
     * @return Stable preview mode ids.
     */
    QStringList AvailableChannels() const;

    /**
     * @brief Return manifest layer indices in ascending low-Z order.
     * @return Manifest-authoritative layer indices.
     */
    QVector<int> LayerIndices() const;

    /**
     * @brief Return the selected real manifest layer index.
     * @return Real layer index, or -1 when no package is indexed.
     */
    int CurrentLayerIndex() const;

    /**
     * @brief Select an exact manifest layer.
     * @param layerIndex Target real layer index.
     * @return True when the exact layer exists.
     */
    bool SelectLayer(int layerIndex);

    /**
     * @brief Select an exact layer for smoke tests.
     * @param layerIndex Target real layer index.
     * @return True when the exact layer exists.
     */
    bool SelectLayerForTest(int layerIndex);

    /**
     * @brief Select a TIFF-native preview mode for smoke tests.
     * @param channel Stable mode id.
     * @return True when the mode exists.
     */
    bool SelectChannelForTest(const QString& channel);

    /**
     * @brief Return the current display image.
     * @return Current physical-source preview image.
     */
    QImage CurrentImageForTest() const;

    /**
     * @brief Return the exact six-channel probe text without publishing it.
     * @param x Display image X coordinate.
     * @param y Display image Y coordinate.
     * @return Production probe text, or empty when the layer is not ready.
     */
    QString PixelProbeForTest(int x, int y) const;

    /**
     * @brief Probe and publish one display pixel.
     * @param x Display image X coordinate.
     * @param y Display image Y coordinate.
     * @return Production probe text.
     */
    QString ProbePixelForTest(int x, int y);

    /**
     * @brief Return a configured display-only pseudo color.
     * @param channel white, support, varnish, occupancy, or empty.
     * @return Configured pseudo color.
     */
    QColor PseudoColor(const QString& channel) const;

    /**
     * @brief Return the physical-aspect-corrected base display size.
     * @return Physical display size before fit or zoom.
     */
    QSize PhysicalDisplaySizeForTest() const;

    /**
     * @brief Return current user-visible production preview status.
     * @return Status text.
     */
    QString StatusForTest() const;

    /**
     * @brief Return whether the selected layer buffer is ready.
     * @return True only for an exact current-layer TIFF buffer.
     */
    bool IsLayerReadyForTest() const;

    /**
     * @brief Return the loaded TIFF buffer layer index.
     * @return Loaded real layer index, or -1.
     */
    int LoadedLayerIndexForTest() const;

    /**
     * @brief Return the number of asynchronous layer requests.
     * @return Layer request count since construction.
     */
    quint64 LayerRequestCountForTest() const;

    /**
     * @brief Return the authoritative production preview source description.
     * @return Stable source description.
     */
    QString DataSourceForTest() const;

    /**
     * @brief Return whether the newest accepted layer came from cache.
     * @return True for a cache hit.
     */
    bool LastLoadWasCacheHitForTest() const;

signals:
    /**
     * @brief Emitted when the selected real package layer changes.
     * @param layerIndex Selected manifest layer index.
     */
    void SigLayerIndexChanged(int layerIndex);

    /**
     * @brief Emitted when the six-channel pixel probe changes.
     * @param context Exact production channel context, or empty.
     */
    void SigPixelProbeChanged(const QString& context);

    /**
     * @brief Emitted after the exact selected TIFF layer buffer is accepted.
     * @param buffer Immutable production RGBWSV layer shared with diagnostic views.
     */
    void SigLayerBufferReady(TiffLayerBufferPtr buffer);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void OnLayerChanged(int value);
    void OnChannelChanged(int index);
    void OnZoomIn();
    void OnZoomOut();
    void OnFitToWindow();
    void OnActualSize();
    void OnLayerLoaded(
        quint64 generation,
        const QString& consumerId,
        int layerIndex,
        TiffLayerBufferPtr buffer,
        bool cacheHit);
    void OnLayerLoadFailed(
        quint64 generation,
        const QString& consumerId,
        int layerIndex,
        const QString& errorCode,
        const QString& message);

private:
    QString CurrentChannel() const;
    double CurrentLayerZMm() const;
    void ApplyPackageIndex(
        const slicer_core::ProductionPackageIndex& index);
    void RebuildChannelSelector();
    void RebuildLayerSlider();
    void RequestCurrentLayer();
    void ComposeCurrentImage();
    void ApplyPixmap();
    void UpdateStatus(const QString& note = QString());
    QString BuildPixelProbeText(int displayX, int displayY) const;
    QString InterpretPixel(
        const slicer_core::MaterialPixelProbe& probe) const;
    QString BuildLayerSemanticText(
        const LayerPreviewLayerStats& stats) const;
    QString BuildSourcePolicyText() const;
    QString ApplyPixelProbe(int displayX, int displayY);
    void ClearPixelProbe();
    void ClearCurrentLayer();
    QSize PhysicalDisplaySize() const;

    LayerPreviewDataProvider m_provider;
    LayerPreviewPackage m_package;
    std::shared_ptr<slicer_core::TiffLayerSource> m_layerSource;
    TiffLayerLoadWorker* m_layerWorker{nullptr};
    TiffLayerBufferPtr m_currentBuffer;
    slicer_core::MaterialPreviewResult m_currentPreview;
    QImage m_currentImage;
    QString m_probeText;
    QString m_errorCode;
    QString m_errorMessage;
    quint64 m_expectedGeneration{0U};
    quint64 m_layerRequestCount{0U};
    int m_requestedLayerIndex{-1};
    double m_zoom{1.0};
    bool m_fit{true};
    bool m_loading{false};
    bool m_lastCacheHit{false};

    QComboBox* m_channelSelector{nullptr};
    QSlider* m_layerSlider{nullptr};
    QLabel* m_status{nullptr};
    QLabel* m_imageLabel{nullptr};
    QScrollArea* m_scrollArea{nullptr};
};
