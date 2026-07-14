#pragma once

#include "../services/LayerPreviewDataProvider.h"
#include "../services/PackageLoader.h"

#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QVector>
#include <QWidget>

class LayerPreviewPanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the layer preview panel.
     * @param parent Qt parent widget.
     */
    explicit LayerPreviewPanel(QWidget* parent = nullptr);

    /**
     * @brief Load preview frames from an output package.
     * @param package Package summary produced by PackageLoader.
     */
    void LoadPackage(const PackageSummary& package);

    /**
     * @brief Return loaded layer count for smoke tests.
     * @return Number of layers known to the panel.
     */
    int LayerCount() const;

    /**
     * @brief Return loaded UI channel ids for smoke tests.
     * @return Available channel ids.
     */
    QStringList AvailableChannels() const;

    /**
     * @brief Return the real package layer indices available to this view.
     * @return Ascending package layer indices.
     */
    QVector<int> LayerIndices() const;

    /**
     * @brief Return the real layer index currently displayed.
     * @return Package layer index, or -1 when no package is loaded.
     */
    int CurrentLayerIndex() const;

    /**
     * @brief Select an exact real package layer.
     * @param layerIndex Target package layer index.
     * @return true when the exact layer exists and was selected.
     */
    bool SelectLayer(int layerIndex);

    /**
     * @brief Select a layer by package layer index for smoke tests.
     * @param layerindex Target package layer index.
     * @return True when the layer exists and was selected.
     */
    bool SelectLayerForTest(int layerIndex);

    /**
     * @brief Select a channel by UI channel id for smoke tests.
     * @param channel Target UI channel id.
     * @return True when the channel exists and was selected.
     */
    bool SelectChannelForTest(const QString& channel);

    /**
     * @brief Return the currently rendered image for smoke tests.
     * @return Current display image.
     */
    QImage CurrentImageForTest() const;

    /**
     * @brief Return production RGBWSV probe text for a display pixel in smoke tests.
     * @param x Display image x coordinate.
     * @param y Display image y coordinate.
     * @return Human-readable probe text, or an empty string when no production TIFF exists.
     */
    QString PixelProbeForTest(int x, int y) const;

signals:
    /**
     * @brief Emitted when the user selects a different real package layer.
     * @param layerIndex Selected package layer index.
     */
    void SigLayerIndexChanged(int layerIndex);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void OnLayerChanged(int value);
    void OnChannelChanged(int index);
    void OnZoomIn();
    void OnZoomOut();
    void OnFitToWindow();
    void OnActualSize();

private:
    QString CurrentChannel() const;
    LayerPreviewFrame FindFrame(int layerIndex, const QString& channel) const;
    QImage ReadFrameImage(const LayerPreviewFrame& frame) const;
    QImage RenderCurrentImage() const;
    QImage RenderProductionRgb(int layerIndex) const;
    QImage RenderMaskChannel(int layerIndex, const QString& channel) const;
    QImage RenderOccupancy(int layerIndex) const;
    QImage RenderDiagnostic(int layerIndex) const;
    QImage BlankCanvas() const;
    QImage ApplyPseudoColor(const QImage& source, const QColor& color) const;
    void RebuildChannelSelector();
    void RebuildLayerSlider();
    void UpdateImage();
    void ApplyPixmap();
    void UpdateStatus(const QString& note = QString());
    bool IsPrintedPixel(const QColor& color) const;
    QString BuildPixelProbeText(int displayX, int displayY) const;
    QString InterpretPixel(int r, int g, int b, int w, int s, int v) const;
    QString BuildLayerSemanticText(const LayerPreviewLayerStats& stats) const;
    QString BuildSourcePolicyText() const;

    LayerPreviewDataProvider m_provider;
    LayerPreviewPackage m_package;
    QImage m_currentImage;
    QString m_probeText;
    double m_zoom{1.0};
    bool m_fit{true};

    QComboBox* m_channelSelector{nullptr};
    QSlider* m_layerSlider{nullptr};
    QLabel* m_status{nullptr};
    QLabel* m_imageLabel{nullptr};
    QScrollArea* m_scrollArea{nullptr};
};
