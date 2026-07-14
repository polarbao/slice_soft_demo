#pragma once

#include "../services/PackageLoader.h"

#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QVector>
#include <QWidget>

class PreviewPanel final : public QWidget {
    Q_OBJECT

public:
    explicit PreviewPanel(QWidget* parent = nullptr);
    void loadPackage(const PackageSummary& package);

    /**
     * @brief Return real layer indices available for the current raw preview channel.
     * @return Ascending real layer indices.
     */
    QVector<int> LayerIndices() const;

    /**
     * @brief Return the exact real layer requested by the shared workspace.
     * @return Real layer index, or -1 when no layer is selected.
     */
    int CurrentLayerIndex() const;

    /**
     * @brief Select an exact real layer without choosing a nearby preview image.
     * @param layerIndex Target real layer index.
     * @return true when the current channel has an image at that layer; false when a same-layer missing state is shown.
     */
    bool SelectLayer(int layerIndex);

    /**
     * @brief Return current status text for UI smoke tests.
     * @return Human-readable raw preview status.
     */
    QString StatusForTest() const;

signals:
    /**
     * @brief Emitted when the user selects a different real layer.
     * @param layerIndex Selected real layer index.
     */
    void SigLayerIndexChanged(int layerIndex);

private slots:
    void selectImage(int index);
    void selectChannel(int index);
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    void actualSize();

private:
    struct PreviewItem
    {
        QString path;
        QString channel;
        int layer{-1};
    };

    QString normalizeChannel(const QString& channel, const QString& path) const;
    QString channelFromPath(const QString& path) const;
    int parseLayer(const QString& path) const;
    void appendPreviewItem(const PreviewItem& item);
    void rebuildChannelSelector();
    void rebuildVisibleList(int requestedLayerIndex = -1);
    PreviewItem FindItemForLayer(int layerIndex) const;
    void showCurrentImage();
    void applyPixmap();

    QVector<PreviewItem> all_items_;
    QVector<PreviewItem> visible_items_;
    QVector<int> m_layerIndices;
    int m_requestedLayerIndex{-1};
    QImage current_image_;
    double zoom_{1.0};
    bool fit_{true};

    QComboBox* channel_selector_{nullptr};
    QSlider* layer_slider_{nullptr};
    QLabel* status_{nullptr};
    QLabel* image_label_{nullptr};
    QScrollArea* scroll_area_{nullptr};
};
