#include "PreviewWorkspace.h"

#include "DiagnosticSemanticPreviewPanel.h"
#include "LayerPreviewPanel.h"
#include "PreviewOverlayPanel.h"
#include "PreviewPanel.h"

#include "../services/MaterialClosureReportInterpreter.h"

#include <QComboBox>
#include <QFrame>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSet>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{

QLabel* AddLegendEntry(
    QGridLayout* layout,
    QWidget* parent,
    const QString& objectName,
    const QString& text,
    const int row,
    const int column)
{
    auto* entry = new QWidget(parent);
    auto* entryLayout = new QHBoxLayout(entry);
    entryLayout->setContentsMargins(0, 0, 0, 0);
    entryLayout->setSpacing(4);

    auto* swatch = new QLabel(entry);
    swatch->setObjectName(objectName + QStringLiteral("Swatch"));
    swatch->setFixedSize(16, 16);
    entryLayout->addWidget(swatch);

    auto* label = new QLabel(text, entry);
    label->setObjectName(objectName + QStringLiteral("Label"));
    entryLayout->addWidget(label);
    entryLayout->addStretch(1);
    layout->addWidget(entry, row, column);
    return swatch;
}

}  // namespace

PreviewWorkspace::PreviewWorkspace(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("previewWorkspace"));

    auto* layout = new QVBoxLayout(this);
    auto* modeRow = new QHBoxLayout();
    m_modeSelector = new QComboBox(this);
    m_modeSelector->setObjectName(QStringLiteral("previewWorkspaceModeSelector"));
    m_modeSelector->addItem(
        QStringLiteral("生产预览"),
        static_cast<int>(PreviewWorkspaceMode::Production));
    m_modeSelector->addItem(
        QStringLiteral("诊断预览"),
        static_cast<int>(PreviewWorkspaceMode::Diagnostic));
    m_modeSelector->setToolTip(
        QStringLiteral(
            "生产预览只读取 manifest 同层 RGBWSV TIFF；诊断预览保留 Overlay 与原始调试图，缺图不跨层兜底。"));
    modeRow->addWidget(new QLabel(QStringLiteral("预览模式"), this));
    modeRow->addWidget(m_modeSelector);
    modeRow->addStretch(1);
    layout->addLayout(modeRow);

    m_status = new QLabel(QStringLiteral("尚未加载预览输出包。"), this);
    m_status->setObjectName(QStringLiteral("previewWorkspaceStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* legendBar = new QFrame(this);
    legendBar->setObjectName(QStringLiteral("previewLegendBar"));
    auto* legendLayout = new QGridLayout(legendBar);
    legendLayout->setContentsMargins(0, 0, 0, 0);
    legendLayout->setSpacing(6);
    auto* legendTitle = new QLabel(QStringLiteral("材料图例"), legendBar);
    legendTitle->setObjectName(QStringLiteral("previewLegendTitle"));
    legendLayout->addWidget(legendTitle, 0, 0, 1, 3);
    m_rgbLegendSwatch = AddLegendEntry(
        legendLayout,
        legendBar,
        QStringLiteral("legendRgb"),
        QStringLiteral("RGB 模型颜色/填充"),
        1,
        0);
    m_whiteLegendSwatch = AddLegendEntry(
        legendLayout,
        legendBar,
        QStringLiteral("legendWhite"),
        QStringLiteral("W 白墨填充"),
        1,
        1);
    m_supportLegendSwatch = AddLegendEntry(
        legendLayout,
        legendBar,
        QStringLiteral("legendSupport"),
        QStringLiteral("S 支撑"),
        1,
        2);
    m_varnishLegendSwatch = AddLegendEntry(
        legendLayout,
        legendBar,
        QStringLiteral("legendVarnish"),
        QStringLiteral("V 光油/填充"),
        2,
        0);
    m_emptyLegendSwatch = AddLegendEntry(
        legendLayout,
        legendBar,
        QStringLiteral("legendEmpty"),
        QStringLiteral("真实空白"),
        2,
        1);
    legendLayout->setColumnStretch(2, 1);
    layout->addWidget(legendBar);

    m_protocolHint = new QLabel(
        QStringLiteral(
            "生产值：RGBWSV uint8、black_is_print，0=打印，255=不打印；图例颜色仅用于显示，不等于 TIFF 生产值。"),
        this);
    m_protocolHint->setObjectName(QStringLiteral("previewProtocolHint"));
    m_protocolHint->setWordWrap(true);
    layout->addWidget(m_protocolHint);

    m_probeContext = new QLabel(DefaultProbeGuidance(), this);
    m_probeContext->setObjectName(QStringLiteral("previewProbeContext"));
    m_probeContext->setWordWrap(true);
    m_probeContext->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_probeContext);

    m_stack = new QStackedWidget(this);
    m_productionView = new LayerPreviewPanel(m_stack);
    m_productionView->setObjectName(QStringLiteral("productionLayerView"));

    m_diagnosticContainer = new QWidget(m_stack);
    m_diagnosticContainer->setObjectName(
        QStringLiteral("diagnosticPreviewContainer"));
    auto* diagnosticLayout =
        new QVBoxLayout(m_diagnosticContainer);
    diagnosticLayout->setContentsMargins(0, 0, 0, 0);
    auto* diagnosticControls = new QHBoxLayout();
    m_diagnosticModeSelector =
        new QComboBox(m_diagnosticContainer);
    m_diagnosticModeSelector->setObjectName(
        QStringLiteral("diagnosticPreviewModeSelector"));
    m_diagnosticModeSelector->addItem(
        QStringLiteral("纹理/填充同层语义"),
        static_cast<int>(
            DiagnosticPreviewMode::
                TextureFillSemantics));
    m_diagnosticModeSelector->addItem(
        QStringLiteral("材料与闭环诊断"),
        static_cast<int>(
            DiagnosticPreviewMode::MaterialOverlay));
    m_diagnosticModeSelector->addItem(
        QStringLiteral("原始调试图"),
        static_cast<int>(
            DiagnosticPreviewMode::RawPreview));
    m_diagnosticModeSelector->setToolTip(
        QStringLiteral(
            "纹理/填充语义使用当前分析证据和同层生产 TIFF；"
            "旧诊断图来自 preview/report，缺失时不跨层兜底。"));
    diagnosticControls->addWidget(
        new QLabel(QStringLiteral("诊断来源"), m_diagnosticContainer));
    diagnosticControls->addWidget(m_diagnosticModeSelector);
    diagnosticControls->addStretch(1);
    diagnosticLayout->addLayout(diagnosticControls);

    m_diagnosticStack =
        new QStackedWidget(m_diagnosticContainer);
    m_semanticView =
        new DiagnosticSemanticPreviewPanel(
            m_diagnosticStack);
    m_overlayView =
        new PreviewOverlayPanel(m_diagnosticStack);
    m_overlayView->setObjectName(QStringLiteral("materialOverlayView"));
    m_rawPreviewView =
        new PreviewPanel(m_diagnosticStack);
    m_rawPreviewView->setObjectName(QStringLiteral("rawPreviewView"));
    m_diagnosticStack->addWidget(m_semanticView);
    m_diagnosticStack->addWidget(m_overlayView);
    m_diagnosticStack->addWidget(m_rawPreviewView);
    diagnosticLayout->addWidget(m_diagnosticStack, 1);

    m_stack->addWidget(m_productionView);
    m_stack->addWidget(m_diagnosticContainer);
    layout->addWidget(m_stack, 1);

    connect(
        m_modeSelector,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &PreviewWorkspace::OnModeChanged);
    connect(
        m_diagnosticModeSelector,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &PreviewWorkspace::OnDiagnosticModeChanged);
    connect(
        m_productionView,
        &LayerPreviewPanel::SigLayerIndexChanged,
        this,
        &PreviewWorkspace::OnPanelLayerIndexChanged);
    connect(
        m_productionView,
        &LayerPreviewPanel::SigPixelProbeChanged,
        this,
        &PreviewWorkspace::OnPixelProbeChanged);
    connect(
        m_productionView,
        &LayerPreviewPanel::SigLayerBufferReady,
        m_semanticView,
        &DiagnosticSemanticPreviewPanel::
            SetProductionLayer);
    connect(
        m_overlayView,
        &PreviewOverlayPanel::SigLayerIndexChanged,
        this,
        &PreviewWorkspace::OnPanelLayerIndexChanged);
    connect(
        m_rawPreviewView,
        &PreviewPanel::SigLayerIndexChanged,
        this,
        &PreviewWorkspace::OnPanelLayerIndexChanged);
}

void PreviewWorkspace::LoadPackage(const PackageSummary& package)
{
    m_productionView->LoadPackage(package);
    m_overlayView->loadPackage(package);
    m_rawPreviewView->loadPackage(package);
    QString closureReportPath;
    for (const QString& reportPath : package.report_paths)
    {
        if (QFileInfo(reportPath).fileName()
            == QStringLiteral("material_closure_report.json"))
        {
            closureReportPath = reportPath;
            break;
        }
    }
    m_semanticView->SetMaterialClosureSummary(
        MaterialClosureReportInterpreter::Interpret(
            closureReportPath,
            package.package_dir));
    RebuildCanonicalLayers();
    m_currentLayerIndex = m_layerIndices.isEmpty() ? -1 : m_layerIndices.first();
    m_probeContext->setText(DefaultProbeGuidance());
    UpdateLegend();
    SyncPanels();
    UpdateStatus();
}

void PreviewWorkspace::SetDiagnosticAnalysis(
    const DiagnosticAnalysisResult& result)
{
    m_semanticView->SetDiagnosticAnalysis(result);
    UpdateStatus();
}

void PreviewWorkspace::ClearDiagnosticAnalysis(
    const QString& reason)
{
    m_semanticView->ClearDiagnosticAnalysis(reason);
    UpdateStatus();
}

QVector<int> PreviewWorkspace::LayerIndices() const
{
    return m_layerIndices;
}

int PreviewWorkspace::CurrentLayerIndex() const
{
    return m_currentLayerIndex;
}

bool PreviewWorkspace::SelectLayer(const int layerIndex)
{
    if (!m_layerIndices.contains(layerIndex))
    {
        return false;
    }
    if (m_currentLayerIndex == layerIndex)
    {
        SyncPanels();
        UpdateStatus();
        return true;
    }

    m_currentLayerIndex = layerIndex;
    SyncPanels();
    UpdateStatus();
    emit SigLayerIndexChanged(layerIndex);
    return true;
}

bool PreviewWorkspace::ShowMaterialClosureLayer(
    const int layerIndex,
    const QString& gapPreviewPath)
{
    const bool hasGapPreview =
        m_overlayView->ShowMaterialClosureGapPreview(layerIndex, gapPreviewPath);
    if (hasGapPreview && !m_layerIndices.contains(layerIndex))
    {
        m_layerIndices.push_back(layerIndex);
        std::sort(m_layerIndices.begin(), m_layerIndices.end());
    }

    const bool selected = SelectLayer(layerIndex);
    if (hasGapPreview)
    {
        SetDiagnosticMode(
            DiagnosticPreviewMode::MaterialOverlay);
        SetMode(PreviewWorkspaceMode::Diagnostic);
        SyncPanels();
        UpdateStatus();
    }
    return selected;
}

void PreviewWorkspace::SetMode(const PreviewWorkspaceMode mode)
{
    const int index = m_modeSelector->findData(static_cast<int>(mode));
    if (index >= 0)
    {
        m_modeSelector->setCurrentIndex(index);
    }
}

PreviewWorkspaceMode PreviewWorkspace::CurrentMode() const
{
    return static_cast<PreviewWorkspaceMode>(m_modeSelector->currentData().toInt());
}

void PreviewWorkspace::SetDiagnosticMode(
    const DiagnosticPreviewMode mode)
{
    const int index =
        m_diagnosticModeSelector->findData(
            static_cast<int>(mode));
    if (index >= 0)
    {
        m_diagnosticModeSelector->setCurrentIndex(index);
    }
}

DiagnosticPreviewMode
PreviewWorkspace::CurrentDiagnosticMode() const
{
    return static_cast<DiagnosticPreviewMode>(
        m_diagnosticModeSelector->currentData().toInt());
}

QString PreviewWorkspace::StatusForTest() const
{
    return m_status == nullptr ? QString{} : m_status->text();
}

QString PreviewWorkspace::LegendTextForTest() const
{
    return QStringLiteral(
        "RGB 模型颜色/填充；W 白墨模型填充；S 模型外支撑或内部镂空支撑；V 光油或光油模型填充；真实空白无材料；"
        "RGBWSV uint8；black_is_print；0=打印；255=不打印；显示颜色不等于生产值");
}

QString PreviewWorkspace::ProbeContextForTest() const
{
    return m_probeContext == nullptr ? QString{} : m_probeContext->text();
}

void PreviewWorkspace::OnModeChanged(const int index)
{
    const PreviewWorkspaceMode mode = static_cast<PreviewWorkspaceMode>(
        m_modeSelector->itemData(index).toInt());
    m_stack->setCurrentIndex(static_cast<int>(mode));
    SyncPanels();
    UpdateStatus();
}

void PreviewWorkspace::OnDiagnosticModeChanged(
    const int index)
{
    const DiagnosticPreviewMode mode =
        static_cast<DiagnosticPreviewMode>(
            m_diagnosticModeSelector
                ->itemData(index)
                .toInt());
    m_diagnosticStack->setCurrentIndex(
        static_cast<int>(mode));
    SyncPanels();
    UpdateStatus();
}

void PreviewWorkspace::OnPanelLayerIndexChanged(const int layerIndex)
{
    if (m_syncing || !m_layerIndices.contains(layerIndex))
    {
        return;
    }
    m_currentLayerIndex = layerIndex;
    SyncPanels();
    UpdateStatus();
    emit SigLayerIndexChanged(layerIndex);
}

void PreviewWorkspace::OnPixelProbeChanged(const QString& context)
{
    m_probeContext->setText(context.isEmpty() ? DefaultProbeGuidance() : context);
}

void PreviewWorkspace::RebuildCanonicalLayers()
{
    m_layerIndices = m_productionView->LayerIndices();
    if (!m_layerIndices.isEmpty())
    {
        return;
    }

    QSet<int> layers;
    for (const int layerIndex : m_overlayView->LayerIndices())
    {
        layers.insert(layerIndex);
    }
    for (const int layerIndex : m_rawPreviewView->LayerIndices())
    {
        layers.insert(layerIndex);
    }
    m_layerIndices = layers.values().toVector();
    std::sort(m_layerIndices.begin(), m_layerIndices.end());
}

void PreviewWorkspace::SyncPanels()
{
    if (m_currentLayerIndex < 0)
    {
        return;
    }

    m_syncing = true;
    m_productionView->SelectLayer(m_currentLayerIndex);
    m_overlayView->SelectLayer(m_currentLayerIndex);
    m_rawPreviewView->SelectLayer(m_currentLayerIndex);
    m_syncing = false;
}

void PreviewWorkspace::UpdateStatus()
{
    if (m_currentLayerIndex < 0)
    {
        m_status->setText(QStringLiteral("尚未加载可用层。"));
        return;
    }

    const QString productionState = m_productionView->LayerIndices().contains(m_currentLayerIndex)
        ? QStringLiteral("同层")
        : QStringLiteral("缺失");
    const QString overlayState = m_overlayView->LayerIndices().contains(m_currentLayerIndex)
        ? QStringLiteral("同层")
        : QStringLiteral("同层无图");
    const QString rawState = m_rawPreviewView->LayerIndices().contains(m_currentLayerIndex)
        ? QStringLiteral("同层")
        : QStringLiteral("同层无图");
    m_status->setText(
        QStringLiteral(
            "共享 layer=%1  一级模式=%2  生产=%3  语义=%4  "
            "诊断Overlay=%5  诊断Raw=%6  缺图不跨层兜底")
            .arg(m_currentLayerIndex)
            .arg(ModeName(CurrentMode()))
            .arg(productionState)
            .arg(
                m_semanticView->LayerIndexForTest()
                        == m_currentLayerIndex
                    ? QStringLiteral("同层")
                    : QStringLiteral("未评估"))
            .arg(overlayState)
            .arg(rawState));
}

void PreviewWorkspace::UpdateLegend()
{
    m_rgbLegendSwatch->setStyleSheet(
        QStringLiteral(
            "border: 1px solid #666666; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "stop:0 #e74c3c, stop:0.5 #2ecc71, stop:1 #3498db);"));
    m_rgbLegendSwatch->setToolTip(QStringLiteral("RGB 为生产真彩色模型数据；色块只是通道提示。"));
    SetLegendSwatch(
        m_whiteLegendSwatch,
        m_productionView->PseudoColor(QStringLiteral("white")),
        QStringLiteral("W 白墨显示伪彩"));
    SetLegendSwatch(
        m_supportLegendSwatch,
        m_productionView->PseudoColor(QStringLiteral("support")),
        QStringLiteral("S 支撑显示伪彩"));
    SetLegendSwatch(
        m_varnishLegendSwatch,
        m_productionView->PseudoColor(QStringLiteral("varnish")),
        QStringLiteral("V 光油显示伪彩"));
    SetLegendSwatch(
        m_emptyLegendSwatch,
        m_productionView->PseudoColor(QStringLiteral("empty")),
        QStringLiteral("真实空白显示色；所有 RGBWSV 生产值均为 255"));
}

void PreviewWorkspace::SetLegendSwatch(
    QLabel* swatch,
    const QColor& color,
    const QString& tooltip)
{
    swatch->setStyleSheet(
        QStringLiteral("border: 1px solid #666666; background-color: rgb(%1, %2, %3);")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue()));
    swatch->setToolTip(
        QStringLiteral("%1：rgb(%2,%3,%4)，仅用于 UI 显示。")
            .arg(tooltip)
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue()));
}

QString PreviewWorkspace::DefaultProbeGuidance() const
{
    return QStringLiteral(
        "像素探针：切换到“生产预览”并点击图像，查看当前 layer 的 R/G/B/W/S/V 生产值、打印通道和材料语义。"
        "未选择像素时不根据预览颜色推断材料。");
}

QString PreviewWorkspace::ModeName(const PreviewWorkspaceMode mode) const
{
    switch (mode)
    {
    case PreviewWorkspaceMode::Diagnostic:
        return QStringLiteral("诊断预览");
    case PreviewWorkspaceMode::Production:
    default:
        return QStringLiteral("生产预览");
    }
}
