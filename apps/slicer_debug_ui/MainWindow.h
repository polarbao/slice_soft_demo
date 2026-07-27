#pragma once

#include "services/ConfigDocument.h"
#include "services/EffectiveConfigGenerator.h"
#include "services/PackageLoader.h"
#include "services/ModelPreflightController.h"
#include "services/ModelPreflightPresenter.h"
#include "services/ModelTopViewLoader.h"
#include "services/SceneModelRepository.h"
#include "services/ProcessRunner.h"
#include "services/ProductionPackageResultValidator.h"
#include "services/ProductionSliceRunSession.h"
#include "services/ReportLoader.h"
#include "services/ScenarioRegistry.h"
#include "services/SliceProgressProtocolParser.h"
#include "services/SlicePreflightCoordinator.h"
#include "services/ToolPaths.h"
#include "widgets/ChannelChartPanel.h"
#include "widgets/ConfigEditorPanel.h"
#include "widgets/DiagnosticsDock.h"
#include "widgets/LogPanel.h"
#include "widgets/MaterialProcessPanel.h"
#include "widgets/ModelPreflightPanel.h"
#include "widgets/ModelTopViewWidget.h"
#include "widgets/ModelTransformPanel.h"
#include "widgets/PreviewWorkspace.h"
#include "widgets/ReportPanel.h"
#include "widgets/SliceTimingPanel.h"

#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>

#include <optional>

class QComboBox;
class QCheckBox;
class QTabWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QString repo_root, QWidget* parent = nullptr);

private slots:
    void browseConfig();
    void browsePackage();
    void browseProfileA();
    void browseProfileB();
    void buildDebug();
    void runSlicer();
    void runRipSummary();
    void runQuickRegression();
    void compareProfiles();
    void openOutputFolder();
    void loadPackageFromEdit();
    void OnImportModelPreview();
    void OnImportModelAndSlice();
    void OnImportModelOpenVdbDiagnostic();
    void OnImportModelOpenVdbCandidate();
    void OnScenarioChanged(int index);
    void OnReloadScenarios();
    void OnScenarioVisibilityChanged(bool checked);
    void OnMaterialClosureLayerRequested(int layerIndex, const QString& gapPreviewPath);
    void OnProcessOutput(const QString& text);
    void OnModelPreflightStateChanged();
    void OnPreflightActionAdmitted();
    void OnPreflightActionBlocked();
    void OnLegacyPreflightConfirmationRequired();
    void OnRecheckModelPreflight();
    void OnCancelModelPreflight();
    void OnSceneDocumentChanged();
    void OnSaveSceneTransform();
    void handleProcessStarted(const QString& command);
    void handleProcessFinished(int exit_code, qint64 elapsed_ms);
    void handleProcessFailed(const QString& message);

private:
    QWidget* createProjectPanel();
    QWidget* createRunPanel();
    QWidget* createRightPanel();
    QString absoluteFromRepo(const QString& path) const;
    QString inferPackageFromConfig(const QString& config_path) const;
    QString CreateOneClickConfig(const QString& modelPath, QString* packageDir);
    QString CreateOpenVdbCandidateConfig(const QString& modelPath, QString* packageDir) const;
    QString CreateOpenVdbReportPath(const QString& modelPath) const;
    EffectiveConfigResult GenerateEffectiveConfig(
        const QString& modelPathOverride,
        const QString& packageDirOverride,
        SliceEngineRole engineRole,
        const QString& sessionTag);
    SliceSettingsState BuildCurrentSettings(
        const QString& modelPathOverride,
        const QString& packageDirOverride,
        SliceEngineRole engineRole) const;
    void ApplyProfileDefaultsToDocument(const QString& profileId, const QString& packageDir);
    void RunGeneratedConfig(const SlicePreflightAction& action);
    void RunOpenVdbDiagnostic(const QString& configPath, const QString& reportPath);
    void RunOpenVdbCandidate(const QString& configPath, const QString& packageDir);
    void RequestSlicePreflight(const SlicePreflightAction& action);
    void UpdateModelPreflightUi();
    void UpdateActionAvailability();
    void LoadScenarios();
    bool ShouldShowScenario(const ScenarioEntry& scenario) const;
    void ApplyScenario(const ScenarioEntry& scenario);
    void loadPackage(const QString& package_dir);
    void LoadPackageSummary(const PackageSummary& package);
    void loadCompareResult(const QString& path);
    void runCommand(const QString& action, const QString& program, const QStringList& args);
    void setBusy(bool busy);

    ToolPaths paths_;
    ConfigDocument config_document_;
    PackageLoader package_loader_;
    ReportLoader report_loader_;
    ScenarioRegistry m_scenarioRegistry;
    SliceProgressProtocolParser m_sliceProgressParser;
    ProcessRunner runner_;
    ProductionPackageResultValidator m_productionPackageResultValidator;
    ProductionSliceRunSession m_productionRunSession;
    ModelPreflightController m_modelPreflightController;
    SlicePreflightCoordinator m_slicePreflightCoordinator;
    SceneDocument m_sceneDocument;
    SceneSelectionModel m_sceneSelectionModel;
    SceneModelRepository m_sceneModelRepository;
    ModelTopViewLoader m_modelTopViewLoader;
    SceneTransformController m_sceneTransformController;
    QString current_action_;
    QString pending_package_;
    QString compare_output_;
    QString m_currentProfileId;

    QLineEdit* config_edit_{nullptr};
    QLineEdit* package_edit_{nullptr};
    QLineEdit* profile_a_edit_{nullptr};
    QLineEdit* profile_b_edit_{nullptr};
    QLabel* repo_label_{nullptr};
    QLabel* slicer_label_{nullptr};
    QLabel* m_openVdbSlicerLabel{nullptr};
    QLabel* rip_label_{nullptr};
    QLabel* status_label_{nullptr};
    QLabel* m_modelPreflightCompactState{nullptr};
    QLabel* m_modelPreflightCompactMode{nullptr};
    QPlainTextEdit* warnings_view_{nullptr};
    QPlainTextEdit* compare_view_{nullptr};
    QComboBox* m_scenarioSelector{nullptr};
    QCheckBox* m_showAdvancedScenariosCheck{nullptr};
    QLabel* m_scenarioCountLabel{nullptr};
    QLabel* m_scenarioDescriptionLabel{nullptr};
    QPushButton* build_button_{nullptr};
    QPushButton* run_slicer_button_{nullptr};
    QPushButton* run_rip_button_{nullptr};
    QPushButton* regression_button_{nullptr};
    QPushButton* compare_button_{nullptr};
    QPushButton* m_importModelPreviewButton{nullptr};
    QPushButton* m_importSliceButton{nullptr};
    QPushButton* m_importOpenVdbButton{nullptr};
    QPushButton* m_importOpenVdbCandidateButton{nullptr};
    QPushButton* m_modelPreflightRecheckButton{nullptr};
    QPushButton* m_modelPreflightCancelButton{nullptr};
    SliceTimingPanel* m_sliceTimingPanel{nullptr};

    PreviewWorkspace* m_previewWorkspace{nullptr};
    QTabWidget* m_mainWorkspaceTabs{nullptr};
    QWidget* m_modelTopViewWorkspace{nullptr};
    ModelTopViewWidget* m_modelTopViewWidget{nullptr};
    ModelTransformPanel* m_modelTransformPanel{nullptr};
    DiagnosticsDock* m_diagnosticsDock{nullptr};
    ReportPanel* report_panel_{nullptr};
    ConfigEditorPanel* config_editor_panel_{nullptr};
    ChannelChartPanel* channel_chart_panel_{nullptr};
    MaterialProcessPanel* material_process_panel_{nullptr};
    ModelPreflightPanel* m_modelPreflightPanel{nullptr};
    LogPanel* log_panel_{nullptr};
    bool m_processBusy{false};
    bool m_suppressPreflightStale{false};
    std::optional<SliceTimingEvent> m_lastSliceTimingEvent;
};
