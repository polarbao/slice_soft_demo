#pragma once

#include "services/PackageLoader.h"
#include "services/ProcessRunner.h"
#include "services/ReportLoader.h"
#include "services/ToolPaths.h"
#include "widgets/LogPanel.h"
#include "widgets/MaterialProcessPanel.h"
#include "widgets/PreviewPanel.h"
#include "widgets/ReportPanel.h"

#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>

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
    void handleProcessStarted(const QString& command);
    void handleProcessFinished(int exit_code, qint64 elapsed_ms);
    void handleProcessFailed(const QString& message);

private:
    QWidget* createProjectPanel();
    QWidget* createRunPanel();
    QWidget* createRightPanel();
    QString absoluteFromRepo(const QString& path) const;
    QString inferPackageFromConfig(const QString& config_path) const;
    void loadPackage(const QString& package_dir);
    void loadCompareResult(const QString& path);
    void runCommand(const QString& action, const QString& program, const QStringList& args);
    void setBusy(bool busy);

    ToolPaths paths_;
    PackageLoader package_loader_;
    ReportLoader report_loader_;
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
    QLabel* rip_label_{nullptr};
    QLabel* status_label_{nullptr};
    QPlainTextEdit* warnings_view_{nullptr};
    QPlainTextEdit* compare_view_{nullptr};
    QPushButton* build_button_{nullptr};
    QPushButton* run_slicer_button_{nullptr};
    QPushButton* run_rip_button_{nullptr};
    QPushButton* regression_button_{nullptr};
    QPushButton* compare_button_{nullptr};

    PreviewPanel* preview_panel_{nullptr};
    ReportPanel* report_panel_{nullptr};
    MaterialProcessPanel* material_process_panel_{nullptr};
    LogPanel* log_panel_{nullptr};
};

