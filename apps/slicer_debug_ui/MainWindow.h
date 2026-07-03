#pragma once

#include "services/ConfigDocument.h"
#include "services/PackageLoader.h"
#include "services/ProcessRunner.h"
#include "services/ReportLoader.h"
#include "services/ScenarioRegistry.h"
#include "services/ToolPaths.h"
#include "widgets/ChannelChartPanel.h"
#include "widgets/ConfigEditorPanel.h"
#include "widgets/LayerPreviewPanel.h"
#include "widgets/LogPanel.h"
#include "widgets/MaterialProcessPanel.h"
#include "widgets/PreviewOverlayPanel.h"
#include "widgets/PreviewPanel.h"
#include "widgets/ReportPanel.h"

#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>

class QComboBox;

class MainWindow final : public QMainWindow {
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
    void OnImportModelAndSlice();
    void OnImportModelOpenVdbDiagnostic();
    void OnImportModelOpenVdbCandidate();
    void OnScenarioChanged(int index);
    void OnReloadScenarios();
    void handleProcessStarted(const QString& command);
    void handleProcessFinished(int exit_code, qint64 elapsed_ms);
    void handleProcessFailed(const QString& message);

private:
    QWidget* createProjectPanel();
    QWidget* createRunPanel();
    QWidget* createRightPanel();
    QString absoluteFromRepo(const QString& path) const;
    QString inferPackageFromConfig(const QString& config_path) const;
    QString CreateOneClickConfig(const QString& modelPath, QString* packageDir) const;
    QString CreateOpenVdbCandidateConfig(const QString& modelPath, QString* packageDir) const;
    QString CreateOpenVdbReportPath(const QString& modelPath) const;
    void RunGeneratedConfig(const QString& configPath, const QString& packageDir);
    void RunOpenVdbDiagnostic(const QString& configPath, const QString& reportPath);
    void RunOpenVdbCandidate(const QString& configPath, const QString& packageDir);
    void LoadScenarios();
    void ApplyScenario(const ScenarioEntry& scenario);
    void loadPackage(const QString& package_dir);
    void loadCompareResult(const QString& path);
    void runCommand(const QString& action, const QString& program, const QStringList& args);
    void setBusy(bool busy);

    ToolPaths paths_;
    ConfigDocument config_document_;
    PackageLoader package_loader_;
    ReportLoader report_loader_;
    ScenarioRegistry m_scenarioRegistry;
    ProcessRunner runner_;
    QString current_action_;
    QString pending_package_;
    QString compare_output_;

    QLineEdit* config_edit_{nullptr};
    QLineEdit* package_edit_{nullptr};
    QLineEdit* profile_a_edit_{nullptr};
    QLineEdit* profile_b_edit_{nullptr};
    QLabel* repo_label_{nullptr};
    QLabel* slicer_label_{nullptr};
    QLabel* m_openVdbSlicerLabel{nullptr};
    QLabel* rip_label_{nullptr};
    QLabel* status_label_{nullptr};
    QPlainTextEdit* warnings_view_{nullptr};
    QPlainTextEdit* compare_view_{nullptr};
    QComboBox* m_scenarioSelector{nullptr};
    QLabel* m_scenarioDescriptionLabel{nullptr};
    QPushButton* build_button_{nullptr};
    QPushButton* run_slicer_button_{nullptr};
    QPushButton* run_rip_button_{nullptr};
    QPushButton* regression_button_{nullptr};
    QPushButton* compare_button_{nullptr};
    QPushButton* m_importSliceButton{nullptr};
    QPushButton* m_importOpenVdbButton{nullptr};
    QPushButton* m_importOpenVdbCandidateButton{nullptr};

    PreviewPanel* preview_panel_{nullptr};
    LayerPreviewPanel* m_layerPreviewPanel{nullptr};
    ReportPanel* report_panel_{nullptr};
    ConfigEditorPanel* config_editor_panel_{nullptr};
    ChannelChartPanel* channel_chart_panel_{nullptr};
    PreviewOverlayPanel* preview_overlay_panel_{nullptr};
    MaterialProcessPanel* material_process_panel_{nullptr};
    LogPanel* log_panel_{nullptr};
};
