#pragma once

#include "../services/PackageLoader.h"

#include <QComboBox>
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QVector>
#include <QWidget>

class PreviewOverlayPanel final : public QWidget {
    Q_OBJECT

public:
    explicit PreviewOverlayPanel(QWidget* parent = nullptr);
    void loadPackage(const PackageSummary& package);
    int imageCount() const;
    QStringList availableChannels() const;
    bool canComposeMode(const QString& mode) const;

    /**
     * @brief Return real layer indices that have at least one overlay preview source.
     * @return Ascending preview layer indices.
     */
    QVector<int> LayerIndices() const;

    /**
     * @brief Return the exact real layer requested by the shared workspace.
     * @return Real layer index, or -1 when no layer is selected.
     */
    int CurrentLayerIndex() const;

    /**
     * @brief Select an exact real layer without falling back to another layer.
     * @param layerIndex Target real layer index.
     * @return true when overlay data exists at that layer; false when the view keeps the layer but shows a missing state.
     */
    bool SelectLayer(int layerIndex);

    /**
     * @brief Return current status text for UI smoke tests.
     * @return Human-readable overlay status.
     */
    QString StatusForTest() const;

signals:
    /**
     * @brief Emitted when the user selects a different real layer.
     * @param layerIndex Selected real layer index.
     */
    void SigLayerIndexChanged(int layerIndex);

private slots:
    void updateImage();
    void OnLayerChanged(int value);
    void zoomIn();
    void zoomOut();
    void fitToWindow();

private:
    struct PreviewImage {
        QString path;
        QString channel;
        int layer{-1};
    };

    QString classifyChannel(const QString& path) const;
    QString normalizeChannel(const QString& channel) const;
    int parseLayer(const QString& path) const;
    void LoadLayerMetadata(const PackageSummary& package);
    void ReadLayerMetadataObject(const QJsonObject& root);
    QString BuildLayerSemanticSummary(const QJsonObject& object) const;
    void ReadSourcePolicyObject(const QJsonObject& root);
    void rebuildLayerSlider();
    QImage readImage(const QString& path) const;
    QImage FindImageForLayer(const QString& channel, int layer) const;
    QImage FindFirstImageForLayer(int layer) const;
    QImage composeCurrent() const;
    QImage composeForMode(const QString& mode, int index) const;
    QImage ComposeForLayer(const QString& mode, int layerIndex) const;
    void applyPixmap(const QImage& image);

    QVector<PreviewImage> images_;
    QVector<int> m_layerIndices;
    QHash<int, double> m_layerZMm;
    QHash<int, QString> m_layerSemanticSummary;
    QString m_sourcePolicySummary;
    int m_requestedLayerIndex{-1};
    double zoom_{1.0};
    bool fit_{true};

    QComboBox* mode_{nullptr};
    QSlider* layer_slider_{nullptr};
    QLabel* status_{nullptr};
    QLabel* image_label_{nullptr};
    QScrollArea* scroll_area_{nullptr};
};
