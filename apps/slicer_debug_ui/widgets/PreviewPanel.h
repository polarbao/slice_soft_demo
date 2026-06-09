#pragma once

#include "../services/PackageLoader.h"

#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QScrollArea>
#include <QSlider>
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
    QString channelFromPath(const QString& path) const;
    void rebuildChannelSelector();
    void rebuildVisibleList();
    void showCurrentImage();
    void applyPixmap();

    QStringList all_images_;
    QStringList visible_images_;
    QImage current_image_;
    double zoom_{1.0};
    bool fit_{true};

    QComboBox* channel_selector_{nullptr};
    QSlider* layer_slider_{nullptr};
    QLabel* status_{nullptr};
    QLabel* image_label_{nullptr};
    QScrollArea* scroll_area_{nullptr};
};
