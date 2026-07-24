#include "LayerPreviewPanel.h"

#include "slicer_core/tiff_io.h"

#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace
{

QPushButton* MakeButton(const QString& text, QWidget* parent)
{
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

LayerPreviewPanel::LayerPreviewPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();

    m_channelSelector = new QComboBox(this);
    m_channelSelector->setToolTip("选择生产层视图通道。production RGB 会读取 TIFF RGBWSV 数据，点击图像可查看六通道像素探针。");
    m_layerSlider = new QSlider(Qt::Horizontal, this);
    m_layerSlider->setToolTip("按真实 layerIndex 从低 Z 到高 Z 浏览层数据。");
    auto* fitButton = MakeButton("适应", this);
    auto* actualButton = MakeButton("1:1", this);
    auto* zoomInButton = MakeButton("+", this);
    auto* zoomOutButton = MakeButton("-", this);

    controls->addWidget(new QLabel("通道", this));
    controls->addWidget(m_channelSelector);
    controls->addWidget(new QLabel("层", this));
    controls->addWidget(m_layerSlider, 1);
    controls->addWidget(fitButton);
    controls->addWidget(actualButton);
    controls->addWidget(zoomInButton);
    controls->addWidget(zoomOutButton);
    layout->addLayout(controls);

    m_status = new QLabel("尚未加载层预览输出包。", this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->setWidgetResizable(true);
    layout->addWidget(m_scrollArea, 1);
    m_imageLabel->installEventFilter(this);

    connect(m_layerSlider, &QSlider::valueChanged, this, &LayerPreviewPanel::OnLayerChanged);
    connect(m_channelSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, &LayerPreviewPanel::OnChannelChanged);
    connect(zoomInButton, &QPushButton::clicked, this, &LayerPreviewPanel::OnZoomIn);
    connect(zoomOutButton, &QPushButton::clicked, this, &LayerPreviewPanel::OnZoomOut);
    connect(fitButton, &QPushButton::clicked, this, &LayerPreviewPanel::OnFitToWindow);
    connect(actualButton, &QPushButton::clicked, this, &LayerPreviewPanel::OnActualSize);
}

void LayerPreviewPanel::LoadPackage(const PackageSummary& package)
{
    m_package = m_provider.Load(package);
    m_zoom = 1.0;
    m_fit = true;
    ClearPixelProbe();
    RebuildChannelSelector();
    RebuildLayerSlider();
    UpdateImage();
}

int LayerPreviewPanel::LayerCount() const
{
    return m_package.layercount;
}

QStringList LayerPreviewPanel::AvailableChannels() const
{
    return m_package.channels;
}

QVector<int> LayerPreviewPanel::LayerIndices() const
{
    return m_package.layerindices;
}

bool LayerPreviewPanel::SelectLayer(const int layerIndex)
{
    const int position = m_package.layerindices.indexOf(layerIndex);
    if (position < 0)
    {
        return false;
    }
    if (m_layerSlider->value() == position)
    {
        UpdateImage();
        return true;
    }
    m_layerSlider->setValue(position);
    return true;
}

bool LayerPreviewPanel::SelectLayerForTest(const int layerIndex)
{
    return SelectLayer(layerIndex);
}

bool LayerPreviewPanel::SelectChannelForTest(const QString& channel)
{
    for (int index = 0; index < m_channelSelector->count(); ++index)
    {
        if (m_channelSelector->itemData(index).toString() == channel)
        {
            m_channelSelector->setCurrentIndex(index);
            return true;
        }
    }
    return false;
}

QImage LayerPreviewPanel::CurrentImageForTest() const
{
    return m_currentImage;
}

QString LayerPreviewPanel::PixelProbeForTest(const int x, const int y) const
{
    return BuildPixelProbeText(x, y);
}

QString LayerPreviewPanel::ProbePixelForTest(const int x, const int y)
{
    return ApplyPixelProbe(x, y);
}

QColor LayerPreviewPanel::PseudoColor(const QString& channel) const
{
    return m_package.pseudocolors.value(channel, QColor(255, 255, 255));
}

QSize LayerPreviewPanel::PhysicalDisplaySizeForTest() const
{
    return PhysicalDisplaySize();
}

QString LayerPreviewPanel::StatusForTest() const
{
    return m_status == nullptr ? QString{} : m_status->text();
}

bool LayerPreviewPanel::eventFilter(QObject* object, QEvent* event)
{
    if (object == m_imageLabel && event->type() == QEvent::MouseButtonPress)
    {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (m_imageLabel->width() > 0 && m_imageLabel->height() > 0 && !m_currentImage.isNull())
        {
            const int displayX = qBound(0, mouseEvent->pos().x() * m_currentImage.width() / m_imageLabel->width(), m_currentImage.width() - 1);
            const int displayY = qBound(0, mouseEvent->pos().y() * m_currentImage.height() / m_imageLabel->height(), m_currentImage.height() - 1);
            ApplyPixelProbe(displayX, displayY);
        }
    }
    return QWidget::eventFilter(object, event);
}

void LayerPreviewPanel::OnLayerChanged(const int value)
{
    Q_UNUSED(value);
    ClearPixelProbe();
    UpdateImage();
    const int layerIndex = CurrentLayerIndex();
    if (layerIndex >= 0)
    {
        emit SigLayerIndexChanged(layerIndex);
    }
}

void LayerPreviewPanel::OnChannelChanged(const int index)
{
    Q_UNUSED(index);
    ClearPixelProbe();
    UpdateImage();
}

void LayerPreviewPanel::OnZoomIn()
{
    m_fit = false;
    m_zoom *= 1.25;
    ApplyPixmap();
}

void LayerPreviewPanel::OnZoomOut()
{
    m_fit = false;
    m_zoom /= 1.25;
    ApplyPixmap();
}

void LayerPreviewPanel::OnFitToWindow()
{
    m_fit = true;
    ApplyPixmap();
}

void LayerPreviewPanel::OnActualSize()
{
    m_fit = false;
    m_zoom = 1.0;
    ApplyPixmap();
}

QString LayerPreviewPanel::CurrentChannel() const
{
    return m_channelSelector->currentData().toString();
}

int LayerPreviewPanel::CurrentLayerIndex() const
{
    if (m_package.layerindices.isEmpty())
    {
        return -1;
    }
    const int position = qBound(0, m_layerSlider->value(), m_package.layerindices.size() - 1);
    return m_package.layerindices.at(position);
}

LayerPreviewFrame LayerPreviewPanel::FindFrame(const int layerIndex, const QString& channel) const
{
    return m_package.frames.value(layerIndex).value(channel);
}

QImage LayerPreviewPanel::ReadFrameImage(const LayerPreviewFrame& frame) const
{
    if (frame.path.isEmpty() || !QFileInfo::exists(frame.path))
    {
        return {};
    }
    QImageReader reader(frame.path);
    return ToDisplayCoordinateImage(reader.read());
}

QImage LayerPreviewPanel::RenderCurrentImage() const
{
    const int layerIndex = CurrentLayerIndex();
    const QString channel = CurrentChannel();
    if (layerIndex < 0 || channel.isEmpty())
    {
        return {};
    }

    if (channel == "white" || channel == "support" || channel == "varnish")
    {
        return RenderMaskChannel(layerIndex, channel);
    }
    if (channel == "production_rgb")
    {
        return RenderProductionRgb(layerIndex);
    }
    if (channel == "occupancy")
    {
        return RenderOccupancy(layerIndex);
    }
    if (channel == "diagnostic")
    {
        return RenderDiagnostic(layerIndex);
    }

    const QImage image = ReadFrameImage(FindFrame(layerIndex, channel));
    return image.isNull() ? BlankCanvas() : image;
}

QImage LayerPreviewPanel::RenderProductionRgb(const int layerIndex) const
{
    const LayerPreviewFrame frame = FindFrame(layerIndex, "production_rgb");
    if (frame.path.isEmpty() || !QFileInfo::exists(frame.path))
    {
        return BlankCanvas();
    }

    try
    {
        const slicer_core::TiffReadResult result = slicer_core::read_rgbwsv_tiff(frame.path.toStdWString());
        QImage image(
            QSize(static_cast<int>(result.spec.width), static_cast<int>(result.spec.height)),
            QImage::Format_ARGB32);
        image.fill(m_package.pseudocolors.value("empty", QColor(255, 255, 255)));
        for (std::uint32_t y = 0; y < result.spec.height; ++y)
        {
            for (std::uint32_t x = 0; x < result.spec.width; ++x)
            {
                const std::size_t index =
                    (static_cast<std::size_t>(y) * result.spec.width + x) * result.spec.samples_per_pixel;
                image.setPixelColor(
                    static_cast<int>(x),
                    static_cast<int>(y),
                    QColor(
                        result.pixels.at(index),
                        result.pixels.at(index + 1),
                        result.pixels.at(index + 2)));
            }
        }
        return ToDisplayCoordinateImage(image);
    }
    catch (...)
    {
        return BlankCanvas();
    }
}

QImage LayerPreviewPanel::RenderMaskChannel(const int layerIndex, const QString& channel) const
{
    const QImage source = ReadFrameImage(FindFrame(layerIndex, channel));
    if (source.isNull())
    {
        return BlankCanvas();
    }
    return ApplyPseudoColor(source, m_package.pseudocolors.value(channel, QColor(0, 0, 0)));
}

QImage LayerPreviewPanel::RenderOccupancy(const int layerIndex) const
{
    QImage result = BlankCanvas();
    if (result.isNull())
    {
        return {};
    }

    const QStringList sourceChannels{"texture_rgb", "rgb", "white", "support", "varnish"};
    const QColor occupancyColor = m_package.pseudocolors.value("occupancy", QColor(80, 80, 80));
    for (const QString& channel : sourceChannels)
    {
        const QImage source = ReadFrameImage(FindFrame(layerIndex, channel)).convertToFormat(QImage::Format_ARGB32);
        if (source.isNull())
        {
            continue;
        }
        const QImage scaled = source.size() == result.size() ? source : source.scaled(result.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
        for (int y = 0; y < scaled.height(); ++y)
        {
            for (int x = 0; x < scaled.width(); ++x)
            {
                if (IsPrintedPixel(QColor::fromRgba(scaled.pixel(x, y))))
                {
                    result.setPixelColor(x, y, occupancyColor);
                }
            }
        }
    }
    return result;
}

QImage LayerPreviewPanel::RenderDiagnostic(const int layerIndex) const
{
    QImage result = RenderOccupancy(layerIndex).convertToFormat(QImage::Format_ARGB32);
    if (result.isNull())
    {
        return {};
    }

    const LayerPreviewLayerStats stats = m_package.layerstats.value(layerIndex);
    const bool hasWarning = !stats.fillwarnings.isEmpty() || stats.smallcomponentcount > 0 || stats.tinycomponentcount > 0;
    if (!hasWarning)
    {
        return result;
    }

    QPainter painter(&result);
    painter.setPen(QPen(m_package.pseudocolors.value("diagnostic", QColor(255, 180, 0)), 2));
    painter.drawRect(result.rect().adjusted(1, 1, -2, -2));
    return result;
}

QImage LayerPreviewPanel::BlankCanvas() const
{
    if (m_package.widthpx <= 0 || m_package.heightpx <= 0)
    {
        return {};
    }
    QImage image(QSize(m_package.widthpx, m_package.heightpx), QImage::Format_ARGB32);
    image.fill(m_package.pseudocolors.value("empty", QColor(255, 255, 255)));
    return image;
}

QImage LayerPreviewPanel::ApplyPseudoColor(const QImage& source, const QColor& color) const
{
    const QImage input = source.convertToFormat(QImage::Format_ARGB32);
    QImage output(input.size(), QImage::Format_ARGB32);
    output.fill(m_package.pseudocolors.value("empty", QColor(255, 255, 255)));

    for (int y = 0; y < input.height(); ++y)
    {
        for (int x = 0; x < input.width(); ++x)
        {
            if (IsPrintedPixel(QColor::fromRgba(input.pixel(x, y))))
            {
                output.setPixelColor(x, y, color);
            }
        }
    }
    return output;
}

void LayerPreviewPanel::RebuildChannelSelector()
{
    m_channelSelector->blockSignals(true);
    m_channelSelector->clear();
    for (const QString& channel : m_package.channels)
    {
        m_channelSelector->addItem(LayerPreviewDataProvider::DisplayName(channel), channel);
    }
    m_channelSelector->setEnabled(m_channelSelector->count() > 0);
    m_channelSelector->blockSignals(false);
}

void LayerPreviewPanel::RebuildLayerSlider()
{
    m_layerSlider->blockSignals(true);
    m_layerSlider->setMinimum(0);
    m_layerSlider->setMaximum(m_package.layerindices.isEmpty() ? 0 : m_package.layerindices.size() - 1);
    m_layerSlider->setValue(0);
    m_layerSlider->setEnabled(!m_package.layerindices.isEmpty());
    m_layerSlider->blockSignals(false);
}

void LayerPreviewPanel::UpdateImage()
{
    m_currentImage = RenderCurrentImage();
    if (m_currentImage.isNull())
    {
        m_imageLabel->clear();
        UpdateStatus("未找到可显示的层预览图。");
        return;
    }

    ApplyPixmap();
    UpdateStatus();
}

void LayerPreviewPanel::ApplyPixmap()
{
    if (m_currentImage.isNull())
    {
        return;
    }

    const QSize physicalSize = PhysicalDisplaySize();
    QSize targetSize = physicalSize;
    if (m_fit)
    {
        targetSize = physicalSize.scaled(
            m_scrollArea->viewport()->size(),
            Qt::KeepAspectRatio);
    }
    else
    {
        targetSize = QSize(
            static_cast<int>(physicalSize.width() * m_zoom),
            static_cast<int>(physicalSize.height() * m_zoom));
    }

    if (targetSize.width() <= 0 || targetSize.height() <= 0)
    {
        targetSize = physicalSize;
    }

    m_imageLabel->setPixmap(
        QPixmap::fromImage(m_currentImage)
            .scaled(
                targetSize,
                Qt::IgnoreAspectRatio,
                Qt::FastTransformation));
    m_imageLabel->resize(targetSize);
}

void LayerPreviewPanel::UpdateStatus(const QString& note)
{
    if (!note.isEmpty())
    {
        m_status->setText(note);
        return;
    }

    const int layerIndex = CurrentLayerIndex();
    const LayerPreviewLayerStats stats = m_package.layerstats.value(layerIndex);
    const LayerPreviewFrame frame = FindFrame(layerIndex, CurrentChannel());
    QString text = QString("第 %1/%2 层  layer=%3  z=%4 mm  通道=%5  RGB=%6  W=%7  S=%8  V=%9  层序=低Z->高Z  显示=切片坐标")
                       .arg(qMax(1, m_layerSlider->value() + 1))
                       .arg(qMax(1, m_package.layerindices.size()))
                       .arg(layerIndex)
                       .arg(stats.zmm, 0, 'f', 3)
                       .arg(LayerPreviewDataProvider::DisplayName(CurrentChannel()))
                       .arg(stats.rgbprintpixels)
                       .arg(stats.whiteprintpixels)
                       .arg(stats.supportprintpixels)
                       .arg(stats.varnishprintpixels);

    const QString semanticText = BuildLayerSemanticText(stats);
    if (!semanticText.isEmpty())
    {
        text += "  " + semanticText;
    }
    const QString sourcePolicyText = BuildSourcePolicyText();
    if (!sourcePolicyText.isEmpty())
    {
        text += "  " + sourcePolicyText;
    }

    if (!frame.path.isEmpty())
    {
        text += "  " + QFileInfo(frame.path).fileName();
    }
    if (!m_provider.ErrorString().isEmpty())
    {
        text += "  " + m_provider.ErrorString();
    }
    if (!m_probeText.isEmpty())
    {
        text += "  " + m_probeText;
    }
    text += "  "
        + PreviewPhysicalScaleResolver::Summary(m_package.physicalscale);
    m_status->setText(text);
}

QSize LayerPreviewPanel::PhysicalDisplaySize() const
{
    return PreviewPhysicalScaleResolver::DisplaySize(
        m_currentImage.size(),
        m_package.physicalscale);
}

bool LayerPreviewPanel::IsPrintedPixel(const QColor& color) const
{
    if (color.alpha() == 0)
    {
        return false;
    }
    return !(color.red() > 245 && color.green() > 245 && color.blue() > 245);
}

QString LayerPreviewPanel::BuildPixelProbeText(const int displayX, const int displayY) const
{
    const int layerIndex = CurrentLayerIndex();
    const LayerPreviewFrame frame = FindFrame(layerIndex, "production_rgb");
    if (layerIndex < 0 || frame.path.isEmpty() || !QFileInfo::exists(frame.path))
    {
        return {};
    }

    try
    {
        const slicer_core::TiffReadResult result = slicer_core::read_rgbwsv_tiff(frame.path.toStdWString());
        if (displayX < 0 || displayY < 0 || displayX >= static_cast<int>(result.spec.width)
            || displayY >= static_cast<int>(result.spec.height))
        {
            return {};
        }

        const int rawY = static_cast<int>(result.spec.height) - 1 - displayY;
        const std::size_t index =
            (static_cast<std::size_t>(rawY) * result.spec.width + static_cast<std::uint32_t>(displayX))
                * result.spec.samples_per_pixel;
        const int r = static_cast<int>(result.pixels.at(index));
        const int g = static_cast<int>(result.pixels.at(index + 1));
        const int b = static_cast<int>(result.pixels.at(index + 2));
        const int w = static_cast<int>(result.pixels.at(index + 3));
        const int s = static_cast<int>(result.pixels.at(index + 4));
        const int v = static_cast<int>(result.pixels.at(index + 5));
        return QString("像素探针 x=%1 y=%2 layer=%3 生产值 RGBWSV=(%4,%5,%6,%7,%8,%9) %10 协议=black_is_print/0打印/255不打印 显示颜色=伪彩或真彩预览")
            .arg(displayX)
            .arg(displayY)
            .arg(layerIndex)
            .arg(r)
            .arg(g)
            .arg(b)
            .arg(w)
            .arg(s)
            .arg(v)
            .arg(InterpretPixel(r, g, b, w, s, v));
    }
    catch (...)
    {
        return {};
    }
}

QString LayerPreviewPanel::InterpretPixel(const int r, const int g, const int b, const int w, const int s, const int v) const
{
    const bool hasRgb = r < 255 || g < 255 || b < 255;
    const bool hasWhite = w < 255;
    const bool hasSupport = s < 255;
    const bool hasVarnish = v < 255;
    const LayerPreviewLayerStats stats = m_package.layerstats.value(CurrentLayerIndex());
    const LayerPreviewSemanticPolicy policy = m_package.semanticpolicy;

    QStringList semantics;
    QStringList sourcePolicies;
    QStringList printedChannels;
    if (hasRgb)
    {
        printedChannels.push_back("RGB");
        semantics.push_back("RGB模型颜色或填充");
        sourcePolicies.push_back("textureSurfacePixels=" + QString::number(stats.texturesurfacepixels));
    }
    if (hasWhite)
    {
        printedChannels.push_back("W");
        const QString material = policy.modelfillmaterial.isEmpty() ? "white" : policy.modelfillmaterial;
        semantics.push_back(material == "white" ? "白墨模型填充" : "W白墨通道打印");
        sourcePolicies.push_back("modelFill=" + material + "/" + policy.modelfillscope);
    }
    if (hasSupport)
    {
        printedChannels.push_back("S");
        semantics.push_back("支撑填充");
        QString supportPolicy = "supportPlacement=" + (policy.supportplacement.isEmpty() ? QString("unknown") : policy.supportplacement);
        if (!stats.supporttypesummary.isEmpty())
        {
            supportPolicy += "; layerSupportTypes=" + stats.supporttypesummary;
        }
        sourcePolicies.push_back(supportPolicy);
    }
    if (hasVarnish)
    {
        printedChannels.push_back("V");
        if (!hasRgb && !hasWhite && policy.modelfillmaterial == "varnish")
        {
            semantics.push_back("光油模型填充");
        }
        else
        {
            semantics.push_back("光油表面或外侧层");
        }
        QString varnishPolicy;
        if (policy.outervarnishenabled)
        {
            varnishPolicy += QString("outerVarnish=%1px").arg(policy.outervarnishthicknesspx);
        }
        if (policy.surfacevarnishenabled)
        {
            if (!varnishPolicy.isEmpty())
            {
                varnishPolicy += "; ";
            }
            varnishPolicy += QString("surfaceVarnish=%1px").arg(policy.surfacevarnishthicknesspx);
        }
        sourcePolicies.push_back(varnishPolicy.isEmpty() ? QString("varnishChannel=V") : varnishPolicy);
    }
    if (semantics.isEmpty())
    {
        return "打印通道=无 材料语义=真实空白 semantic=Empty sourcePolicy=emptyValue=255";
    }

    printedChannels.removeDuplicates();
    semantics.removeDuplicates();
    sourcePolicies.removeDuplicates();
    return "打印通道=" + printedChannels.join("+")
        + " 材料语义=" + semantics.join("+")
        + " semantic=" + semantics.join("+")
        + " sourcePolicy=" + sourcePolicies.join(" | ");
}

QString LayerPreviewPanel::ApplyPixelProbe(const int displayX, const int displayY)
{
    m_probeText = BuildPixelProbeText(displayX, displayY);
    if (!m_probeText.isEmpty())
    {
        UpdateStatus(m_probeText);
    }
    emit SigPixelProbeChanged(m_probeText);
    return m_probeText;
}

void LayerPreviewPanel::ClearPixelProbe()
{
    if (m_probeText.isEmpty())
    {
        return;
    }
    m_probeText.clear();
    emit SigPixelProbeChanged(QString{});
}

QString LayerPreviewPanel::BuildLayerSemanticText(const LayerPreviewLayerStats& stats) const
{
    QStringList parts;
    parts.push_back("TextureSurface=" + QString::number(stats.texturesurfacepixels));
    parts.push_back("ModelFill=" + QString::number(stats.modelfillpixels));
    parts.push_back("Support=" + QString::number(stats.supportprintpixels));
    if (stats.internalvoidsupportpixels > 0)
    {
        parts.push_back("InternalVoidS=" + QString::number(stats.internalvoidsupportpixels));
    }
    if (stats.uppersurfacesupportpixels > 0)
    {
        parts.push_back("UpperS=" + QString::number(stats.uppersurfacesupportpixels));
    }
    if (stats.outervarnishpixels > 0)
    {
        parts.push_back("OuterV=" + QString::number(stats.outervarnishpixels));
    }
    if (stats.outersurfacevarnishpixels > 0 || stats.innersurfacevarnishpixels > 0)
    {
        parts.push_back(
            QString("SurfaceV(out/in)=%1/%2").arg(stats.outersurfacevarnishpixels).arg(stats.innersurfacevarnishpixels));
    }
    return "semantic: " + parts.join(", ");
}

QString LayerPreviewPanel::BuildSourcePolicyText() const
{
    const LayerPreviewSemanticPolicy policy = m_package.semanticpolicy;
    QStringList parts;
    if (!policy.modelfillmaterial.isEmpty())
    {
        parts.push_back("modelFill=" + policy.modelfillmaterial + "/" + policy.modelfillscope);
    }
    if (!policy.supportplacement.isEmpty())
    {
        QString support = "support=" + policy.supportplacement;
        if (!policy.supportsource.isEmpty())
        {
            support += "(" + policy.supportsource + ")";
        }
        parts.push_back(support);
    }
    parts.push_back(QString("internalVoid=%1").arg(policy.internalvoidsupportenabled ? "on" : "off"));
    if (policy.outervarnishenabled)
    {
        parts.push_back(QString("outerVarnish=%1px/%2mm").arg(policy.outervarnishthicknesspx).arg(policy.outervarnishthicknessmm, 0, 'f', 3));
    }
    if (policy.surfacevarnishenabled)
    {
        parts.push_back(QString("surfaceVarnish=%1px").arg(policy.surfacevarnishthicknesspx));
    }
    if (!policy.semanticpriority.isEmpty())
    {
        parts.push_back("priority=" + policy.semanticpriority);
    }
    return parts.isEmpty() ? QString() : "sourcePolicy: " + parts.join(", ");
}
