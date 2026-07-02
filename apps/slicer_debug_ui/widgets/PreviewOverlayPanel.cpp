#include "PreviewOverlayPanel.h"

#include "../services/PreviewReportIndex.h"

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
#include <QSet>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QPushButton* makeButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setMinimumHeight(28);
    return button;
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
    m_layerIndices.clear();
    QSet<QString> seen;
    const auto append_image = [this, &seen](const PreviewImage& image) {
        if (image.path.isEmpty() || image.channel.isEmpty() || image.layer < 0) {
            return;
        }
        const QString key = QString("%1|%2|%3").arg(image.layer).arg(image.channel, image.path);
        if (seen.contains(key)) {
            return;
        }
        seen.insert(key);
        images_.push_back(image);
        if (!m_layerIndices.contains(image.layer)) {
            m_layerIndices.push_back(image.layer);
        }
    };

    PreviewReportIndex index;
    if (index.load(package.package_dir)) {
        for (const PreviewReportEntry& entry : index.entries()) {
            if (QFileInfo::exists(entry.path)) {
                append_image(
                    PreviewImage{
                        entry.path,
                        normalizeChannel(entry.channel.isEmpty() ? classifyChannel(entry.path) : entry.channel),
                        entry.layer_index});
            }
        }
    }
    if (images_.isEmpty()) {
        QStringList paths;
        paths = package.preview_paths;
        paths.sort();
        for (const QString& path : paths) {
            if (QFileInfo::exists(path)) {
                append_image(PreviewImage{path, classifyChannel(path), parseLayer(path)});
            }
        }
    }
    std::sort(m_layerIndices.begin(), m_layerIndices.end());
    rebuildLayerSlider();
    updateImage();
}

int PreviewOverlayPanel::imageCount() const {
    return images_.size();
}

QStringList PreviewOverlayPanel::availableChannels() const {
    QSet<QString> channels;
    for (const PreviewImage& image : images_) {
        channels.insert(image.channel);
    }
    QStringList result = channels.values();
    result.sort();
    return result;
}

bool PreviewOverlayPanel::canComposeMode(const QString& mode) const {
    if (mode == "单通道") {
        return imageCount() > 0;
    }
    QString overlay_channel;
    if (mode.contains("W")) {
        overlay_channel = "white";
    } else if (mode.contains("V")) {
        overlay_channel = "varnish";
    } else {
        overlay_channel = "support";
    }
    for (int i = 0; i < m_layerIndices.size(); ++i) {
        const int layer = m_layerIndices.at(i);
        if ((!FindImageForLayer("rgb", layer).isNull() || !FindImageForLayer(overlay_channel, layer).isNull())
            && !composeForMode(mode, i).isNull()) {
            return true;
        }
    }
    return false;
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

QString PreviewOverlayPanel::normalizeChannel(const QString& channel) const {
    const QString normalized = channel.toLower();
    if (normalized == "texture_rgb" || normalized == "model_rgb" || normalized == "true_rgb") {
        return "rgb";
    }
    if (normalized == "w") {
        return "white";
    }
    if (normalized == "v") {
        return "varnish";
    }
    if (normalized == "s") {
        return "support";
    }
    return normalized;
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
    layer_slider_->setMaximum(m_layerIndices.isEmpty() ? 0 : m_layerIndices.size() - 1);
    layer_slider_->setValue(0);
    layer_slider_->setEnabled(!m_layerIndices.isEmpty());
    layer_slider_->blockSignals(false);
}

QImage PreviewOverlayPanel::readImage(const QString& path) const {
    QImageReader reader(path);
    return reader.read();
}

QImage PreviewOverlayPanel::FindImageForLayer(const QString& channel, const int layer) const {
    if (images_.isEmpty()) {
        return {};
    }
    for (const PreviewImage& image : images_) {
        if (image.channel == channel && image.layer == layer) {
            return readImage(image.path);
        }
    }
    return {};
}

QImage PreviewOverlayPanel::FindFirstImageForLayer(const int layer) const {
    for (const PreviewImage& image : images_) {
        if (image.layer == layer) {
            return readImage(image.path);
        }
    }
    return {};
}

int PreviewOverlayPanel::CurrentLayer() const {
    if (m_layerIndices.isEmpty()) {
        return -1;
    }
    const int position = qBound(0, layer_slider_->value(), m_layerIndices.size() - 1);
    return m_layerIndices.at(position);
}

QImage PreviewOverlayPanel::composeCurrent() const {
    return composeForMode(mode_->currentText(), layer_slider_->value());
}

QImage PreviewOverlayPanel::composeForMode(const QString& mode, const int index) const {
    if (m_layerIndices.isEmpty()) {
        return {};
    }
    const int layer = m_layerIndices.at(qBound(0, index, m_layerIndices.size() - 1));
    if (mode == "单通道") {
        return FindFirstImageForLayer(layer);
    }

    QImage base = FindImageForLayer("rgb", layer);
    if (!base.isNull()) {
        base = base.convertToFormat(QImage::Format_ARGB32);
    }

    QString overlay_channel;
    if (mode.contains("W")) {
        overlay_channel = "white";
    } else if (mode.contains("V")) {
        overlay_channel = "varnish";
    } else {
        overlay_channel = "support";
    }
    const QImage overlay = FindImageForLayer(overlay_channel, layer);
    if (base.isNull() && overlay.isNull()) {
        return {};
    }
    if (base.isNull()) {
        base = QImage(overlay.size(), QImage::Format_ARGB32);
        base.fill(Qt::white);
    }
    if (overlay.isNull()) {
        return base;
    }

    QImage result = base.convertToFormat(QImage::Format_ARGB32);
    const QImage mask = overlay.convertToFormat(QImage::Format_ARGB32).scaled(result.size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    QImage colored_mask(result.size(), QImage::Format_ARGB32);
    colored_mask.fill(Qt::transparent);
    for (int y = 0; y < mask.height(); ++y) {
        for (int x = 0; x < mask.width(); ++x) {
            const QColor source = QColor::fromRgba(mask.pixel(x, y));
            const int max_component = qMax(source.red(), qMax(source.green(), source.blue()));
            const bool near_white = source.red() > 245 && source.green() > 245 && source.blue() > 245;
            if (!near_white && max_component >= 32) {
                colored_mask.setPixelColor(x, y, QColor(source.red(), source.green(), source.blue(), 170));
            }
        }
    }
    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(QPoint(0, 0), colored_mask);
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
    status_->setText(QString("%1/%2  layer=%3  %4  %5x%6")
                         .arg(layer_slider_->value() + 1)
                         .arg(qMax(1, m_layerIndices.size()))
                         .arg(CurrentLayer())
                         .arg(mode_->currentText())
                         .arg(image.width())
                         .arg(image.height()));
}
