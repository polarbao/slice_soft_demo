#include "LayerPreviewPanel.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
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
    m_layerSlider = new QSlider(Qt::Horizontal, this);
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

bool LayerPreviewPanel::SelectLayerForTest(const int layerIndex)
{
    const int position = m_package.layerindices.indexOf(layerIndex);
    if (position < 0)
    {
        return false;
    }
    m_layerSlider->setValue(position);
    return true;
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

void LayerPreviewPanel::OnLayerChanged(const int value)
{
    Q_UNUSED(value);
    UpdateImage();
}

void LayerPreviewPanel::OnChannelChanged(const int index)
{
    Q_UNUSED(index);
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

    QSize targetSize = m_currentImage.size();
    if (m_fit)
    {
        targetSize = m_currentImage.size().scaled(m_scrollArea->viewport()->size(), Qt::KeepAspectRatio);
    }
    else
    {
        targetSize = QSize(static_cast<int>(m_currentImage.width() * m_zoom), static_cast<int>(m_currentImage.height() * m_zoom));
    }

    if (targetSize.width() <= 0 || targetSize.height() <= 0)
    {
        targetSize = m_currentImage.size();
    }

    m_imageLabel->setPixmap(QPixmap::fromImage(m_currentImage).scaled(targetSize, Qt::KeepAspectRatio, Qt::FastTransformation));
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

    if (!frame.path.isEmpty())
    {
        text += "  " + QFileInfo(frame.path).fileName();
    }
    if (!m_provider.ErrorString().isEmpty())
    {
        text += "  " + m_provider.ErrorString();
    }
    m_status->setText(text);
}

bool LayerPreviewPanel::IsPrintedPixel(const QColor& color) const
{
    if (color.alpha() == 0)
    {
        return false;
    }
    return !(color.red() > 245 && color.green() > 245 && color.blue() > 245);
}
