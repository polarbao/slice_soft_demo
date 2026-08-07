#include "UiSmokeTestInternal.h"

using ui_smoke_test_support::BuildExperimentalReportFixture;
using ui_smoke_test_support::BuildMaterialClosureReportFixture;
using ui_smoke_test_support::BuildOpenVdbUtilityReportFixture;
using ui_smoke_test_support::ClosedBoxObjFixture;
using ui_smoke_test_support::ContainsAll;
using ui_smoke_test_support::GlobalRect;
using ui_smoke_test_support::OpenTriangleObjFixture;
using ui_smoke_test_support::ReadJsonObject;
using ui_smoke_test_support::WaitForCondition;
using ui_smoke_test_support::WriteJsonFixture;
using ui_smoke_test_support::WritePreflightFixture;

int UiSmokeTestRunner::startup(const UiSmokeTestOptions& options) {
    MainWindow window(options.repo_root);
    Q_UNUSED(window);
    return pass("startup");
}

int UiSmokeTestRunner::WorkbenchJobActionBar(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents, 50);

    QWidget* actionBar = window.findChild<QWidget*>(
        QStringLiteral("sceneActionBar"));
    QWidget* projectPanel = window.findChild<QWidget*>(
        QStringLiteral("projectPanel"));
    QSplitter* mainSplitter = window.findChild<QSplitter*>(
        QStringLiteral("mainSplitter"));
    QPushButton* importButton =
        window.findChild<QPushButton*>(
            QStringLiteral("jobImportModelsButton"));
    QPushButton* saveButton =
        window.findChild<QPushButton*>(
            QStringLiteral("jobSaveSceneButton"));
    QPushButton* sliceButton =
        window.findChild<QPushButton*>(
            QStringLiteral("sliceCurrentSceneButton"));
    QPushButton* cancelButton =
        window.findChild<QPushButton*>(
            QStringLiteral("cancelCurrentSceneSliceButton"));
    QLabel* modeLabel = window.findChild<QLabel*>(
        QStringLiteral("jobModeSummaryLabel"));
    QComboBox* profileSelector =
        window.findChild<QComboBox*>(
            QStringLiteral("jobProfileSelector"));
    QLabel* statusLabel = window.findChild<QLabel*>(
        QStringLiteral("sceneSliceActionStateLabel"));
    if (actionBar == nullptr
        || projectPanel == nullptr
        || mainSplitter == nullptr
        || importButton == nullptr
        || saveButton == nullptr
        || sliceButton == nullptr
        || cancelButton == nullptr
        || modeLabel == nullptr
        || profileSelector == nullptr
        || statusLabel == nullptr)
    {
        return fail(QStringLiteral(
            "13D-01 top job action controls missing"));
    }
    if (actionBar->parentWidget() != window.centralWidget()
        || projectPanel->isAncestorOf(actionBar)
        || actionBar->geometry().bottom()
            > mainSplitter->geometry().top()
        || !actionBar->isVisibleTo(&window))
    {
        return fail(QStringLiteral(
            "13D-01 action bar is not fixed above main workspace"));
    }
    if (!importButton->isEnabled()
        || saveButton->isEnabled()
        || sliceButton->isEnabled()
        || cancelButton->isEnabled()
        || !modeLabel->text().contains(
            QStringLiteral("传统切片"))
        || profileSelector->findData(
               QStringLiteral(
                   "textured_nail_rgb_white_lower_support"))
               < 0
        || profileSelector->findData(
               QStringLiteral(
                   "textured_nail_rgb_only_lower_support"))
               < 0
        || profileSelector->currentData().toString()
            != QStringLiteral(
                "textured_nail_rgb_only_lower_support")
        || statusLabel->text().trimmed().isEmpty())
    {
        return fail(QStringLiteral(
            "13D-01 empty-scene action state mismatch"));
    }

    const int rgbOnlyIndex =
        profileSelector->findData(
            QStringLiteral(
                "textured_nail_rgb_only_lower_support"));
    profileSelector->setCurrentIndex(rgbOnlyIndex);
    QMetaObject::invokeMethod(
        profileSelector,
        "activated",
        Qt::DirectConnection,
        Q_ARG(int, rgbOnlyIndex));
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (window.m_currentProfileId
            != QStringLiteral(
                "textured_nail_rgb_only_lower_support")
        || window.config_document_
               .value(
                   {QStringLiteral("modelFill"),
                    QStringLiteral("material")})
               .toString()
            != QStringLiteral("rgb")
        || window.config_document_
               .value(
                   {QStringLiteral("materialProcessProfile"),
                    QStringLiteral("white"),
                    QStringLiteral("enabled")})
               .toBool(true))
    {
        return fail(QStringLiteral(
            "13D-R1 top Profile selector did not apply RGB-only production settings"));
    }

    for (int index = 0;
         index < window.m_mainWorkspaceTabs->count();
         ++index)
    {
        window.m_mainWorkspaceTabs->setCurrentIndex(index);
        QApplication::processEvents(
            QEventLoop::AllEvents,
            20);
        if (!actionBar->isVisibleTo(&window))
        {
            return fail(QStringLiteral(
                "13D-01 action bar disappeared after workspace switch"));
        }
    }

    const QString configPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    if (!window.config_editor_panel_->loadConfig(configPath))
    {
        return fail(QStringLiteral(
            "13D-01 fixture configuration unavailable"));
    }
    SceneBatchImportRequest request;
    request.batchid = QStringLiteral("13d-job-action-bar");
    request.configpath = configPath;
    request.files = QStringList{modelPath};
    request.autolayout = true;
    if (!window.m_sceneBatchImportController
             .Start(request)
             .IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.m_sceneBatchImportController
                            .IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "13D-01 one-model scene import did not complete"));
    }
    window.UpdateActionAvailability();
    if (!saveButton->isEnabled()
        || !sliceButton->isEnabled()
        || cancelButton->isEnabled()
        || !statusLabel->text().contains(
            QStringLiteral("可见 1")))
    {
        return fail(QStringLiteral(
            "13D-01 admitted-scene action state mismatch"));
    }

    return pass(QStringLiteral(
        "workbench-job-action-bar fixed/import/save/"
        "mode-profile/slice/cancel/state"));
}

int UiSmokeTestRunner::WorkbenchContextInspector(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    ContextInspector* inspector =
        window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"));
    QTabWidget* inspectorTabs =
        window.findChild<QTabWidget*>(
            QStringLiteral("contextInspectorTabs"));
    QSplitter* mainSplitter =
        window.findChild<QSplitter*>(
            QStringLiteral("mainSplitter"));
    QPushButton* openConfigButton =
        window.findChild<QPushButton*>(
            QStringLiteral(
                "contextInspectorOpenConfigButton"));
    QWidget* oldModelSideTabs =
        window.findChild<QWidget*>(
            QStringLiteral("modelSceneSideTabs"));
    if (inspector == nullptr
        || inspectorTabs == nullptr
        || mainSplitter == nullptr
        || openConfigButton == nullptr
        || oldModelSideTabs != nullptr
        || mainSplitter->count() != 2
        || mainSplitter->widget(1) != inspector)
    {
        return fail(QStringLiteral(
            "13D-02 single context inspector shell mismatch"));
    }
    const QStringList expectedPages{
        QStringLiteral("场景"),
        QStringLiteral("变换"),
        QStringLiteral("排版"),
        QStringLiteral("切片设置"),
        QStringLiteral("预检与诊断"),
    };
    if (inspector->PageTitles() != expectedPages
        || !inspector->isAncestorOf(
            window.m_modelListPanel)
        || !inspector->isAncestorOf(
            window.m_modelTransformPanel)
        || !inspector->isAncestorOf(
            window.m_sceneLayoutPanel)
        || !inspector->isAncestorOf(
            window.m_modelPreflightPanel)
        || window.m_modelTopViewWorkspace
               ->isAncestorOf(window.m_modelListPanel))
    {
        return fail(QStringLiteral(
            "13D-02 context pages were duplicated or misplaced"));
    }

    const QString configPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    if (!window.config_editor_panel_->loadConfig(configPath))
    {
        return fail(QStringLiteral(
            "13D-02 fixture configuration unavailable"));
    }
    SceneBatchImportRequest request;
    request.batchid =
        QStringLiteral("13d-context-inspector");
    request.configpath = configPath;
    request.files = QStringList{modelPath};
    request.autolayout = true;
    if (!window.m_sceneBatchImportController
             .Start(request)
             .IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.m_sceneBatchImportController
                            .IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "13D-02 fixture model import did not complete"));
    }
    const quint64 sceneRevision =
        window.m_sceneDocument.SceneRevision();
    const QString selectedInstance =
        window.m_sceneSelectionModel.SelectedInstance();
    if (selectedInstance.isEmpty())
    {
        return fail(QStringLiteral(
            "13D-02 imported instance was not selected"));
    }
    for (int index = 0;
         index < inspectorTabs->count();
         ++index)
    {
        inspectorTabs->setCurrentIndex(index);
        QApplication::processEvents(
            QEventLoop::AllEvents,
            20);
        if (window.m_sceneDocument.SceneRevision()
                != sceneRevision
            || window.m_sceneSelectionModel
                   .SelectedInstance()
                != selectedInstance)
        {
            return fail(QStringLiteral(
                "13D-02 page switch changed scene identity"));
        }
    }

    openConfigButton->click();
    if (window.m_mainWorkspaceTabs->currentWidget()
        != window.m_configWorkspace)
    {
        return fail(QStringLiteral(
            "13D-02 slice settings did not open central config"));
    }
    return pass(QStringLiteral(
        "workbench-context-inspector single-shell/"
        "five-contexts/identity/config-navigation"));
}
