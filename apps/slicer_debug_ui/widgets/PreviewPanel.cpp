#include "PreviewPanel.h"

#include "../services/PreviewReportIndex.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QVBoxLayout>

#include <algorithm>

namespace
{

QImage ToDisplayCoordinateImage(const QImage& image)
{
    if (image.isNull())
    {
        return image;
    }

    // Package pixels are interpreted in slicer coordinates; Qt displays images from the top-left.
    return image.mirrored(false, true);
}

int ChannelOrder(const QString& channel)
{
    if (channel == "RGB")
    {
        return 0;
    }
    if (channel == "纹理RGB")
    {
        return 1;
    }
    if (channel == "支撑")
    {
        return 2;
    }
    if (channel == "白墨")
    {
        return 3;
    }
    if (channel == "光油")
    {
        return 4;
    }
    return 10;
}

}  // namespace

PreviewPanel::PreviewPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();

    channel_selector_ = new QComboBox(this);
    channel_selector_->setToolTip("直接浏览输出包 preview 目录中的 PNG/PPM 调试图；它不是生产 TIFF 的六通道探针视图。");
    controls->addWidget(channel_selector_);

    layer_slider_ = new QSlider(Qt::Horizontal, this);
    layer_slider_->setToolTip("按当前通道的原始 preview 文件顺序浏览。生产层检查优先使用“层预览”。");
    controls->addWidget(layer_slider_, 1);

    auto* fit_button = new QPushButton("适应", this);
    auto* actual_button = new QPushButton("1:1", this);
    auto* zoom_in_button = new QPushButton("+", this);
    auto* zoom_out_button = new QPushButton("-", this);
    controls->addWidget(fit_button);
    controls->addWidget(actual_button);
    controls->addWidget(zoom_in_button);
    controls->addWidget(zoom_out_button);
    layout->addLayout(controls);

    status_ = new QLabel("尚未加载预览输出包。", this);
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
    all_items_.clear();

    PreviewReportIndex index;
    if (index.load(package.package_dir))
    {
        for (const PreviewReportEntry& entry : index.entries())
        {
            if (!QFileInfo::exists(entry.path))
            {
                continue;
            }
            appendPreviewItem(PreviewItem{entry.path, normalizeChannel(entry.channel, entry.path), entry.layer_index});
        }
    }

    if (all_items_.isEmpty())
    {
        QStringList paths = package.preview_paths;
        paths.sort();
        for (const QString& path : paths)
        {
            appendPreviewItem(PreviewItem{path, channelFromPath(path), parseLayer(path)});
        }
    }

    std::sort(all_items_.begin(), all_items_.end(), [](const PreviewItem& left, const PreviewItem& right) {
        if (left.layer != right.layer)
        {
            return left.layer < right.layer;
        }
        const int leftOrder = ChannelOrder(left.channel);
        const int rightOrder = ChannelOrder(right.channel);
        if (leftOrder != rightOrder)
        {
            return leftOrder < rightOrder;
        }
        return left.path < right.path;
    });
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
    if (base.contains("texture_rgb")) {
        return "纹理RGB";
    }
    if (base.contains("rgb")) {
        return "RGB";
    }
    if (base.contains("white") || base.contains("_w") || base.endsWith("w")) {
        return "白墨";
    }
    if (base.contains("support") || base.contains("_s") || base.endsWith("s")) {
        return "支撑";
    }
    if (base.contains("varnish") || base.contains("_v") || base.endsWith("v")) {
        return "光油";
    }
    if (base.contains("alpha")) {
        return "透明度";
    }
    return "预览";
}

QString PreviewPanel::normalizeChannel(const QString& channel, const QString& path) const
{
    const QString normalized = channel.trimmed().toLower();
    if (normalized == "texture_rgb")
    {
        return "纹理RGB";
    }
    if (normalized == "rgb" || normalized == "model_rgb" || normalized == "true_rgb")
    {
        return "RGB";
    }
    if (normalized == "white" || normalized == "w")
    {
        return "白墨";
    }
    if (normalized == "support" || normalized == "s")
    {
        return "支撑";
    }
    if (normalized == "varnish" || normalized == "v")
    {
        return "光油";
    }
    return channelFromPath(path);
}

int PreviewPanel::parseLayer(const QString& path) const
{
    const QString base = QFileInfo(path).completeBaseName();
    const QRegularExpression expression("(?:layer|z|_)(\\d+)");
    const QRegularExpressionMatch match = expression.match(base);
    if (match.hasMatch())
    {
        return match.captured(1).toInt();
    }

    const QRegularExpression digits("(\\d+)");
    const QRegularExpressionMatch fallback = digits.match(base);
    return fallback.hasMatch() ? fallback.captured(1).toInt() : -1;
}

void PreviewPanel::appendPreviewItem(const PreviewItem& item)
{
    if (item.path.isEmpty() || item.channel.isEmpty() || item.layer < 0)
    {
        return;
    }
    all_items_.push_back(item);
}

void PreviewPanel::rebuildChannelSelector() {
    QSet<QString> channels;
    for (const PreviewItem& item : all_items_) {
        channels.insert(item.channel);
    }
    QStringList sorted = channels.values();
    std::sort(sorted.begin(), sorted.end(), [](const QString& left, const QString& right) {
        const int leftOrder = ChannelOrder(left);
        const int rightOrder = ChannelOrder(right);
        if (leftOrder != rightOrder)
        {
            return leftOrder < rightOrder;
        }
        return left < right;
    });
    channel_selector_->blockSignals(true);
    channel_selector_->clear();
    channel_selector_->addItem("全部");
    for (const QString& channel : sorted) {
        channel_selector_->addItem(channel);
    }
    channel_selector_->blockSignals(false);
}

void PreviewPanel::rebuildVisibleList() {
    visible_items_.clear();
    const QString selected = channel_selector_->currentText();
    for (const PreviewItem& item : all_items_) {
        if (selected == "全部" || item.channel == selected) {
            visible_items_.push_back(item);
        }
    }
    layer_slider_->blockSignals(true);
    layer_slider_->setMinimum(0);
    layer_slider_->setMaximum(visible_items_.isEmpty() ? 0 : visible_items_.size() - 1);
    layer_slider_->setEnabled(!visible_items_.isEmpty());
    layer_slider_->setValue(0);
    layer_slider_->blockSignals(false);
    showCurrentImage();
}

void PreviewPanel::showCurrentImage() {
    if (visible_items_.isEmpty()) {
        current_image_ = QImage();
        image_label_->clear();
        status_->setText("未找到 PNG/PPM 预览图。");
        return;
    }
    const int index = layer_slider_->value();
    const PreviewItem item = visible_items_.at(index);
    const QString path = item.path;
    QImageReader reader(path);
    current_image_ = ToDisplayCoordinateImage(reader.read());
    if (current_image_.isNull()) {
        image_label_->clear();
        status_->setText("读取预览图失败：" + path + " (" + reader.errorString() + ")");
        return;
    }
    status_->setText(QString("%1/%2  layer=%3  %4  %5  %6x%7  显示=切片坐标")
                         .arg(index + 1)
                         .arg(visible_items_.size())
                         .arg(item.layer)
                         .arg(item.channel)
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
