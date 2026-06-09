#include "PreviewOverlayPanel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

QPushButton* makeButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setMinimumHeight(28);
    return button;
}

QStringList reportPreviewFiles(const QString& package_dir) {
    QStringList paths;
    QFile file(QDir(package_dir).filePath("reports/preview_report.json"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return paths;
    }
    QJsonParseError parse_error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        return paths;
    }
    const QJsonObject object = document.object();
    for (const QString& key : QStringList{"files", "generated", "previewFiles"}) {
        const QJsonArray array = object.value(key).toArray();
        for (const QJsonValue& value : array) {
            if (value.isString()) {
                paths.push_back(QDir(package_dir).filePath(value.toString()));
            } else if (value.isObject()) {
                const QString path = value.toObject().value("path").toString(value.toObject().value("file").toString());
                if (!path.isEmpty()) {
                    paths.push_back(QDir(package_dir).filePath(path));
                }
            }
        }
    }
    return paths;
}

}  // namespace

PreviewOverlayPanel::PreviewOverlayPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();
    mode_ = new QComboBox(this);
    mode_->addItems({"单通道", "RGB + W 白墨", "RGB + V 光油", "RGB + S 支撑"});
    layer_slider_ = new QSlider(Qt::Horizontal, this);
    controls->addWidget(mode_);
    controls->addWidget(layer_slider_, 1);
    controls->addWidget(makeButton("+", this));
    auto* zoom_out = makeButton("-", this);
    auto* fit = makeButton("适应", this);
    controls->addWidget(zoom_out);
    controls->addWidget(fit);
    layout->addLayout(controls);

    status_ = new QLabel("尚未加载 preview。", this);
    status_->setWordWrap(true);
    layout->addWidget(status_);

    image_label_ = new QLabel(this);
    image_label_->setAlignment(Qt::AlignCenter);
    image_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidget(image_label_);
    scroll_area_->setWidgetResizable(true);
    layout->addWidget(scroll_area_, 1);

    auto* zoom_in = qobject_cast<QPushButton*>(controls->itemAt(2)->widget());
    connect(mode_, qOverload<int>(&QComboBox::currentIndexChanged), this, &PreviewOverlayPanel::updateImage);
    connect(layer_slider_, &QSlider::valueChanged, this, &PreviewOverlayPanel::updateImage);
    connect(zoom_in, &QPushButton::clicked, this, &PreviewOverlayPanel::zoomIn);
    connect(zoom_out, &QPushButton::clicked, this, &PreviewOverlayPanel::zoomOut);
    connect(fit, &QPushButton::clicked, this, &PreviewOverlayPanel::fitToWindow);
}

void PreviewOverlayPanel::loadPackage(const PackageSummary& package) {
    images_.clear();
    QStringList paths = reportPreviewFiles(package.package_dir);
    if (paths.isEmpty()) {
        paths = package.preview_paths;
    }
    paths.sort();
    for (const QString& path : paths) {
        if (QFileInfo::exists(path)) {
            images_.push_back(PreviewImage{path, classifyChannel(path), parseLayer(path)});
        }
    }
    rebuildLayerSlider();
    updateImage();
}

void PreviewOverlayPanel::updateImage() {
    const QImage image = composeCurrent();
    if (image.isNull()) {
        image_label_->clear();
        status_->setText(images_.isEmpty() ? "未找到 preview 图像。" : "当前层/模式没有可显示图像。");
        return;
    }
    applyPixmap(image);
}

void PreviewOverlayPanel::zoomIn() {
    fit_ = false;
    zoom_ *= 1.25;
    updateImage();
}

void PreviewOverlayPanel::zoomOut() {
    fit_ = false;
    zoom_ /= 1.25;
    updateImage();
}

void PreviewOverlayPanel::fitToWindow() {
    fit_ = true;
    updateImage();
}

QString PreviewOverlayPanel::classifyChannel(const QString& path) const {
    const QString base = QFileInfo(path).completeBaseName().toLower();
    if (base.contains("texture_rgb") || base.contains("rgb")) {
        return "rgb";
    }
    if (base.contains("white") || base.contains("_w") || base.endsWith("w")) {
        return "white";
    }
    if (base.contains("varnish") || base.contains("_v") || base.endsWith("v")) {
        return "varnish";
    }
    if (base.contains("support") || base.contains("_s") || base.endsWith("s")) {
        return "support";
    }
    return "preview";
}

int PreviewOverlayPanel::parseLayer(const QString& path) const {
    const QString base = QFileInfo(path).completeBaseName();
    const QRegularExpression expression("(?:layer|z|_)(\\d+)");
    const QRegularExpressionMatch match = expression.match(base);
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }
    const QRegularExpression digits("(\\d+)");
    const QRegularExpressionMatch fallback = digits.match(base);
    return fallback.hasMatch() ? fallback.captured(1).toInt() : -1;
}

void PreviewOverlayPanel::rebuildLayerSlider() {
    layer_slider_->blockSignals(true);
    layer_slider_->setMinimum(0);
    layer_slider_->setMaximum(images_.isEmpty() ? 0 : images_.size() - 1);
    layer_slider_->setValue(0);
    layer_slider_->setEnabled(!images_.isEmpty());
    layer_slider_->blockSignals(false);
}

QImage PreviewOverlayPanel::readImage(const QString& path) const {
    QImageReader reader(path);
    return reader.read();
}

QImage PreviewOverlayPanel::findImage(const QString& channel, const int index) const {
    if (images_.isEmpty()) {
        return {};
    }
    const int target = qBound(0, index, images_.size() - 1);
    const int layer = images_.at(target).layer;
    for (const PreviewImage& image : images_) {
        if (image.channel == channel && image.layer == layer) {
            return readImage(image.path);
        }
    }
    int seen = -1;
    for (const PreviewImage& image : images_) {
        if (image.channel == channel) {
            ++seen;
            if (seen == target) {
                return readImage(image.path);
            }
        }
    }
    return {};
}

QImage PreviewOverlayPanel::composeCurrent() const {
    if (images_.isEmpty()) {
        return {};
    }
    const int index = layer_slider_->value();
    const QString mode = mode_->currentText();
    QImage base = mode == "单通道" ? readImage(images_.at(qBound(0, index, images_.size() - 1)).path) : findImage("rgb", index);
    if (base.isNull()) {
        base = readImage(images_.at(qBound(0, index, images_.size() - 1)).path);
    }
    if (mode == "单通道" || base.isNull()) {
        return base;
    }

    QString overlay_channel;
    QColor tint;
    if (mode.contains("W")) {
        overlay_channel = "white";
        tint = QColor(0, 210, 170, 160);
    } else if (mode.contains("V")) {
        overlay_channel = "varnish";
        tint = QColor(210, 60, 220, 160);
    } else {
        overlay_channel = "support";
        tint = QColor(255, 150, 20, 170);
    }
    const QImage overlay = findImage(overlay_channel, index);
    if (overlay.isNull()) {
        return base;
    }

    QImage result = base.convertToFormat(QImage::Format_ARGB32);
    QImage mask = overlay.convertToFormat(QImage::Format_ARGB32).scaled(result.size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(0.45);
    painter.drawImage(QPoint(0, 0), mask);
    painter.setOpacity(0.22);
    painter.fillRect(result.rect(), tint);
    return result;
}

void PreviewOverlayPanel::applyPixmap(const QImage& image) {
    QSize target_size = image.size();
    if (fit_) {
        target_size = image.size().scaled(scroll_area_->viewport()->size(), Qt::KeepAspectRatio);
    } else {
        target_size = QSize(static_cast<int>(image.width() * zoom_), static_cast<int>(image.height() * zoom_));
    }
    image_label_->setPixmap(QPixmap::fromImage(image).scaled(target_size, Qt::KeepAspectRatio, Qt::FastTransformation));
    image_label_->resize(target_size);
    status_->setText(QString("%1/%2  %3  %4x%5")
                         .arg(layer_slider_->value() + 1)
                         .arg(qMax(1, images_.size()))
                         .arg(mode_->currentText())
                         .arg(image.width())
                         .arg(image.height()));
}
