#pragma once

#include "../services/PackageLoader.h"

#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
#include <QWidget>

class PreviewOverlayPanel final : public QWidget {
    Q_OBJECT

public:
    explicit PreviewOverlayPanel(QWidget* parent = nullptr);
    void loadPackage(const PackageSummary& package);
    int imageCount() const;
    QStringList availableChannels() const;
    bool canComposeMode(const QString& mode) const;

private slots:
    void updateImage();
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
    void rebuildLayerSlider();
    QImage readImage(const QString& path) const;
    QImage findImage(const QString& channel, int index) const;
    QImage composeCurrent() const;
    QImage composeForMode(const QString& mode, int index) const;
    void applyPixmap(const QImage& image);

    QVector<PreviewImage> images_;
    double zoom_{1.0};
    bool fit_{true};

    QComboBox* mode_{nullptr};
    QSlider* layer_slider_{nullptr};
    QLabel* status_{nullptr};
    QLabel* image_label_{nullptr};
    QScrollArea* scroll_area_{nullptr};
};
