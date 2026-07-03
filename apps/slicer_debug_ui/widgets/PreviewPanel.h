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
    void rebuildVisibleList();
    void showCurrentImage();
    void applyPixmap();

    QVector<PreviewItem> all_items_;
    QVector<PreviewItem> visible_items_;
    QImage current_image_;
    double zoom_{1.0};
    bool fit_{true};

    QComboBox* channel_selector_{nullptr};
    QSlider* layer_slider_{nullptr};
    QLabel* status_{nullptr};
    QLabel* image_label_{nullptr};
    QScrollArea* scroll_area_{nullptr};
};
