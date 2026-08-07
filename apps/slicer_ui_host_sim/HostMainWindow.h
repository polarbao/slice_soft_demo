#pragma once

#include "HostModelListPanel.h"
#include "HostModelImportWorkflow.h"
#include "HostTransformLayoutPanel.h"
#include "ModuleClient.h"

#include <QMainWindow>

#include <memory>

class QComboBox;
class QLabel;
class QPlainTextEdit;
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
    void SaveViewSettings();
    void OnImportModel();
    void OnRemoveModels(const QStringList& instanceIds);
    void OnModelSelectionChanged(const QStringList& instanceIds);
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
    void ShowSceneEditResult(
        const QString& action,
        const hostsceneeditresult& result);
    void ShowSceneEditError(const QString& action, const QString& error);
    void ShowImportResult(const hostmodelimportresult& result);
    void ShowImportError(const QString& error);

    ModuleClient m_client;
    std::unique_ptr<HostModelImportWorkflow> m_importWorkflow;
    std::unique_ptr<ViewPresentationSettings> m_viewSettings;
    ViewWorkspaceWidget* m_workspace{nullptr};
    QComboBox* m_defaultViewCombo{nullptr};
    QComboBox* m_projectionCombo{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLabel* m_pathLabel{nullptr};
    HostModelListPanel* m_modelListPanel{nullptr};
    HostTransformLayoutPanel* m_transformLayoutPanel{nullptr};
    QLabel* m_importSummaryLabel{nullptr};
    QTableWidget* m_preflightTable{nullptr};
    QPlainTextEdit* m_moduleInfoView{nullptr};
};
