#pragma once

#include "../services/PackageLoader.h"

#include <QVector>
#include <QWidget>

class LayerPreviewPanel;
class PreviewOverlayPanel;
class PreviewPanel;
class QComboBox;
class QLabel;
class QStackedWidget;

/**
 * @brief Available views in the unified preview workspace.
 */
enum class PreviewWorkspaceMode
{
    ProductionLayer = 0,
    MaterialOverlay = 1,
    RawPreview = 2,
};

/**
 * @brief Coordinates existing preview panels with one shared real layer index.
 */
class PreviewWorkspace final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the unified preview workspace.
     * @param parent Qt parent widget.
     */
    explicit PreviewWorkspace(QWidget* parent = nullptr);

    /**
     * @brief Load one output package into all existing preview views.
     * @param package Package summary produced by PackageLoader.
     */
    void LoadPackage(const PackageSummary& package);

    /**
     * @brief Return the canonical real layer range used by the workspace.
     * @return Ascending real package layer indices.
     */
    QVector<int> LayerIndices() const;

    /**
     * @brief Return the real layer shared by all preview modes.
     * @return Real package layer index, or -1 when no package is loaded.
     */
    int CurrentLayerIndex() const;

    /**
     * @brief Select an exact real layer in all preview modes.
     * @param layerIndex Target real package layer index.
     * @return true when the layer belongs to the canonical package range.
     */
    bool SelectLayer(int layerIndex);

    /**
     * @brief Select the active preview mode without changing the shared layer.
     * @param mode Target workspace mode.
     */
    void SetMode(PreviewWorkspaceMode mode);

    /**
     * @brief Return the active preview mode.
     * @return Current workspace mode.
     */
    PreviewWorkspaceMode CurrentMode() const;

    /**
     * @brief Return the workspace synchronization status for UI smoke tests.
     * @return Human-readable shared-layer status.
     */
    QString StatusForTest() const;

signals:
    /**
     * @brief Emitted when the shared real layer changes.
     * @param layerIndex New shared package layer index.
     */
    void SigLayerIndexChanged(int layerIndex);

private slots:
    void OnModeChanged(int index);
    void OnPanelLayerIndexChanged(int layerIndex);

private:
    void RebuildCanonicalLayers();
    void SyncPanels();
    void UpdateStatus();
    QString ModeName(PreviewWorkspaceMode mode) const;

    QVector<int> m_layerIndices;
    int m_currentLayerIndex{-1};
    bool m_syncing{false};

    QComboBox* m_modeSelector{nullptr};
    QLabel* m_status{nullptr};
    QStackedWidget* m_stack{nullptr};
    LayerPreviewPanel* m_productionView{nullptr};
    PreviewOverlayPanel* m_overlayView{nullptr};
    PreviewPanel* m_rawPreviewView{nullptr};
};
