#pragma once

#include "HostModelListPanel.h"
#include "HostModelImportWorkflow.h"
#include "HostProfileCatalog.h"
#include "HostProfilePanel.h"
#include "HostPackageReviewController.h"
#include "HostPackageReviewPanel.h"
#include "HostSliceJobController.h"
#include "HostSliceJobPanel.h"
#include "HostSliceSettingsPanel.h"
#include "HostTransformLayoutPanel.h"
#include "ModuleClient.h"

#include <QMainWindow>

#include <memory>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QSplitter;
class QTableWidget;
class QTabWidget;
class ViewPresentationSettings;
class ViewWorkspaceWidget;

/**
 * @brief Minimal Qt reference shell that consumes only the public module ABI.
 */
class HostMainWindow final : public QMainWindow
{
public:
    /**
     * @brief Creates the reference host and attempts to load the module.
     * @param modulePath Runtime path to slicer_module.dll.
     * @param parent Optional Qt parent widget.
     */
    explicit HostMainWindow(
        const QString& modulePath,
        QWidget* parent = nullptr);

    /** @brief Releases host UI and session settings resources. */
    ~HostMainWindow() override;

private:
    void BuildInterface();
    void LoadModule(const QString& modulePath);
    void ConfigureProfiles();
    void SaveViewSettings();
    void RestoreWorkspaceState();
    void SaveWorkspaceState();
    void OnImportModel();
    void OnRemoveModels(const QStringList& instanceIds);
    void OnModelSelectionChanged(const QStringList& instanceIds);
    void OnProfileChanged(const QString& profileId);
    bool ProfileSupportsSlice(const QString& profileId) const;
    void OnSliceSettingsChanged();
    void OnStartSlice();
    void OnCancelSlice();
    void OnSliceJobProgress(
        const QString& state,
        const QString& phase,
        int current,
        int total,
        int percent,
        qint64 elapsedMs);
    void OnSliceJobCompleted(
        bool success,
        bool cancelled,
        const QString& code,
        const QString& message,
        const QString& detail,
        const QString& packageDirectory,
        qint64 elapsedMs,
        qint64 cancelLatencyMs);
    void LoadSliceResult(const QString& packageDirectory);
    void OnResultLayerRequested(
        int layerIndex,
        const QStringList& channels);
    void OnResultReportRequested(const QString& reportName);
    bool ApplyPendingSceneContext(QString* error);
    void RefreshSliceSettings();
    void RefreshSliceJobReadiness();
    void OnTransformRequested(
        const QStringList& instanceIds,
        double deltaXMm,
        double deltaYMm,
        double deltaZMm,
        double rotateZDegrees,
        double uniformScaleFactor,
        bool mirrorX,
        bool mirrorY);
    void OnLayoutRequested(
        int maxColumns,
        int maxRows,
        double columnGapMm,
        double rowGapMm);
    void SetSceneCommandsEnabled(bool enabled);
    void SetWorkflowEditingEnabled(bool enabled);
    void ShowSceneEditResult(
        const QString& action,
        const hostsceneeditresult& result);
    void ShowSceneEditError(const QString& action, const QString& error);
    void ShowImportResult(const hostmodelimportresult& result);
    void ShowImportError(const QString& error);

    ModuleClient m_client;
    std::unique_ptr<HostModelImportWorkflow> m_importWorkflow;
    std::unique_ptr<HostSliceJobController> m_sliceJobController;
    std::unique_ptr<HostPackageReviewController> m_packageReviewController;
    std::unique_ptr<IHostProfileCatalog> m_profileCatalog;
    std::unique_ptr<ViewPresentationSettings> m_viewSettings;
    ViewWorkspaceWidget* m_workspace{nullptr};
    QSplitter* m_workspaceSplitter{nullptr};
    QTabWidget* m_workspaceTabs{nullptr};
    QTabWidget* m_inspectorTabs{nullptr};
    QComboBox* m_defaultViewCombo{nullptr};
    QComboBox* m_projectionCombo{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_pathLabel{nullptr};
    HostModelListPanel* m_modelListPanel{nullptr};
    HostProfilePanel* m_profilePanel{nullptr};
    HostSliceSettingsPanel* m_sliceSettingsPanel{nullptr};
    HostSliceJobPanel* m_sliceJobPanel{nullptr};
    HostPackageReviewPanel* m_packageReviewPanel{nullptr};
    HostTransformLayoutPanel* m_transformLayoutPanel{nullptr};
    QLabel* m_importSummaryLabel{nullptr};
    QTableWidget* m_preflightTable{nullptr};
    QPlainTextEdit* m_moduleInfoView{nullptr};
    QString m_selectedProfileId;
    QString m_restoredProfileId;
};
