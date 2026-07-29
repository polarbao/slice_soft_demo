#include "LayerPreviewPanel.h"

#include "../services/MaterialPreviewImageAdapter.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>

namespace
{

constexpr auto kProductionConsumerId = "production-preview";

QPushButton* MakeButton(
    const QString& text,
    QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setMinimumHeight(28);
    return button;
}

QString NormalizeModeId(const QString& modeId)
{
    if (modeId == QStringLiteral("production_rgb")
        || modeId == QStringLiteral("texture_rgb"))
    {
        return QStringLiteral("rgb");
    }
    if (modeId == QStringLiteral("diagnostic"))
    {
        return QStringLiteral("occupancy");
    }
    return modeId;
}

}  // namespace

LayerPreviewPanel::LayerPreviewPanel(QWidget* parent)
    : QWidget(parent),
      m_layerSource(
          std::make_shared<slicer_core::TiffLayerSource>()),
      m_layerWorker(
          new TiffLayerLoadWorker(m_layerSource, this))
{
    auto* layout = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();

    m_channelSelector = new QComboBox(this);
    m_channelSelector->setObjectName(
        QStringLiteral("productionMaterialModeSelector"));
    m_channelSelector->setToolTip(
        QStringLiteral(
            "从当前层 RGBWSV TIFF 缓冲切换生产材料视图；切换模式不会重新读取 TIFF。"));
    m_layerSlider = new QSlider(Qt::Horizontal, this);
    m_layerSlider->setObjectName(
        QStringLiteral("productionLayerSlider"));
    m_layerSlider->setToolTip(
        QStringLiteral(
            "按 manifest 真实 layerIndex 从低 Z 到高 Z 异步读取生产 TIFF。"));
    auto* fitButton = MakeButton(
        QStringLiteral("适应"),
        this);
    auto* actualButton = MakeButton(
        QStringLiteral("1:1"),
        this);
    auto* zoomInButton = MakeButton(
        QStringLiteral("+"),
        this);
    auto* zoomOutButton = MakeButton(
        QStringLiteral("-"),
        this);

    controls->addWidget(
        new QLabel(QStringLiteral("材料视图"), this));
    controls->addWidget(m_channelSelector);
    controls->addWidget(
        new QLabel(QStringLiteral("层"), this));
    controls->addWidget(m_layerSlider, 1);
    controls->addWidget(fitButton);
    controls->addWidget(actualButton);
    controls->addWidget(zoomInButton);
    controls->addWidget(zoomOutButton);
    layout->addLayout(controls);

    m_status = new QLabel(
        QStringLiteral("尚未加载生产 TIFF 输出包。"),
        this);
    m_status->setObjectName(
        QStringLiteral("productionPreviewStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setSizePolicy(
        QSizePolicy::Ignored,
        QSizePolicy::Ignored);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->setWidgetResizable(true);
    layout->addWidget(m_scrollArea, 1);
    m_imageLabel->installEventFilter(this);

    connect(
        m_layerSlider,
        &QSlider::valueChanged,
        this,
        &LayerPreviewPanel::OnLayerChanged);
    connect(
        m_channelSelector,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &LayerPreviewPanel::OnChannelChanged);
    connect(
        zoomInButton,
        &QPushButton::clicked,
        this,
        &LayerPreviewPanel::OnZoomIn);
    connect(
        zoomOutButton,
        &QPushButton::clicked,
        this,
        &LayerPreviewPanel::OnZoomOut);
    connect(
        fitButton,
        &QPushButton::clicked,
        this,
        &LayerPreviewPanel::OnFitToWindow);
    connect(
        actualButton,
        &QPushButton::clicked,
        this,
        &LayerPreviewPanel::OnActualSize);
    connect(
        m_layerWorker,
        &TiffLayerLoadWorker::SigLayerLoaded,
        this,
        &LayerPreviewPanel::OnLayerLoaded);
    connect(
        m_layerWorker,
        &TiffLayerLoadWorker::SigLayerLoadFailed,
        this,
        &LayerPreviewPanel::OnLayerLoadFailed);

    RebuildChannelSelector();
    RebuildLayerSlider();
}

LayerPreviewPanel::~LayerPreviewPanel()
{
    m_layerWorker->Cancel();
}

void LayerPreviewPanel::LoadPackage(
    const PackageSummary& package)
{
    m_layerWorker->Cancel();
    ClearCurrentLayer();
    m_package =
        m_provider.LoadProductionMetadata(package);
    m_package.layerindices.clear();
    m_package.layercount = 0;
    m_zoom = 1.0;
    m_fit = true;
    m_errorCode.clear();
    m_errorMessage.clear();
    ClearPixelProbe();

    const bool indexed =
        !package.manifest_path.isEmpty()
        && m_layerWorker->IndexPackage(package.manifest_path);
    const auto index =
        m_layerWorker->PackageIndexSnapshot();
    if (indexed && index.has_value())
    {
        ApplyPackageIndex(*index);
    }
    else if (m_errorCode.isEmpty())
    {
        m_errorCode =
            QStringLiteral("TIFF_LAYER_MANIFEST_INVALID");
        m_errorMessage =
            QStringLiteral(
                "无法建立 manifest 权威生产层索引。");
    }

    RebuildChannelSelector();
    RebuildLayerSlider();
    if (!m_package.layerindices.isEmpty())
    {
        RequestCurrentLayer();
    }
    else
    {
        UpdateStatus();
    }
}

int LayerPreviewPanel::LayerCount() const
{
    return m_package.layerindices.size();
}

QStringList LayerPreviewPanel::AvailableChannels() const
{
    return MaterialPreviewImageAdapter::ModeIds();
}

QVector<int> LayerPreviewPanel::LayerIndices() const
{
    return m_package.layerindices;
}

int LayerPreviewPanel::CurrentLayerIndex() const
{
    if (m_package.layerindices.isEmpty())
    {
        return -1;
    }
    const int position = qBound(
        0,
        m_layerSlider->value(),
        m_package.layerindices.size() - 1);
    return m_package.layerindices.at(position);
}

bool LayerPreviewPanel::SelectLayer(
    const int layerIndex)
{
    const int position =
        m_package.layerindices.indexOf(layerIndex);
    if (position < 0)
    {
        return false;
    }
    if (m_layerSlider->value() == position)
    {
        if (!IsLayerReadyForTest())
        {
            RequestCurrentLayer();
        }
        else
        {
            ComposeCurrentImage();
        }
        return true;
    }
    m_layerSlider->setValue(position);
    return true;
}

bool LayerPreviewPanel::SelectLayerForTest(
    const int layerIndex)
{
    return SelectLayer(layerIndex);
}

bool LayerPreviewPanel::SelectChannelForTest(
    const QString& channel)
{
    const QString normalized = NormalizeModeId(channel);
    for (int index = 0;
         index < m_channelSelector->count();
         ++index)
    {
        if (m_channelSelector->itemData(index).toString()
            == normalized)
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

QString LayerPreviewPanel::PixelProbeForTest(
    const int x,
    const int y) const
{
    return BuildPixelProbeText(x, y);
}

QString LayerPreviewPanel::ProbePixelForTest(
    const int x,
    const int y)
{
    return ApplyPixelProbe(x, y);
}

QColor LayerPreviewPanel::PseudoColor(
    const QString& channel) const
{
    return m_package.pseudocolors.value(
        channel,
        QColor(255, 255, 255));
}

QSize LayerPreviewPanel::PhysicalDisplaySizeForTest() const
{
    return PhysicalDisplaySize();
}

QString LayerPreviewPanel::StatusForTest() const
{
    return m_status == nullptr
        ? QString{}
        : m_status->text();
}

bool LayerPreviewPanel::IsLayerReadyForTest() const
{
    return m_currentBuffer != nullptr
        && m_currentBuffer->layerIndex
            == CurrentLayerIndex()
        && !m_currentImage.isNull()
        && !m_loading;
}

int LayerPreviewPanel::LoadedLayerIndexForTest() const
{
    return m_currentBuffer == nullptr
        ? -1
        : m_currentBuffer->layerIndex;
}

quint64 LayerPreviewPanel::LayerRequestCountForTest() const
{
    return m_layerRequestCount;
}

QString LayerPreviewPanel::DataSourceForTest() const
{
    return QStringLiteral(
        "manifest/layers RGBWSV TIFF");
}

bool LayerPreviewPanel::LastLoadWasCacheHitForTest() const
{
    return m_lastCacheHit;
}

bool LayerPreviewPanel::eventFilter(
    QObject* object,
    QEvent* event)
{
    if (object == m_imageLabel
        && event->type()
            == QEvent::MouseButtonPress
        && IsLayerReadyForTest())
    {
        const auto* mouseEvent =
            static_cast<QMouseEvent*>(event);
        if (m_imageLabel->width() > 0
            && m_imageLabel->height() > 0)
        {
            const int displayX = qBound(
                0,
                mouseEvent->pos().x()
                    * m_currentImage.width()
                    / m_imageLabel->width(),
                m_currentImage.width() - 1);
            const int displayY = qBound(
                0,
                mouseEvent->pos().y()
                    * m_currentImage.height()
                    / m_imageLabel->height(),
                m_currentImage.height() - 1);
            ApplyPixelProbe(displayX, displayY);
        }
    }
    return QWidget::eventFilter(object, event);
}

void LayerPreviewPanel::OnLayerChanged(
    const int value)
{
    Q_UNUSED(value);
    ClearPixelProbe();
    RequestCurrentLayer();
    const int layerIndex = CurrentLayerIndex();
    if (layerIndex >= 0)
    {
        emit SigLayerIndexChanged(layerIndex);
    }
}

void LayerPreviewPanel::OnChannelChanged(
    const int index)
{
    Q_UNUSED(index);
    ClearPixelProbe();
    ComposeCurrentImage();
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

void LayerPreviewPanel::OnLayerLoaded(
    const quint64 generation,
    const QString& consumerId,
    const int layerIndex,
    TiffLayerBufferPtr buffer,
    const bool cacheHit)
{
    if (consumerId
            != QString::fromLatin1(kProductionConsumerId)
        || generation != m_expectedGeneration
        || layerIndex != CurrentLayerIndex()
        || buffer == nullptr
        || buffer->layerIndex != layerIndex)
    {
        return;
    }

    m_currentBuffer = std::move(buffer);
    m_loading = false;
    m_requestedLayerIndex = -1;
    m_lastCacheHit = cacheHit;
    m_errorCode.clear();
    m_errorMessage.clear();
    ComposeCurrentImage();
    emit SigLayerBufferReady(m_currentBuffer);
}

void LayerPreviewPanel::OnLayerLoadFailed(
    const quint64 generation,
    const QString& consumerId,
    const int layerIndex,
    const QString& errorCode,
    const QString& message)
{
    const bool packageFailure =
        consumerId == QStringLiteral("package-index");
    if (!packageFailure
        && (consumerId
                != QString::fromLatin1(
                    kProductionConsumerId)
            || generation != m_expectedGeneration
            || layerIndex != CurrentLayerIndex()))
    {
        return;
    }

    ClearCurrentLayer();
    m_errorCode = errorCode;
    m_errorMessage = message;
    UpdateStatus();
}

QString LayerPreviewPanel::CurrentChannel() const
{
    return m_channelSelector->currentData().toString();
}

double LayerPreviewPanel::CurrentLayerZMm() const
{
    if (m_currentBuffer != nullptr
        && m_currentBuffer->layerIndex
            == CurrentLayerIndex())
    {
        return m_currentBuffer->zMm;
    }
    return m_package.layerstats
        .value(CurrentLayerIndex())
        .zmm;
}

void LayerPreviewPanel::ApplyPackageIndex(
    const slicer_core::ProductionPackageIndex& index)
{
    m_package.layerindices.clear();
    for (const auto& layer : index.layers)
    {
        m_package.layerindices.push_back(
            layer.layerIndex);
        LayerPreviewLayerStats stats =
            m_package.layerstats.value(
                layer.layerIndex);
        stats.layerindex = layer.layerIndex;
        stats.zmm = layer.zMm;
        m_package.layerstats.insert(
            layer.layerIndex,
            stats);
    }
    std::sort(
        m_package.layerindices.begin(),
        m_package.layerindices.end());
    m_package.layercount =
        m_package.layerindices.size();
    m_package.widthpx =
        static_cast<int>(index.width);
    m_package.heightpx =
        static_cast<int>(index.height);

    QJsonObject grid;
    grid.insert(
        QStringLiteral("dpiX"),
        index.dpiX);
    grid.insert(
        QStringLiteral("dpiY"),
        index.dpiY);
    m_package.physicalscale =
        PreviewPhysicalScaleResolver::Resolve(grid);
}

void LayerPreviewPanel::RebuildChannelSelector()
{
    m_channelSelector->blockSignals(true);
    m_channelSelector->clear();
    for (const QString& modeId
         : MaterialPreviewImageAdapter::ModeIds())
    {
        m_channelSelector->addItem(
            MaterialPreviewImageAdapter::DisplayName(
                modeId),
            modeId);
    }
    const int allMaterials =
        m_channelSelector->findData(
            QStringLiteral(
                "rgb_support_white_varnish"));
    m_channelSelector->setCurrentIndex(
        allMaterials >= 0 ? allMaterials : 0);
    m_channelSelector->setEnabled(
        m_channelSelector->count() > 0);
    m_channelSelector->blockSignals(false);
}

void LayerPreviewPanel::RebuildLayerSlider()
{
    m_layerSlider->blockSignals(true);
    m_layerSlider->setMinimum(0);
    m_layerSlider->setMaximum(
        m_package.layerindices.isEmpty()
            ? 0
            : m_package.layerindices.size() - 1);
    m_layerSlider->setValue(0);
    m_layerSlider->setEnabled(
        !m_package.layerindices.isEmpty());
    m_layerSlider->blockSignals(false);
}

void LayerPreviewPanel::RequestCurrentLayer()
{
    const int layerIndex = CurrentLayerIndex();
    if (layerIndex < 0)
    {
        ClearCurrentLayer();
        UpdateStatus();
        return;
    }
    if (m_loading
        && m_requestedLayerIndex == layerIndex)
    {
        return;
    }
    if (m_currentBuffer != nullptr
        && m_currentBuffer->layerIndex
            == layerIndex)
    {
        ComposeCurrentImage();
        return;
    }

    ClearCurrentLayer();
    m_loading = true;
    m_requestedLayerIndex = layerIndex;
    ++m_layerRequestCount;
    m_expectedGeneration =
        m_layerWorker->RequestLayer(
            layerIndex,
            QString::fromLatin1(
                kProductionConsumerId));
    UpdateStatus();
}

void LayerPreviewPanel::ComposeCurrentImage()
{
    if (m_currentBuffer == nullptr
        || m_currentBuffer->layerIndex
            != CurrentLayerIndex())
    {
        UpdateStatus();
        return;
    }

    const auto mode =
        MaterialPreviewImageAdapter::ModeFromId(
            CurrentChannel());
    if (!mode.has_value())
    {
        m_currentImage = {};
        m_errorCode =
            QStringLiteral(
                "MATERIAL_PREVIEW_MODE_INVALID");
        m_errorMessage =
            QStringLiteral("未知生产材料预览模式。");
        m_imageLabel->clear();
        UpdateStatus();
        return;
    }

    try
    {
        slicer_core::MaterialPreviewRequest request;
        request.mode = *mode;
        request.palette =
            MaterialPreviewImageAdapter::BuildPalette(
                m_package.pseudocolors);
        m_currentPreview =
            slicer_core::MaterialPreviewComposer::Compose(
                *m_currentBuffer,
                request);
        m_currentImage =
            MaterialPreviewImageAdapter::ToDisplayImage(
                m_currentPreview);
        if (m_currentImage.isNull())
        {
            throw slicer_core::MaterialPreviewError(
                slicer_core::MaterialPreviewErrorCode::
                    BufferInvalid,
                "Qt display image conversion failed");
        }
        m_errorCode.clear();
        m_errorMessage.clear();
        ApplyPixmap();
        UpdateStatus();
    }
    catch (
        const slicer_core::MaterialPreviewError& error)
    {
        m_currentImage = {};
        m_imageLabel->clear();
        m_errorCode = QString::fromStdString(
            slicer_core::MaterialPreviewErrorCodeString(
                error.Code()));
        m_errorMessage =
            QString::fromUtf8(error.what());
        UpdateStatus();
    }
    catch (const std::exception& error)
    {
        m_currentImage = {};
        m_imageLabel->clear();
        m_errorCode =
            QStringLiteral(
                "MATERIAL_PREVIEW_BUFFER_INVALID");
        m_errorMessage =
            QString::fromUtf8(error.what());
        UpdateStatus();
    }
}

void LayerPreviewPanel::ApplyPixmap()
{
    if (m_currentImage.isNull())
    {
        m_imageLabel->clear();
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
            static_cast<int>(
                physicalSize.width() * m_zoom),
            static_cast<int>(
                physicalSize.height() * m_zoom));
    }
    if (targetSize.width() <= 0
        || targetSize.height() <= 0)
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

void LayerPreviewPanel::UpdateStatus(
    const QString& note)
{
    const int layerIndex = CurrentLayerIndex();
    const int position = layerIndex < 0
        ? 0
        : m_package.layerindices.indexOf(
              layerIndex);
    QString text = QStringLiteral(
        "第 %1/%2 层  layer=%3  z=%4 mm  模式=%5  "
        "层序=低Z->高Z  数据源=manifest/layers TIFF")
        .arg(position >= 0 ? position + 1 : 0)
        .arg(m_package.layerindices.size())
        .arg(layerIndex)
        .arg(CurrentLayerZMm(), 0, 'f', 3)
        .arg(
            MaterialPreviewImageAdapter::DisplayName(
                CurrentChannel()));

    if (m_loading)
    {
        text += QStringLiteral(
            "  状态=加载中 generation=%1")
            .arg(m_expectedGeneration);
    }
    else if (!m_errorCode.isEmpty())
    {
        text += QStringLiteral(
            "  状态=失败 code=%1 message=%2")
            .arg(m_errorCode, m_errorMessage);
    }
    else if (m_currentBuffer != nullptr)
    {
        const auto& stats =
            m_currentPreview.stats;
        text += QStringLiteral(
            "  cache=%1  RGB=%2  W=%3  S=%4  V=%5  "
            "occupied=%6  empty=%7  multiMaterial=%8")
            .arg(
                m_lastCacheHit
                    ? QStringLiteral("hit")
                    : QStringLiteral("miss"))
            .arg(stats.rgbPixels)
            .arg(stats.whitePixels)
            .arg(stats.supportPixels)
            .arg(stats.varnishPixels)
            .arg(stats.occupiedPixels)
            .arg(stats.emptyPixels)
            .arg(stats.multiMaterialPixels);
    }

    const LayerPreviewLayerStats layerStats =
        m_package.layerstats.value(layerIndex);
    const QString semanticText =
        BuildLayerSemanticText(layerStats);
    if (!semanticText.isEmpty())
    {
        text += QStringLiteral("  ")
            + semanticText;
    }
    const QString sourcePolicyText =
        BuildSourcePolicyText();
    if (!sourcePolicyText.isEmpty())
    {
        text += QStringLiteral("  ")
            + sourcePolicyText;
    }
    if (!note.isEmpty())
    {
        text += QStringLiteral("  ") + note;
    }
    if (!m_probeText.isEmpty()
        && note != m_probeText)
    {
        text += QStringLiteral("  ")
            + m_probeText;
    }
    text += QStringLiteral("  ")
        + PreviewPhysicalScaleResolver::Summary(
            m_package.physicalscale);
    m_status->setText(text);
}

QString LayerPreviewPanel::BuildPixelProbeText(
    const int displayX,
    const int displayY) const
{
    if (!IsLayerReadyForTest()
        || displayX < 0
        || displayY < 0
        || displayX
            >= static_cast<int>(
                m_currentBuffer->width)
        || displayY
            >= static_cast<int>(
                m_currentBuffer->height))
    {
        return {};
    }

    const int rawY =
        static_cast<int>(m_currentBuffer->height)
        - 1 - displayY;
    try
    {
        const slicer_core::MaterialPixelProbe probe =
            slicer_core::MaterialPreviewComposer::Probe(
                *m_currentBuffer,
                static_cast<std::uint32_t>(displayX),
                static_cast<std::uint32_t>(rawY));
        return QStringLiteral(
            "像素探针 display=(%1,%2) raw=(%3,%4) "
            "layer=%5 生产值 RGBWSV=(%6,%7,%8,%9,%10,%11) "
            "%12 协议=black_is_print/0打印/255不打印 "
            "显示颜色=伪彩或真彩预览")
            .arg(displayX)
            .arg(displayY)
            .arg(probe.x)
            .arg(probe.y)
            .arg(CurrentLayerIndex())
            .arg(probe.values.at(0))
            .arg(probe.values.at(1))
            .arg(probe.values.at(2))
            .arg(probe.values.at(3))
            .arg(probe.values.at(4))
            .arg(probe.values.at(5))
            .arg(InterpretPixel(probe));
    }
    catch (...)
    {
        return {};
    }
}

QString LayerPreviewPanel::InterpretPixel(
    const slicer_core::MaterialPixelProbe& probe) const
{
    const LayerPreviewLayerStats stats =
        m_package.layerstats.value(
            CurrentLayerIndex());
    const LayerPreviewSemanticPolicy policy =
        m_package.semanticpolicy;

    QStringList semantics;
    QStringList sourcePolicies;
    QStringList printedChannels;
    if (probe.hasRgb)
    {
        printedChannels.push_back(
            QStringLiteral("RGB"));
        semantics.push_back(
            QStringLiteral(
                "RGB模型颜色或填充"));
        sourcePolicies.push_back(
            QStringLiteral(
                "textureSurfacePixels=%1")
                .arg(stats.texturesurfacepixels));
    }
    if (probe.hasWhite)
    {
        printedChannels.push_back(
            QStringLiteral("W"));
        const QString material =
            policy.modelfillmaterial.isEmpty()
            ? QStringLiteral("white")
            : policy.modelfillmaterial;
        semantics.push_back(
            material == QStringLiteral("white")
            ? QStringLiteral("白墨模型填充")
            : QStringLiteral("W白墨通道打印"));
        sourcePolicies.push_back(
            QStringLiteral("modelFill=%1/%2")
                .arg(
                    material,
                    policy.modelfillscope));
    }
    if (probe.hasSupport)
    {
        printedChannels.push_back(
            QStringLiteral("S"));
        semantics.push_back(
            QStringLiteral("支撑填充"));
        QString supportPolicy =
            QStringLiteral("supportPlacement=")
            + (policy.supportplacement.isEmpty()
                   ? QStringLiteral("unknown")
                   : policy.supportplacement);
        if (!stats.supporttypesummary.isEmpty())
        {
            supportPolicy +=
                QStringLiteral(
                    "; layerSupportTypes=")
                + stats.supporttypesummary;
        }
        sourcePolicies.push_back(supportPolicy);
    }
    if (probe.hasVarnish)
    {
        printedChannels.push_back(
            QStringLiteral("V"));
        if (!probe.hasRgb
            && !probe.hasWhite
            && policy.modelfillmaterial
                == QStringLiteral("varnish"))
        {
            semantics.push_back(
                QStringLiteral("光油模型填充"));
        }
        else
        {
            semantics.push_back(
                QStringLiteral(
                    "光油表面或外侧层"));
        }
        QString varnishPolicy;
        if (policy.outervarnishenabled)
        {
            varnishPolicy +=
                QStringLiteral(
                    "outerVarnish=%1px")
                    .arg(
                        policy
                            .outervarnishthicknesspx);
        }
        if (policy.surfacevarnishenabled)
        {
            if (!varnishPolicy.isEmpty())
            {
                varnishPolicy +=
                    QStringLiteral("; ");
            }
            varnishPolicy +=
                QStringLiteral(
                    "surfaceVarnish=%1px")
                    .arg(
                        policy
                            .surfacevarnishthicknesspx);
        }
        sourcePolicies.push_back(
            varnishPolicy.isEmpty()
            ? QStringLiteral("varnishChannel=V")
            : varnishPolicy);
    }
    if (semantics.isEmpty())
    {
        return QStringLiteral(
            "打印通道=无 材料语义=真实空白 "
            "semantic=Empty sourcePolicy=emptyValue=255");
    }

    printedChannels.removeDuplicates();
    semantics.removeDuplicates();
    sourcePolicies.removeDuplicates();
    return QStringLiteral("打印通道=")
        + printedChannels.join(
            QStringLiteral("+"))
        + QStringLiteral(" 材料语义=")
        + semantics.join(QStringLiteral("+"))
        + QStringLiteral(" semantic=")
        + semantics.join(QStringLiteral("+"))
        + QStringLiteral(" sourcePolicy=")
        + sourcePolicies.join(
            QStringLiteral(" | "));
}

QString LayerPreviewPanel::BuildLayerSemanticText(
    const LayerPreviewLayerStats& stats) const
{
    QStringList parts;
    parts.push_back(
        QStringLiteral("TextureSurface=%1")
            .arg(stats.texturesurfacepixels));
    parts.push_back(
        QStringLiteral("ModelFill=%1")
            .arg(stats.modelfillpixels));
    parts.push_back(
        QStringLiteral("Support=%1")
            .arg(stats.supportprintpixels));
    if (stats.internalvoidsupportpixels > 0)
    {
        parts.push_back(
            QStringLiteral("InternalVoidS=%1")
                .arg(
                    stats
                        .internalvoidsupportpixels));
    }
    if (stats.uppersurfacesupportpixels > 0)
    {
        parts.push_back(
            QStringLiteral("UpperS=%1")
                .arg(
                    stats
                        .uppersurfacesupportpixels));
    }
    if (stats.outervarnishpixels > 0)
    {
        parts.push_back(
            QStringLiteral("OuterV=%1")
                .arg(stats.outervarnishpixels));
    }
    if (stats.outersurfacevarnishpixels > 0
        || stats.innersurfacevarnishpixels > 0)
    {
        parts.push_back(
            QStringLiteral(
                "SurfaceV(out/in)=%1/%2")
                .arg(
                    stats
                        .outersurfacevarnishpixels)
                .arg(
                    stats
                        .innersurfacevarnishpixels));
    }
    return QStringLiteral("semantic: ")
        + parts.join(QStringLiteral(", "));
}

QString LayerPreviewPanel::BuildSourcePolicyText() const
{
    const LayerPreviewSemanticPolicy policy =
        m_package.semanticpolicy;
    QStringList parts;
    if (!policy.modelfillmaterial.isEmpty())
    {
        parts.push_back(
            QStringLiteral("modelFill=%1/%2")
                .arg(
                    policy.modelfillmaterial,
                    policy.modelfillscope));
    }
    if (!policy.supportplacement.isEmpty())
    {
        QString support =
            QStringLiteral("support=")
            + policy.supportplacement;
        if (!policy.supportsource.isEmpty())
        {
            support += QStringLiteral("(")
                + policy.supportsource
                + QStringLiteral(")");
        }
        parts.push_back(support);
    }
    parts.push_back(
        QStringLiteral("internalVoid=%1")
            .arg(
                policy.internalvoidsupportenabled
                ? QStringLiteral("on")
                : QStringLiteral("off")));
    if (policy.outervarnishenabled)
    {
        parts.push_back(
            QStringLiteral(
                "outerVarnish=%1px/%2mm")
                .arg(
                    policy
                        .outervarnishthicknesspx)
                .arg(
                    policy
                        .outervarnishthicknessmm,
                    0,
                    'f',
                    3));
    }
    if (policy.surfacevarnishenabled)
    {
        parts.push_back(
            QStringLiteral(
                "surfaceVarnish=%1px")
                .arg(
                    policy
                        .surfacevarnishthicknesspx));
    }
    if (!policy.semanticpriority.isEmpty())
    {
        parts.push_back(
            QStringLiteral("priority=")
            + policy.semanticpriority);
    }
    return parts.isEmpty()
        ? QString{}
        : QStringLiteral("sourcePolicy: ")
            + parts.join(QStringLiteral(", "));
}

QString LayerPreviewPanel::ApplyPixelProbe(
    const int displayX,
    const int displayY)
{
    m_probeText = BuildPixelProbeText(
        displayX,
        displayY);
    UpdateStatus();
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

void LayerPreviewPanel::ClearCurrentLayer()
{
    m_currentBuffer.reset();
    m_currentPreview =
        slicer_core::MaterialPreviewResult{};
    m_currentImage = {};
    m_loading = false;
    m_lastCacheHit = false;
    m_requestedLayerIndex = -1;
    m_imageLabel->clear();
    emit SigLayerBufferReady(TiffLayerBufferPtr{});
}

QSize LayerPreviewPanel::PhysicalDisplaySize() const
{
    const QSize rasterSize =
        m_currentImage.isNull()
        ? QSize(
              m_package.widthpx,
              m_package.heightpx)
        : m_currentImage.size();
    return PreviewPhysicalScaleResolver::DisplaySize(
        rasterSize,
        m_package.physicalscale);
}
