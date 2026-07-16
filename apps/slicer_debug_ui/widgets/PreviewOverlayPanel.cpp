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

QImage ToDisplayCoordinateImage(const QImage& image)
{
    if (image.isNull())
    {
        return image;
    }

    // Package pixels are interpreted in slicer coordinates; Qt displays images from the top-left.
    return image.mirrored(false, true);
}

}  // namespace

PreviewOverlayPanel::PreviewOverlayPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();
    mode_ = new QComboBox(this);
    mode_->setObjectName(QStringLiteral("overlayModeSelector"));
    mode_->addItems(
        {"单通道",
         "RGB + W 白墨",
         "RGB + V 光油",
         "RGB + S 支撑",
         "RGB + 闭环 Gap"});
    mode_->setToolTip("选择叠加方式：仅把同一 layerIndex 的 RGB 与 W/S/V 伪彩图合成，用于检查材料相对位置。");
    layer_slider_ = new QSlider(Qt::Horizontal, this);
    layer_slider_->setToolTip("按真实 layerIndex 从低 Z 到高 Z 浏览叠加结果。");
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
    connect(layer_slider_, &QSlider::valueChanged, this, &PreviewOverlayPanel::OnLayerChanged);
    connect(zoom_in, &QPushButton::clicked, this, &PreviewOverlayPanel::zoomIn);
    connect(zoom_out, &QPushButton::clicked, this, &PreviewOverlayPanel::zoomOut);
    connect(fit, &QPushButton::clicked, this, &PreviewOverlayPanel::fitToWindow);
}

void PreviewOverlayPanel::loadPackage(const PackageSummary& package) {
    images_.clear();
    m_layerIndices.clear();
    m_layerZMm.clear();
    m_layerSemanticSummary.clear();
    m_sourcePolicySummary.clear();
    m_requestedLayerIndex = -1;
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
    LoadLayerMetadata(package);
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

bool PreviewOverlayPanel::canComposeMode(const QString& mode) const
{
    if (mode == "单通道")
    {
        return imageCount() > 0;
    }
    QString overlayChannel;
    if (mode.contains("闭环 Gap"))
    {
        overlayChannel = "closure_gap";
    }
    else if (mode.contains("W"))
    {
        overlayChannel = "white";
    }
    else if (mode.contains("V"))
    {
        overlayChannel = "varnish";
    }
    else
    {
        overlayChannel = "support";
    }
    for (int index{0}; index < m_layerIndices.size(); ++index)
    {
        const int layer = m_layerIndices.at(index);
        if (!FindImageForLayer(overlayChannel, layer).isNull()
            && !composeForMode(mode, index).isNull())
        {
            return true;
        }
    }
    return false;
}

QString PreviewOverlayPanel::StatusForTest() const
{
    return status_ == nullptr ? QString() : status_->text();
}

QVector<int> PreviewOverlayPanel::LayerIndices() const
{
    return m_layerIndices;
}

int PreviewOverlayPanel::CurrentLayerIndex() const
{
    if (m_requestedLayerIndex >= 0)
    {
        return m_requestedLayerIndex;
    }
    if (m_layerIndices.isEmpty())
    {
        return -1;
    }
    const int position = qBound(0, layer_slider_->value(), m_layerIndices.size() - 1);
    return m_layerIndices.at(position);
}

bool PreviewOverlayPanel::SelectLayer(const int layerIndex)
{
    const int position = m_layerIndices.indexOf(layerIndex);
    m_requestedLayerIndex = position < 0 ? layerIndex : -1;
    if (position < 0)
    {
        updateImage();
        return false;
    }
    if (layer_slider_->value() == position)
    {
        updateImage();
        return true;
    }
    layer_slider_->setValue(position);
    return true;
}

bool PreviewOverlayPanel::ShowMaterialClosureGapPreview(
    const int layerIndex,
    const QString& path)
{
    if (layerIndex < 0 || path.isEmpty() || !QFileInfo::exists(path))
    {
        return false;
    }

    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    bool exists = false;
    for (const PreviewImage& image : images_)
    {
        if (image.layer == layerIndex
            && image.channel == QStringLiteral("closure_gap")
            && QFileInfo(image.path).absoluteFilePath() == absolutePath)
        {
            exists = true;
            break;
        }
    }
    if (!exists)
    {
        images_.push_back(
            PreviewImage{
                absolutePath,
                QStringLiteral("closure_gap"),
                layerIndex});
    }
    if (!m_layerIndices.contains(layerIndex))
    {
        m_layerIndices.push_back(layerIndex);
        std::sort(m_layerIndices.begin(), m_layerIndices.end());
        rebuildLayerSlider();
    }

    const int modeIndex = mode_->findText(QStringLiteral("RGB + 闭环 Gap"));
    if (modeIndex >= 0)
    {
        mode_->setCurrentIndex(modeIndex);
    }
    return SelectLayer(layerIndex);
}

void PreviewOverlayPanel::updateImage() {
    const QImage image = composeCurrent();
    if (image.isNull()) {
        image_label_->clear();
        status_->setText(
            images_.isEmpty()
                ? "未找到 preview 图像。"
                : QString("layer=%1 当前模式同层无图；未跨层兜底。")
                      .arg(CurrentLayerIndex()));
        return;
    }
    applyPixmap(image);
}

void PreviewOverlayPanel::OnLayerChanged(const int value)
{
    Q_UNUSED(value);
    m_requestedLayerIndex = -1;
    updateImage();
    const int layerIndex = CurrentLayerIndex();
    if (layerIndex >= 0)
    {
        emit SigLayerIndexChanged(layerIndex);
    }
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

QString PreviewOverlayPanel::classifyChannel(const QString& path) const
{
    const QString base = QFileInfo(path).completeBaseName().toLower();
    if (base.contains("closure") && base.contains("gap"))
    {
        return "closure_gap";
    }
    if (base.contains("texture_rgb") || base.contains("rgb"))
    {
        return "rgb";
    }
    if (base.contains("white") || base.contains("_w") || base.endsWith("w"))
    {
        return "white";
    }
    if (base.contains("varnish") || base.contains("_v") || base.endsWith("v"))
    {
        return "varnish";
    }
    if (base.contains("support") || base.contains("_s") || base.endsWith("s"))
    {
        return "support";
    }
    return "preview";
}

QString PreviewOverlayPanel::normalizeChannel(const QString& channel) const
{
    const QString normalized = channel.toLower();
    if (normalized == "closure_gap" || normalized == "material_closure_gap")
    {
        return "closure_gap";
    }
    if (normalized == "texture_rgb" || normalized == "model_rgb" || normalized == "true_rgb")
    {
        return "rgb";
    }
    if (normalized == "w")
    {
        return "white";
    }
    if (normalized == "v")
    {
        return "varnish";
    }
    if (normalized == "s")
    {
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

void PreviewOverlayPanel::LoadLayerMetadata(const PackageSummary& package)
{
    const QStringList candidates{
        package.manifest_path,
        QDir(package.package_dir).filePath("reports/slice_report.json"),
    };
    for (const QString& path : candidates)
    {
        if (path.isEmpty())
        {
            continue;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            continue;
        }
        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            continue;
        }
        ReadLayerMetadataObject(document.object());
    }
}

void PreviewOverlayPanel::ReadLayerMetadataObject(const QJsonObject& root)
{
    ReadSourcePolicyObject(root);
    QJsonArray layers = root.value("layers").toArray();
    if (layers.isEmpty())
    {
        layers = root.value("tiff").toObject().value("layers").toArray();
    }
    for (const QJsonValue& value : layers)
    {
        const QJsonObject object = value.toObject();
        const int layer = object.value("layerIndex").toInt(object.value("index").toInt(-1));
        if (layer < 0 || !object.contains("zMm"))
        {
            continue;
        }
        m_layerZMm.insert(layer, object.value("zMm").toDouble());
        const QString semanticSummary = BuildLayerSemanticSummary(object);
        if (!semanticSummary.isEmpty())
        {
            m_layerSemanticSummary.insert(layer, semanticSummary);
        }
    }
}

QString PreviewOverlayPanel::BuildLayerSemanticSummary(const QJsonObject& object) const
{
    const QJsonObject semantic = object.value("semantic").toObject();
    const auto readCount = [&object, &semantic](const QString& key) -> int
    {
        if (semantic.contains(key))
        {
            return semantic.value(key).toInt(0);
        }
        return object.value(key).toInt(0);
    };

    QStringList parts;
    const int textureSurfacePixels = readCount("textureSurfacePixels");
    const int modelFillPixels = readCount("modelFillPixels");
    const int supportPixels = readCount("supportPixels");
    const int internalVoidSupportPixels = readCount("internalVoidSupportPixels");
    const int outerVarnishPixels = readCount("outerVarnishPixels");
    const int outerSurfaceVarnishPixels = readCount("outerSurfaceVarnishPixels");
    const int innerSurfaceVarnishPixels = readCount("innerSurfaceVarnishPixels");
    const int upperSurfaceSupportPixels = object.value("upperSurfaceSupportPixels").toInt(0);

    parts.push_back("TextureSurface=" + QString::number(textureSurfacePixels));
    parts.push_back("ModelFill=" + QString::number(modelFillPixels));
    parts.push_back("Support=" + QString::number(supportPixels));
    if (internalVoidSupportPixels > 0)
    {
        parts.push_back("InternalVoidS=" + QString::number(internalVoidSupportPixels));
    }
    if (upperSurfaceSupportPixels > 0)
    {
        parts.push_back("UpperS=" + QString::number(upperSurfaceSupportPixels));
    }
    if (outerVarnishPixels > 0)
    {
        parts.push_back("OuterV=" + QString::number(outerVarnishPixels));
    }
    if (outerSurfaceVarnishPixels > 0 || innerSurfaceVarnishPixels > 0)
    {
        parts.push_back(QString("SurfaceV(out/in)=%1/%2").arg(outerSurfaceVarnishPixels).arg(innerSurfaceVarnishPixels));
    }
    return "semantic: " + parts.join(", ");
}

void PreviewOverlayPanel::ReadSourcePolicyObject(const QJsonObject& root)
{
    if (!m_sourcePolicySummary.isEmpty())
    {
        return;
    }

    const QJsonObject materialSemantics = root.value("totals").toObject().value("materialSemantics").toObject();
    if (materialSemantics.isEmpty())
    {
        return;
    }

    QStringList parts;
    const QJsonObject modelFill = materialSemantics.value("modelFill").toObject();
    if (!modelFill.isEmpty())
    {
        parts.push_back(
            "modelFill=" + modelFill.value("material").toString("unknown") + "/" + modelFill.value("scope").toString("unknown"));
    }
    const QJsonObject supportPolicy = materialSemantics.value("supportPlacementPolicy").toObject();
    const QString supportPlacement = materialSemantics.value("supportPlacement").toString(supportPolicy.value("effective").toString());
    if (!supportPlacement.isEmpty())
    {
        QString supportText = "support=" + supportPlacement;
        const QString upperSource = supportPolicy.value("upperBoundarySource").toString();
        if (!upperSource.isEmpty())
        {
            supportText += "(" + upperSource + ")";
        }
        parts.push_back(supportText);
    }
    const QJsonObject internalVoidSupport = materialSemantics.value("internalVoidSupport").toObject();
    if (!internalVoidSupport.isEmpty())
    {
        parts.push_back(QString("internalVoid=%1").arg(internalVoidSupport.value("enabled").toBool(false) ? "on" : "off"));
    }
    const QJsonObject outerVarnish = materialSemantics.value("outerVarnish").toObject();
    if (outerVarnish.value("enabled").toBool(false))
    {
        parts.push_back(QString("outerVarnish=%1px").arg(outerVarnish.value("thicknessPx").toInt(0)));
    }
    const QJsonObject surfaceVarnish = materialSemantics.value("surfaceVarnish").toObject();
    if (surfaceVarnish.value("enabled").toBool(false))
    {
        parts.push_back(QString("surfaceVarnish=%1px").arg(surfaceVarnish.value("thicknessPx").toInt(0)));
    }
    const QString priority = materialSemantics.value("semanticPriority").toString();
    if (!priority.isEmpty())
    {
        parts.push_back("priority=" + priority);
    }
    m_sourcePolicySummary = parts.isEmpty() ? QString() : "sourcePolicy: " + parts.join(", ");
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
    return ToDisplayCoordinateImage(reader.read());
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

QImage PreviewOverlayPanel::composeCurrent() const {
    return ComposeForLayer(mode_->currentText(), CurrentLayerIndex());
}

QImage PreviewOverlayPanel::composeForMode(const QString& mode, const int index) const {
    if (m_layerIndices.isEmpty()) {
        return {};
    }
    const int layer = m_layerIndices.at(qBound(0, index, m_layerIndices.size() - 1));
    return ComposeForLayer(mode, layer);
}

QImage PreviewOverlayPanel::ComposeForLayer(const QString& mode, const int layer) const
{
    if (layer < 0)
    {
        return {};
    }
    if (mode == "单通道")
    {
        return FindFirstImageForLayer(layer);
    }

    QImage base = FindImageForLayer("rgb", layer);
    if (!base.isNull())
    {
        base = base.convertToFormat(QImage::Format_ARGB32);
    }

    QString overlayChannel;
    if (mode.contains("闭环 Gap"))
    {
        overlayChannel = "closure_gap";
    }
    else if (mode.contains("W"))
    {
        overlayChannel = "white";
    }
    else if (mode.contains("V"))
    {
        overlayChannel = "varnish";
    }
    else
    {
        overlayChannel = "support";
    }
    const QImage overlay = FindImageForLayer(overlayChannel, layer);
    if (base.isNull() && overlay.isNull())
    {
        return {};
    }
    if (base.isNull())
    {
        base = QImage(overlay.size(), QImage::Format_ARGB32);
        base.fill(Qt::white);
    }
    if (overlay.isNull())
    {
        return base;
    }

    QImage result = base.convertToFormat(QImage::Format_ARGB32);
    const QImage mask = overlay.convertToFormat(QImage::Format_ARGB32).scaled(result.size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    QImage coloredMask(result.size(), QImage::Format_ARGB32);
    coloredMask.fill(Qt::transparent);
    for (int y{0}; y < mask.height(); ++y)
    {
        for (int x{0}; x < mask.width(); ++x)
        {
            const QColor source = QColor::fromRgba(mask.pixel(x, y));
            const int maxComponent = qMax(source.red(), qMax(source.green(), source.blue()));
            const bool nearWhite = source.red() > 245 && source.green() > 245 && source.blue() > 245;
            if (!nearWhite && maxComponent >= 32)
            {
                coloredMask.setPixelColor(x, y, QColor(source.red(), source.green(), source.blue(), 170));
            }
        }
    }
    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(QPoint(0, 0), coloredMask);
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
    const int layer = CurrentLayerIndex();
    QString layerText = QString("layer=%1").arg(layer);
    if (m_layerZMm.contains(layer))
    {
        layerText += QString(" z=%1mm").arg(m_layerZMm.value(layer), 0, 'f', 3);
    }
    status_->setText(QString("%1/%2  %3  %4  %5x%6  层序=低Z->高Z  显示=切片坐标")
                         .arg(qMax(1, m_layerIndices.indexOf(layer) + 1))
                         .arg(qMax(1, m_layerIndices.size()))
                         .arg(layerText)
                         .arg(mode_->currentText())
                         .arg(image.width())
                         .arg(image.height())
                     + (m_layerSemanticSummary.contains(layer) ? "  " + m_layerSemanticSummary.value(layer) : QString())
                     + (m_sourcePolicySummary.isEmpty() ? QString() : "  " + m_sourcePolicySummary));
}
