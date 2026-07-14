#include "PreviewWorkspace.h"

#include "LayerPreviewPanel.h"
#include "PreviewOverlayPanel.h"
#include "PreviewPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSet>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

PreviewWorkspace::PreviewWorkspace(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("previewWorkspace"));

    auto* layout = new QVBoxLayout(this);
    auto* modeRow = new QHBoxLayout();
    m_modeSelector = new QComboBox(this);
    m_modeSelector->setObjectName(QStringLiteral("previewWorkspaceModeSelector"));
    m_modeSelector->addItem(QStringLiteral("生产层检查"), static_cast<int>(PreviewWorkspaceMode::ProductionLayer));
    m_modeSelector->addItem(QStringLiteral("材料叠加"), static_cast<int>(PreviewWorkspaceMode::MaterialOverlay));
    m_modeSelector->addItem(QStringLiteral("原始调试预览"), static_cast<int>(PreviewWorkspaceMode::RawPreview));
    m_modeSelector->setToolTip(
        QStringLiteral("切换预览数据源时保持同一真实 layerIndex；当前层缺图时不会跨层寻找替代图。"));
    modeRow->addWidget(new QLabel(QStringLiteral("预览模式"), this));
    modeRow->addWidget(m_modeSelector);
    modeRow->addStretch(1);
    layout->addLayout(modeRow);

    m_status = new QLabel(QStringLiteral("尚未加载预览输出包。"), this);
    m_status->setObjectName(QStringLiteral("previewWorkspaceStatus"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_stack = new QStackedWidget(this);
    m_productionView = new LayerPreviewPanel(m_stack);
    m_productionView->setObjectName(QStringLiteral("productionLayerView"));
    m_overlayView = new PreviewOverlayPanel(m_stack);
    m_overlayView->setObjectName(QStringLiteral("materialOverlayView"));
    m_rawPreviewView = new PreviewPanel(m_stack);
    m_rawPreviewView->setObjectName(QStringLiteral("rawPreviewView"));
    m_stack->addWidget(m_productionView);
    m_stack->addWidget(m_overlayView);
    m_stack->addWidget(m_rawPreviewView);
    layout->addWidget(m_stack, 1);

    connect(
        m_modeSelector,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &PreviewWorkspace::OnModeChanged);
    connect(
        m_productionView,
        &LayerPreviewPanel::SigLayerIndexChanged,
        this,
        &PreviewWorkspace::OnPanelLayerIndexChanged);
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
    RebuildCanonicalLayers();
    m_currentLayerIndex = m_layerIndices.isEmpty() ? -1 : m_layerIndices.first();
    SyncPanels();
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

QString PreviewWorkspace::StatusForTest() const
{
    return m_status == nullptr ? QString{} : m_status->text();
}

void PreviewWorkspace::OnModeChanged(const int index)
{
    const PreviewWorkspaceMode mode = static_cast<PreviewWorkspaceMode>(
        m_modeSelector->itemData(index).toInt());
    m_stack->setCurrentIndex(static_cast<int>(mode));
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
            "共享 layer=%1  模式=%2  生产=%3  叠加=%4  原始=%5  缺图不跨层兜底")
            .arg(m_currentLayerIndex)
            .arg(ModeName(CurrentMode()))
            .arg(productionState, overlayState, rawState));
}

QString PreviewWorkspace::ModeName(const PreviewWorkspaceMode mode) const
{
    switch (mode)
    {
    case PreviewWorkspaceMode::MaterialOverlay:
        return QStringLiteral("材料叠加");
    case PreviewWorkspaceMode::RawPreview:
        return QStringLiteral("原始调试预览");
    case PreviewWorkspaceMode::ProductionLayer:
    default:
        return QStringLiteral("生产层检查");
    }
}
