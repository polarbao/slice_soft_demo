#include "DiagnosticSemanticPreviewPanel.h"

#include "../services/MaterialPreviewImageAdapter.h"
#include "../services/PreviewPhysicalScale.h"

#include "slicer_core/preview/MaterialPreviewComposer.h"

#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace
{

const QColor kTextureColor{0, 151, 167};
const QColor kFillColor{230, 126, 34};
const QColor kSupportColor{38, 194, 74};
const QColor kVarnishColor{127, 127, 127};
const QColor kEmptyColor{255, 255, 255};

QColor Blend(
    const QColor& base,
    const QColor& overlay,
    const int overlayWeight)
{
    const int baseWeight = 255 - overlayWeight;
    return QColor(
        (base.red() * baseWeight
         + overlay.red() * overlayWeight)
            / 255,
        (base.green() * baseWeight
         + overlay.green() * overlayWeight)
            / 255,
        (base.blue() * baseWeight
         + overlay.blue() * overlayWeight)
            / 255);
}

QString ShortIdentity(const QString& value)
{
    return value.size() <= 12
        ? value
        : value.left(12);
}

}  // namespace

DiagnosticSemanticPreviewPanel::
    DiagnosticSemanticPreviewPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(
        QStringLiteral("diagnosticSemanticPreviewPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* controls = new QHBoxLayout();
    controls->addWidget(
        new QLabel(QStringLiteral("语义显示"), this));
    m_displayModeSelector = new QComboBox(this);
    m_displayModeSelector->setObjectName(
        QStringLiteral(
            "diagnosticSemanticDisplayModeSelector"));
    m_displayModeSelector->addItem(
        QStringLiteral("分区 + S 支撑 + V 光油"),
        static_cast<int>(
            DiagnosticSemanticDisplayMode::
                PartitionSupportVarnish));
    m_displayModeSelector->addItem(
        QStringLiteral("Texture Surface 纹理表面层"),
        static_cast<int>(
            DiagnosticSemanticDisplayMode::
                TextureSurface));
    m_displayModeSelector->addItem(
        QStringLiteral("Model Fill 模型填充层"),
        static_cast<int>(
            DiagnosticSemanticDisplayMode::ModelFill));
    m_displayModeSelector->setToolTip(
        QStringLiteral(
            "Texture/Fill 来自当前诊断证据，S/V 来自同一 layer 的生产 TIFF；"
            "所有颜色仅为显示伪彩。"));
    controls->addWidget(m_displayModeSelector);
    controls->addStretch(1);
    layout->addLayout(controls);

    m_status = new QLabel(
        QStringLiteral(
            "未评估：尚未提供诊断证据和生产 TIFF 层。"),
        this);
    m_status->setObjectName(
        QStringLiteral("diagnosticSemanticStatus"));
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(
        Qt::TextSelectableByMouse);
    layout->addWidget(m_status);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(
        QStringLiteral("diagnosticSemanticScrollArea"));
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_imageLabel = new QLabel(m_scrollArea);
    m_imageLabel->setObjectName(
        QStringLiteral("diagnosticSemanticImage"));
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(1, 1);
    m_scrollArea->setWidget(m_imageLabel);
    layout->addWidget(m_scrollArea, 1);

    connect(
        m_displayModeSelector,
        qOverload<int>(
            &QComboBox::currentIndexChanged),
        this,
        &DiagnosticSemanticPreviewPanel::
            OnDisplayModeChanged);
}

void DiagnosticSemanticPreviewPanel::
    SetDiagnosticAnalysis(
        const DiagnosticAnalysisResult& result)
{
    m_analysis = result;
    Compose();
}

void DiagnosticSemanticPreviewPanel::
    ClearDiagnosticAnalysis(const QString& reason)
{
    m_analysis.reset();
    SetUnavailable(
        reason.isEmpty()
            ? QStringLiteral(
                  "未评估：诊断证据已清除。")
            : QStringLiteral("未评估：") + reason);
}

void DiagnosticSemanticPreviewPanel::SetProductionLayer(
    TiffLayerBufferPtr buffer)
{
    m_productionLayer = std::move(buffer);
    Compose();
}

void DiagnosticSemanticPreviewPanel::
    SetMaterialClosureSummary(
        const MaterialClosureDiagnosticsSummary& summary)
{
    m_closureSummary = summary;
    Compose();
}

bool DiagnosticSemanticPreviewPanel::
    SetDisplayModeForTest(
        const DiagnosticSemanticDisplayMode mode)
{
    const int index = m_displayModeSelector->findData(
        static_cast<int>(mode));
    if (index < 0)
    {
        return false;
    }
    m_displayModeSelector->setCurrentIndex(index);
    return true;
}

QImage DiagnosticSemanticPreviewPanel::
    CurrentImageForTest() const
{
    return m_image;
}

QString DiagnosticSemanticPreviewPanel::StatusForTest() const
{
    return m_status->text();
}

int DiagnosticSemanticPreviewPanel::LayerIndexForTest() const
{
    return m_semantics.layerindex;
}

void DiagnosticSemanticPreviewPanel::resizeEvent(
    QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    ApplyImage();
}

void DiagnosticSemanticPreviewPanel::
    OnDisplayModeChanged(const int index)
{
    Q_UNUSED(index);
    Compose();
}

void DiagnosticSemanticPreviewPanel::Compose()
{
    if (m_productionLayer == nullptr)
    {
        SetUnavailable(
            QStringLiteral(
                "未评估：当前真实 layer 的生产 TIFF 尚未加载。"));
        return;
    }
    if (!m_analysis.has_value()
        || m_analysis->state
            != DiagnosticAnalysisState::Succeeded
        || m_analysis->evidence == nullptr)
    {
        SetUnavailable(
            QStringLiteral(
                "未评估：当前场景尚无成功的 Texture/Fill 诊断证据。"));
        return;
    }
    if (m_productionLayer->sceneidentityavailable
        && (QString::fromStdString(
                m_productionLayer->sceneid)
                != m_analysis->identity.sceneid
            || m_productionLayer->scenerevision
                != m_analysis->identity.scenerevision))
    {
        SetUnavailable(
            QStringLiteral(
                "未评估：生产包 sceneId/revision 与当前诊断身份不一致。"));
        return;
    }

    slicer_core::
        TextureFillPartitionSemanticPreviewRequest
            request;
    slicer_core::
        TextureFillPartitionSemanticPreviewClosureEvidence
            closureEvidence;
    closureEvidence.available =
        m_closureSummary.reportavailable
        && m_closureSummary.schemavalid;
    closureEvidence.exact =
        closureEvidence.available
        && !m_closureSummary.candidateonly
        && m_closureSummary.confidence
            == QStringLiteral("exact");
    closureEvidence.layers.reserve(
        static_cast<std::size_t>(
            m_closureSummary.layers.size()));
    for (const MaterialClosureLayerUi& layer :
         m_closureSummary.layers)
    {
        slicer_core::
            TextureFillPartitionSemanticPreviewClosureLayer
                closureLayer;
        closureLayer.layerindex = layer.layerindex;
        closureLayer.zmm = layer.zmm;
        closureLayer.closurepass =
            layer.closurestatus == QStringLiteral("pass");
        closureLayer.gappixels =
            static_cast<std::uint64_t>(
                std::max(0, layer.gappixels));
        closureEvidence.layers.push_back(
            std::move(closureLayer));
    }
    request.partition =
        &m_analysis->evidence->partition;
    request.productionlayer =
        m_productionLayer.get();
    request.closureevidence = &closureEvidence;
    m_semantics =
        slicer_core::
            BuildTextureFillPartitionSemanticPreview(
                request);
    if (!m_semantics.available)
    {
        SetUnavailable(
            QStringLiteral("未评估：%1（%2）")
                .arg(
                    QString::fromStdString(
                        m_semantics.message),
                    QString::fromStdString(
                        m_semantics.errorcode)));
        return;
    }

    slicer_core::MaterialPreviewRequest baseRequest;
    baseRequest.mode =
        slicer_core::MaterialPreviewMode::
            RgbSupportWhiteVarnish;
    baseRequest.palette.support = {
        static_cast<std::uint8_t>(
            kSupportColor.red()),
        static_cast<std::uint8_t>(
            kSupportColor.green()),
        static_cast<std::uint8_t>(
            kSupportColor.blue()),
        210U};
    baseRequest.palette.varnish = {
        static_cast<std::uint8_t>(
            kVarnishColor.red()),
        static_cast<std::uint8_t>(
            kVarnishColor.green()),
        static_cast<std::uint8_t>(
            kVarnishColor.blue()),
        210U};
    const slicer_core::MaterialPreviewResult base =
        slicer_core::MaterialPreviewComposer::Compose(
            *m_productionLayer,
            baseRequest);
    const QImage productionImage =
        MaterialPreviewImageAdapter::ToDisplayImage(
            base);
    m_image = QImage(
        productionImage.size(),
        QImage::Format_RGB888);
    m_image.fill(kEmptyColor);

    const DiagnosticSemanticDisplayMode mode =
        CurrentDisplayMode();
    for (int displayY{0};
         displayY < m_image.height();
         ++displayY)
    {
        const int sourceY =
            m_image.height() - 1 - displayY;
        for (int x{0}; x < m_image.width(); ++x)
        {
            const std::size_t sourceIndex =
                static_cast<std::size_t>(sourceY)
                    * static_cast<std::size_t>(
                        m_image.width())
                + static_cast<std::size_t>(x);
            QColor color = kEmptyColor;
            if (mode
                == DiagnosticSemanticDisplayMode::
                    PartitionSupportVarnish)
            {
                color = productionImage.pixelColor(
                    x,
                    displayY);
                if (m_semantics.supportmask.at(
                        sourceIndex)
                    != 0U)
                {
                    color = kSupportColor;
                }
                if (m_semantics.varnishmask.at(
                        sourceIndex)
                    != 0U)
                {
                    color = kVarnishColor;
                }
            }
            if (m_semantics.modelfillmask.at(
                    sourceIndex)
                != 0U
                && mode
                    != DiagnosticSemanticDisplayMode::
                        TextureSurface)
            {
                color = mode
                        == DiagnosticSemanticDisplayMode::
                            PartitionSupportVarnish
                    ? Blend(color, kFillColor, 200)
                    : kFillColor;
            }
            if (m_semantics.texturesurfacemask.at(
                    sourceIndex)
                != 0U
                && mode
                    != DiagnosticSemanticDisplayMode::
                        ModelFill)
            {
                color = mode
                        == DiagnosticSemanticDisplayMode::
                            PartitionSupportVarnish
                    ? Blend(color, kTextureColor, 155)
                    : kTextureColor;
            }
            m_image.setPixelColor(x, displayY, color);
        }
    }

    const QString identityWarning =
        m_productionLayer->sceneidentityavailable
        ? QStringLiteral("scene 身份已匹配")
        : QStringLiteral(
              "生产包未提供 scene 身份，叠加仅作诊断显示");
    m_status->setText(
        QStringLiteral(
            "同层 layer=%1  z=%2 mm；Texture=%3（%4%）；"
            "Fill=%5（%6%）；W=%7；S=%8；V=%9；width=%10 mm；"
            "allTexture=%11；%12；config=%13；"
            "材料闭环=%14（gap=%15）")
            .arg(m_semantics.layerindex)
            .arg(m_semantics.zmm, 0, 'f', 3)
            .arg(m_semantics.texturesurfacepixels)
            .arg(
                m_semantics.texturecoverage * 100.0,
                0,
                'f',
                1)
            .arg(m_semantics.modelfillpixels)
            .arg(
                m_semantics.modelfillcoverage * 100.0,
                0,
                'f',
                1)
            .arg(m_semantics.whitepixels)
            .arg(m_semantics.supportpixels)
            .arg(m_semantics.varnishpixels)
            .arg(
                m_analysis->evidence->partition
                    .widthMetrics.effectiveWidthMm,
                0,
                'f',
                2)
            .arg(
                m_semantics.alltexture
                    ? QStringLiteral("是")
                    : QStringLiteral("否"))
            .arg(identityWarning)
            .arg(ShortIdentity(
                m_analysis->identity.confighash))
            .arg(
                m_semantics.fullclosurepass
                    ? QStringLiteral("通过")
                    : QStringLiteral("未通过"))
            .arg(m_semantics.fullclosuregappixels));
    ApplyImage();
}

void DiagnosticSemanticPreviewPanel::ApplyImage()
{
    if (m_image.isNull() || m_productionLayer == nullptr)
    {
        m_imageLabel->clear();
        return;
    }

    PreviewPhysicalScale scale;
    scale.dpix = m_productionLayer->dpiX;
    scale.dpiy = m_productionLayer->dpiY;
    scale.pixelsizexmm =
        m_productionLayer->pixelsizexmm;
    scale.pixelsizeymm =
        m_productionLayer->pixelsizeymm;
    scale.available =
        scale.pixelsizexmm > 0.0
        && scale.pixelsizeymm > 0.0;
    const QSize physicalSize =
        PreviewPhysicalScaleResolver::DisplaySize(
            m_image.size(),
            scale);
    const QSize viewportSize =
        (m_scrollArea->viewport()->size()
         - QSize(8, 8))
            .expandedTo(QSize(1, 1));
    const QSize displaySize = physicalSize.scaled(
        viewportSize,
        Qt::KeepAspectRatio);
    m_imageLabel->setPixmap(
        QPixmap::fromImage(m_image).scaled(
            displaySize,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation));
    m_imageLabel->resize(displaySize);
}

void DiagnosticSemanticPreviewPanel::SetUnavailable(
    const QString& status)
{
    m_semantics = {};
    m_image = {};
    m_status->setText(status);
    ApplyImage();
}

DiagnosticSemanticDisplayMode
DiagnosticSemanticPreviewPanel::CurrentDisplayMode() const
{
    return static_cast<DiagnosticSemanticDisplayMode>(
        m_displayModeSelector->currentData().toInt());
}
