#include "PreviewPanel.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

PreviewPanel::PreviewPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();

    channel_selector_ = new QComboBox(this);
    controls->addWidget(channel_selector_);

    layer_slider_ = new QSlider(Qt::Horizontal, this);
    controls->addWidget(layer_slider_, 1);

    auto* fit_button = new QPushButton("Fit", this);
    auto* actual_button = new QPushButton("1:1", this);
    auto* zoom_in_button = new QPushButton("+", this);
    auto* zoom_out_button = new QPushButton("-", this);
    controls->addWidget(fit_button);
    controls->addWidget(actual_button);
    controls->addWidget(zoom_in_button);
    controls->addWidget(zoom_out_button);
    layout->addLayout(controls);

    status_ = new QLabel("No preview package loaded.", this);
    layout->addWidget(status_);

    image_label_ = new QLabel(this);
    image_label_->setAlignment(Qt::AlignCenter);
    image_label_->setBackgroundRole(QPalette::Base);
    image_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidget(image_label_);
    scroll_area_->setWidgetResizable(true);
    layout->addWidget(scroll_area_, 1);

    connect(channel_selector_, qOverload<int>(&QComboBox::currentIndexChanged), this, &PreviewPanel::selectChannel);
    connect(layer_slider_, &QSlider::valueChanged, this, &PreviewPanel::selectImage);
    connect(fit_button, &QPushButton::clicked, this, &PreviewPanel::fitToWindow);
    connect(actual_button, &QPushButton::clicked, this, &PreviewPanel::actualSize);
    connect(zoom_in_button, &QPushButton::clicked, this, &PreviewPanel::zoomIn);
    connect(zoom_out_button, &QPushButton::clicked, this, &PreviewPanel::zoomOut);
}

void PreviewPanel::loadPackage(const PackageSummary& package) {
    all_images_ = package.preview_paths;
    all_images_.sort();
    rebuildChannelSelector();
    rebuildVisibleList();
}

void PreviewPanel::selectImage(const int index) {
    Q_UNUSED(index);
    showCurrentImage();
}

void PreviewPanel::selectChannel(const int index) {
    Q_UNUSED(index);
    rebuildVisibleList();
}

void PreviewPanel::zoomIn() {
    fit_ = false;
    zoom_ *= 1.25;
    applyPixmap();
}

void PreviewPanel::zoomOut() {
    fit_ = false;
    zoom_ /= 1.25;
    applyPixmap();
}

void PreviewPanel::fitToWindow() {
    fit_ = true;
    applyPixmap();
}

void PreviewPanel::actualSize() {
    fit_ = false;
    zoom_ = 1.0;
    applyPixmap();
}

QString PreviewPanel::channelFromPath(const QString& path) const {
    const QString base = QFileInfo(path).completeBaseName().toLower();
    const QStringList known{"texture_rgb", "rgb", "white", "w", "support", "s", "varnish", "v", "alpha"};
    for (const QString& token : known) {
        if (base.contains(token)) {
            return token;
        }
    }
    return "preview";
}

void PreviewPanel::rebuildChannelSelector() {
    QSet<QString> channels;
    for (const QString& path : all_images_) {
        channels.insert(channelFromPath(path));
    }
    QStringList sorted = channels.values();
    sorted.sort();
    channel_selector_->blockSignals(true);
    channel_selector_->clear();
    channel_selector_->addItem("all");
    for (const QString& channel : sorted) {
        channel_selector_->addItem(channel);
    }
    channel_selector_->blockSignals(false);
}

void PreviewPanel::rebuildVisibleList() {
    visible_images_.clear();
    const QString selected = channel_selector_->currentText();
    for (const QString& path : all_images_) {
        if (selected == "all" || channelFromPath(path) == selected) {
            visible_images_.push_back(path);
        }
    }
    layer_slider_->blockSignals(true);
    layer_slider_->setMinimum(0);
    layer_slider_->setMaximum(visible_images_.isEmpty() ? 0 : visible_images_.size() - 1);
    layer_slider_->setEnabled(!visible_images_.isEmpty());
    layer_slider_->setValue(0);
    layer_slider_->blockSignals(false);
    showCurrentImage();
}

void PreviewPanel::showCurrentImage() {
    if (visible_images_.isEmpty()) {
        current_image_ = QImage();
        image_label_->clear();
        status_->setText("No PNG/PPM preview found.");
        return;
    }
    const int index = layer_slider_->value();
    const QString path = visible_images_.at(index);
    QImageReader reader(path);
    current_image_ = reader.read();
    if (current_image_.isNull()) {
        image_label_->clear();
        status_->setText("Failed to read preview: " + path + " (" + reader.errorString() + ")");
        return;
    }
    status_->setText(QString("%1/%2  %3  %4x%5")
                         .arg(index + 1)
                         .arg(visible_images_.size())
                         .arg(QFileInfo(path).fileName())
                         .arg(current_image_.width())
                         .arg(current_image_.height()));
    applyPixmap();
}

void PreviewPanel::applyPixmap() {
    if (current_image_.isNull()) {
        return;
    }
    QSize target_size = current_image_.size();
    if (fit_) {
        target_size = current_image_.size().scaled(scroll_area_->viewport()->size(), Qt::KeepAspectRatio);
    } else {
        target_size = QSize(static_cast<int>(current_image_.width() * zoom_), static_cast<int>(current_image_.height() * zoom_));
    }
    image_label_->setPixmap(QPixmap::fromImage(current_image_).scaled(target_size, Qt::KeepAspectRatio, Qt::FastTransformation));
    image_label_->resize(target_size);
}
