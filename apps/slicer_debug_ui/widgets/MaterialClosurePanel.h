#pragma once

#include "../services/MaterialClosureReportInterpreter.h"
#include "../services/PackageLoader.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;
class QTextEdit;

/**
 * @brief Displays p0.material_closure.1 without recomputing production evidence.
 */
class MaterialClosurePanel final : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Create the material-closure diagnostics panel.
     * @param parent Qt parent widget.
     */
    explicit MaterialClosurePanel(QWidget* parent = nullptr);

    /**
     * @brief Load the material-closure report owned by an output package.
     * @param package Package summary produced by PackageLoader.
     */
    void LoadPackage(const PackageSummary& package);

    /**
     * @brief Return the current Chinese summary for smoke tests.
     * @return Current summary text.
     */
    QString SummaryForTest() const;

    /**
     * @brief Return the current worst-layer row count for smoke tests.
     * @return Number of displayed worst layers.
     */
    int WorstLayerCountForTest() const;

    /**
     * @brief Select one worst-layer row for smoke tests.
     * @param row Zero-based row.
     * @return true when the row exists.
     */
    bool SelectWorstLayerForTest(int row);

    /**
     * @brief Trigger the selected worst-layer navigation for smoke tests.
     * @return true when a valid layer request was emitted.
     */
    bool TriggerSelectedLayerForTest();

signals:
    /**
     * @brief Requests unified preview navigation to a report layer.
     * @param layerIndex Real package layer index.
     * @param gapPreviewPath Existing diagnostic-only gap preview path, or empty.
     */
    void SigLayerRequested(int layerIndex, const QString& gapPreviewPath);

private slots:
    void OnLocateSelectedLayer();
    void OnWorstLayerActivated(int row, int column);
    void OnWorstLayerSelectionChanged();

private:
    QString FindReportPath(const PackageSummary& package) const;
    void RebuildView();
    int SelectedWorstLayerRow() const;
    bool EmitSelectedLayer();

    MaterialClosureDiagnosticsSummary m_summary;
    QLabel* m_status{nullptr};
    QTextEdit* m_summaryView{nullptr};
    QTableWidget* m_worstLayers{nullptr};
    QPushButton* m_locateButton{nullptr};
};
