#include "CapabilityCoverageRunner.h"
#include "HostMainWindow.h"
#include "HostProcessPresetCatalog.h"
#include "HostRipJobController.h"
#include "HostWorkspaceState.h"
#include "ModuleClient.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QVariant>

#include <cmath>

namespace
{
bool HasArgument(const QStringList& arguments, const QString& name)
{
    return arguments.contains(name);
}

QString FindArgumentValue(
    const QStringList& arguments,
    const QString& name)
{
    const int index = arguments.indexOf(name);
    if (index < 0 || index + 1 >= arguments.size())
    {
        return {};
    }
    return arguments.at(index + 1);
}

QString DefaultModulePath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("slicer_module.dll"));
}

int RunSelfTest(const QString& modulePath)
{
    ModuleClient client;
    QString error;
    if (!client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        QTextStream(stderr)
            << "MODULE_LOAD_FAILED: " << error << Qt::endl;
        return 3;
    }

    QByteArray report;
    if (!client.SelfTest(&report, &error))
    {
        QTextStream(stderr)
            << "MODULE_SELF_TEST_FAILED: " << error << Qt::endl;
        return 4;
    }

    QTextStream(stdout)
        << "STAGE14E02_SELF_TEST_PASS spi=" << PM_SPI_VERSION
        << " calls=" << client.CallCount() << Qt::endl;
    return 0;
}

int RunCapabilityCoverage(
    const QString& modulePath,
    const QString& repositoryRoot,
    const QString& evidenceRoot)
{
    if (repositoryRoot.isEmpty() || evidenceRoot.isEmpty())
    {
        QTextStream(stderr)
            << "CAPABILITY_COVERAGE_ARGUMENT_FAILED: "
            << "--repo-root and --evidence-root are required"
            << Qt::endl;
        return 5;
    }

    ModuleClient client;
    QString error;
    if (!client.Open(modulePath, QByteArrayLiteral("{}"), &error))
    {
        QTextStream(stderr)
            << "CAPABILITY_COVERAGE_MODULE_FAILED: " << error << Qt::endl;
        return 6;
    }

    CapabilityCoverageRunner runner(client);
    QByteArray report;
    if (!runner.Run(repositoryRoot, evidenceRoot, &report, &error))
    {
        QTextStream(stderr)
            << "CAPABILITY_COVERAGE_FAILED: " << error << Qt::endl;
        return 7;
    }

    QTextStream(stdout)
        << "STAGE14E04B_CAPABILITY_COVERAGE_PASS calls="
        << client.CallCount() << Qt::endl;
    return 0;
}

int RunHostFlowImportUiSmoke(const QString& modulePath)
{
    HostMainWindow window(modulePath);
    const QObject* importButton = window.findChild<QObject*>(
        QStringLiteral("hostImportModelButton"));
    const QObject* modelList = window.findChild<QObject*>(
        QStringLiteral("hostImportedModelList"));
    const QObject* preflightTable = window.findChild<QObject*>(
        QStringLiteral("hostImportPreflightTable"));
    if (importButton == nullptr || modelList == nullptr
        || preflightTable == nullptr
        || !importButton->property("enabled").toBool())
    {
        QTextStream(stderr)
            << "HOSTFLOW_HB01_UI_FAILED: import panel is incomplete"
            << Qt::endl;
        return 8;
    }
    QTextStream(stdout)
        << "HOSTFLOW_HB01_UI_PASS" << Qt::endl;
    return 0;
}

int RunHostFlowProfileUiSmoke(const QString& modulePath)
{
    HostMainWindow window(modulePath);
    const auto* profileCombo = window.findChild<QComboBox*>(
        QStringLiteral("hostProfileCombo"));
    const auto* safetyLabel = window.findChild<QLabel*>(
        QStringLiteral("hostProfileSafetyLabel"));
    const auto* availabilityLabel = window.findChild<QLabel*>(
        QStringLiteral("hostProfileAvailabilityLabel"));
    if (profileCombo == nullptr || safetyLabel == nullptr
        || availabilityLabel == nullptr || !profileCombo->isEnabled()
        || profileCombo->count() < 3
        || profileCombo->currentData().toString().isEmpty()
        || safetyLabel->text().isEmpty()
        || availabilityLabel->text().isEmpty())
    {
        QTextStream(stderr)
            << "HOSTFLOW_HB04_UI_FAILED: Profile panel is incomplete"
            << Qt::endl;
        return 9;
    }
    QTextStream(stdout)
        << "HOSTFLOW_HB04_UI_PASS profiles=" << profileCombo->count()
        << Qt::endl;
    return 0;
}

int RunHostFlowSettingsUiSmoke(const QString& modulePath)
{
    HostMainWindow window(modulePath);
    const auto* dpiXSpin = window.findChild<QSpinBox*>(
        QStringLiteral("hostSliceDpiXSpin"));
    const auto* dpiYSpin = window.findChild<QSpinBox*>(
        QStringLiteral("hostSliceDpiYSpin"));
    const auto* layerSpin = window.findChild<QDoubleSpinBox*>(
        QStringLiteral("hostSliceLayerThicknessSpin"));
    const auto* profilePreview = window.findChild<QPlainTextEdit*>(
        QStringLiteral("hostEffectiveProfilePreview"));
    const auto* validationLabel = window.findChild<QLabel*>(
        QStringLiteral("hostSliceValidationLabel"));
    const auto* whitePreflightLabel = window.findChild<QLabel*>(
        QStringLiteral("hostTextureWhitePreflightLabel"));
    const auto* processPreset = window.findChild<QComboBox*>(
        QStringLiteral("hostProcessPresetCombo"));
    const auto* tiffCompressionCheck = window.findChild<QCheckBox*>(
        QStringLiteral("hostTiffCompressionCheck"));
    const auto* tiffCompressionCombo = window.findChild<QComboBox*>(
        QStringLiteral("hostTiffCompressionCombo"));
    if (dpiXSpin == nullptr || dpiYSpin == nullptr || layerSpin == nullptr
        || profilePreview == nullptr || validationLabel == nullptr
        || whitePreflightLabel == nullptr || processPreset == nullptr
        || tiffCompressionCheck == nullptr
        || tiffCompressionCombo == nullptr
        || dpiXSpin->value() != 635 || dpiYSpin->value() != 600
        || std::abs(layerSpin->value() - 0.038) > 1.0e-9
        || processPreset->currentData().toString()
            != HostProcessPresetCatalog::DefaultPresetId()
        || tiffCompressionCheck->isChecked()
        || tiffCompressionCombo->isEnabled()
        || validationLabel->text().isEmpty()
        || whitePreflightLabel->text().isEmpty())
    {
        QTextStream(stderr)
            << "HOSTFLOW_HB05_UI_FAILED: slice settings are incomplete"
            << Qt::endl;
        return 10;
    }
    QTextStream(stdout)
        << "HOSTFLOW_HB05_UI_PASS dpi=" << dpiXSpin->value()
        << "x" << dpiYSpin->value() << Qt::endl;
    return 0;
}

int RunHostFlowJobUiSmoke(const QString& modulePath)
{
    HostMainWindow window(modulePath);
    const auto* startButton = window.findChild<QPushButton*>(
        QStringLiteral("hostSliceStartButton"));
    const auto* cancelButton = window.findChild<QPushButton*>(
        QStringLiteral("hostSliceCancelButton"));
    const auto* progressBar = window.findChild<QProgressBar*>(
        QStringLiteral("hostSliceJobProgressBar"));
    const auto* statusLabel = window.findChild<QLabel*>(
        QStringLiteral("hostSliceJobStatusLabel"));
    auto* jobPanel = window.findChild<HostSliceJobPanel*>(
        QStringLiteral("hostSliceJobPanel"));
    const auto* detailView = window.findChild<QPlainTextEdit*>(
        QStringLiteral("hostSliceJobDetailView"));
    const auto* modelLoadValue = window.findChild<QLabel*>(
        QStringLiteral("hostSliceTimingModelLoadValue"));
    const auto* tiffWriteValue = window.findChild<QLabel*>(
        QStringLiteral("hostSliceTimingTiffWriteValue"));
    const auto* workerTotalValue = window.findChild<QLabel*>(
        QStringLiteral("hostSliceTimingWorkerTotalValue"));
    const auto* hostTotalValue = window.findChild<QLabel*>(
        QStringLiteral("hostSliceTimingHostTotalValue"));
    if (startButton == nullptr || cancelButton == nullptr
        || progressBar == nullptr || statusLabel == nullptr
        || jobPanel == nullptr || detailView == nullptr
        || modelLoadValue == nullptr || tiffWriteValue == nullptr
        || workerTotalValue == nullptr || hostTotalValue == nullptr
        || startButton->isEnabled() || cancelButton->isEnabled()
        || progressBar->minimum() != 0 || progressBar->maximum() != 100
        || statusLabel->text().isEmpty())
    {
        QTextStream(stderr)
            << "HOSTFLOW_HB06_UI_FAILED: job panel is incomplete"
            << Qt::endl;
        return 11;
    }
    jobPanel->ShowCompletion(
        false,
        false,
        QStringLiteral("HOSTFLOW-TEST-ERROR"),
        QStringLiteral("测试错误说明"),
        QStringLiteral("测试详细信息"),
        QString{},
        QJsonObject{
            {QStringLiteral("available"), true},
            {QStringLiteral("engine"), QStringLiteral("legacy-scene-v1")},
            {QStringLiteral("modelLoadMs"), 12.5},
            {QStringLiteral("tiffWriteMs"), 8.0},
            {QStringLiteral("totalMs"), 20.5}},
        25,
        -1);
    if (!detailView->toPlainText().contains(
            QStringLiteral("错误码：HOSTFLOW-TEST-ERROR"))
        || !detailView->toPlainText().contains(
            QStringLiteral("错误说明：测试错误说明"))
        || !detailView->toPlainText().contains(
            QStringLiteral("详细信息：测试详细信息"))
        || modelLoadValue->text() != QStringLiteral("12.5 ms")
        || tiffWriteValue->text() != QStringLiteral("8.0 ms")
        || workerTotalValue->text() != QStringLiteral("20.5 ms")
        || hostTotalValue->text() != QStringLiteral("25.0 ms"))
    {
        QTextStream(stderr)
            << "HOSTFLOW_HB06_UI_FAILED: failure detail is incomplete"
            << Qt::endl;
        return 11;
    }
    jobPanel->SetReady(true, QStringLiteral("下一次切片已就绪"));
    if (!startButton->isEnabled() || cancelButton->isEnabled())
    {
        QTextStream(stderr)
            << "HOSTFLOW_HB06_UI_FAILED: completion state cannot re-arm"
            << Qt::endl;
        return 11;
    }
    jobPanel->SetActive();
    jobPanel->UpdateProgress(
        QStringLiteral("running"),
        QStringLiteral("scene_instance_slice"),
        1,
        3,
        33,
        17);
    jobPanel->ShowCompletion(
        false,
        false,
        QStringLiteral("HOSTFLOW-TEST-NO-TIMING"),
        QStringLiteral("Worker 未返回细分耗时"),
        QString{},
        QString{},
        QJsonObject{},
        22,
        -1);
    if (modelLoadValue->text() != QStringLiteral("未提供")
        || workerTotalValue->text() != QStringLiteral("未提供")
        || hostTotalValue->text() != QStringLiteral("22.0 ms"))
    {
        QTextStream(stderr)
            << "HOSTFLOW_HB06_UI_FAILED: timing fallback is incorrect"
            << Qt::endl;
        return 11;
    }
    QTextStream(stdout)
        << "HOSTFLOW_HB06_UI_PASS" << Qt::endl;
    return 0;
}

int RunHostFlowResultUiSmoke(const QString& modulePath)
{
    HostMainWindow window(modulePath);
    const auto* panel = window.findChild<QObject*>(
        QStringLiteral("hostPackageReviewPanel"));
    const auto* layerSlider = window.findChild<QSlider*>(
        QStringLiteral("hostPackageLayerSlider"));
    const auto* previewMode = window.findChild<QComboBox*>(
        QStringLiteral("hostPackagePreviewModeCombo"));
    const auto* reportCombo = window.findChild<QComboBox*>(
        QStringLiteral("hostPackageReportCombo"));
    const auto* chart = window.findChild<QObject*>(
        QStringLiteral("hostPackageChannelChart"));
    const auto* openPackageButton = window.findChild<QPushButton*>(
        QStringLiteral("hostOpenPackageDirectoryButton"));
    const auto* previewImage = window.findChild<QLabel*>(
        QStringLiteral("hostPackagePreviewImage"));
    const auto* referenceCaption = window.findChild<QLabel*>(
        QStringLiteral("hostPackageReferencePreviewCaption"));
    const auto* currentCaption = window.findChild<QLabel*>(
        QStringLiteral("hostPackageCurrentPreviewCaption"));
    if (panel == nullptr || layerSlider == nullptr || previewMode == nullptr
        || reportCombo == nullptr || chart == nullptr
        || openPackageButton == nullptr
        || previewImage == nullptr
        || referenceCaption != nullptr || currentCaption != nullptr
        || layerSlider->isEnabled() || previewMode->count() < 7
        || previewMode->currentData().toStringList()
            != QStringList({
                QStringLiteral("R"),
                QStringLiteral("G"),
                QStringLiteral("B"),
                QStringLiteral("W"),
                QStringLiteral("S"),
                QStringLiteral("V")})
        || reportCombo->count() < 3 || openPackageButton->isEnabled())
    {
        QTextStream(stderr)
            << "HOSTFLOW_HB07_UI_FAILED: result workspace is incomplete"
            << Qt::endl;
        return 12;
    }
    QTextStream(stdout) << "HOSTFLOW_HB07_UI_PASS" << Qt::endl;
    return 0;
}

int RunHostFlowWorkspaceUiSmoke(const QString& modulePath)
{
    HostMainWindow window(modulePath);
    const auto* workspaceTabs = window.findChild<QTabWidget*>(
        QStringLiteral("hostWorkspaceTabs"));
    const auto* inspectorTabs = window.findChild<QTabWidget*>(
        QStringLiteral("hostSceneInspectorTabs"));
    const auto* splitter = window.findChild<QSplitter*>(
        QStringLiteral("workspaceSplitter"));
    if (workspaceTabs == nullptr || inspectorTabs == nullptr
        || splitter == nullptr || workspaceTabs->count() < 4
        || inspectorTabs->count() < 5 || splitter->count() != 2
        || HostWorkspaceState::SchemaVersion() != 6)
    {
        QTextStream(stderr)
            << "HOSTFLOW_HB08_UI_FAILED: workspace state is incomplete"
            << Qt::endl;
        return 13;
    }
    QTextStream(stdout)
        << "HOSTFLOW_HB08_UI_PASS tabs=" << workspaceTabs->count()
        << "/" << inspectorTabs->count() << Qt::endl;
    return 0;
}

int RunRipModuleSelfTest(const QString& moduleDirectory)
{
    HostRipJobController controller;
    QString error;
    if (!controller.CheckRuntime(moduleDirectory, &error))
    {
        QTextStream(stderr)
            << "RIPFLOW_MODULE_SELF_TEST_FAILED: " << error << Qt::endl;
        return 14;
    }
    QTextStream(stdout)
        << "RIPFLOW_MODULE_SELF_TEST_PASS path=" << moduleDirectory
        << Qt::endl;
    return 0;
}

int RunRipUiSmoke(const QString& modulePath)
{
    HostMainWindow window(modulePath);
    const auto* autoCheck = window.findChild<QCheckBox*>(
        QStringLiteral("hostRipAutoAfterSliceCheck"));
    const auto* intentCombo = window.findChild<QComboBox*>(
        QStringLiteral("hostRipIntentCombo"));
    const auto* transparentCombo = window.findChild<QComboBox*>(
        QStringLiteral("hostRipTransparentModeCombo"));
    const auto* colorModeCombo = window.findChild<QComboBox*>(
        QStringLiteral("hostRipColorModeCombo"));
    const auto* runButton = window.findChild<QPushButton*>(
        QStringLiteral("hostRipRunButton"));
    const auto* cancelButton = window.findChild<QPushButton*>(
        QStringLiteral("hostRipCancelButton"));
    const auto* runtimeStatus = window.findChild<QLabel*>(
        QStringLiteral("hostRipRuntimeStatus"));
    if (autoCheck == nullptr || intentCombo == nullptr
        || transparentCombo == nullptr || colorModeCombo == nullptr
        || runButton == nullptr || cancelButton == nullptr
        || runtimeStatus == nullptr || autoCheck->isChecked()
        || intentCombo->count() != 4 || transparentCombo->count() != 3
        || colorModeCombo->count() != 1 || runButton->isEnabled()
        || cancelButton->isEnabled() || runtimeStatus->text().isEmpty())
    {
        QTextStream(stderr)
            << "RIPFLOW_UI_SELF_TEST_FAILED: RIP settings page is incomplete"
            << Qt::endl;
        return 15;
    }
    QTextStream(stdout) << "RIPFLOW_UI_SELF_TEST_PASS" << Qt::endl;
    return 0;
}

int RunRipJobSelfTest(
    const QString& packageDirectory,
    const QString& moduleDirectory,
    const QString& transparentMode,
    const int grayBits,
    const int timeoutSeconds,
    const int cancelAfterMs,
    const QString& expectedOutcome)
{
    if (packageDirectory.isEmpty() || moduleDirectory.isEmpty())
    {
        QTextStream(stderr)
            << "RIPFLOW_JOB_SELF_TEST_ARGUMENT_FAILED: "
            << "--package and --rip-module are required" << Qt::endl;
        return 16;
    }
    hostripsettings settings = HostRipSettingsStore::Defaults();
    settings.transparentmode = transparentMode;
    settings.devicegraybits = grayBits;
    settings.timeoutseconds = timeoutSeconds;
    HostRipJobController controller;
    QEventLoop loop;
    int result = 17;
    QObject::connect(
        &controller,
        &HostRipJobController::SigCompleted,
        &loop,
        [&](const bool success,
            const bool cancelled,
            const QString& code,
            const QString& message,
            const QString& outputDirectory,
            const qint64 elapsedMs)
        {
            const bool expectedSuccess = expectedOutcome
                    == QStringLiteral("success")
                && success && !cancelled;
            const bool expectedCancel = expectedOutcome
                    == QStringLiteral("cancel")
                && !success && cancelled
                && code == QStringLiteral("RIP_CANCELLED");
            const bool expectedTimeout = expectedOutcome
                    == QStringLiteral("timeout")
                && !success && cancelled
                && code == QStringLiteral("RIP_TIMEOUT");
            const bool expectedFailure = expectedOutcome
                    == QStringLiteral("failure")
                && !success && !cancelled
                && code == QStringLiteral("RIP_PROCESS_EXIT_FAILED");
            if (expectedSuccess || expectedCancel || expectedTimeout
                || expectedFailure)
            {
                QTextStream(stdout)
                    << "RIPFLOW_JOB_SELF_TEST_PASS outcome="
                    << expectedOutcome << " code=" << code
                    << " elapsedMs=" << elapsedMs
                    << " output=" << outputDirectory << Qt::endl;
                result = 0;
            }
            else
            {
                QTextStream(stderr)
                    << "RIPFLOW_JOB_SELF_TEST_FAILED code=" << code
                    << " cancelled=" << cancelled
                    << " message=" << message << Qt::endl;
            }
            loop.quit();
        });
    QString error;
    if (!controller.Start(
            packageDirectory, moduleDirectory, settings, &error))
    {
        QTextStream(stderr)
            << "RIPFLOW_JOB_SELF_TEST_START_FAILED: " << error << Qt::endl;
        return 18;
    }
    if (cancelAfterMs >= 0)
    {
        QTimer::singleShot(cancelAfterMs, &controller, [&controller]()
        {
            QString ignored;
            (void)controller.Cancel(&ignored);
        });
    }
    loop.exec();
    return result;
}
}

int main(int argc, char* argv[])
{
    bool selfTestRequested = false;
    bool capabilityCoverageRequested = false;
    for (int index = 1; index < argc; ++index)
    {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        if (argument == QStringLiteral("--self-test"))
        {
            selfTestRequested = true;
        }
        else if (argument == QStringLiteral("--capability-self-test"))
        {
            capabilityCoverageRequested = true;
        }
    }

    if (selfTestRequested || capabilityCoverageRequested)
    {
        QCoreApplication application(argc, argv);
        const QStringList arguments = application.arguments();
        const QString requestedPath = FindArgumentValue(
            arguments,
            QStringLiteral("--module"));
        const QString modulePath = requestedPath.isEmpty()
            ? DefaultModulePath() : requestedPath;
        if (capabilityCoverageRequested)
        {
            return RunCapabilityCoverage(
                modulePath,
                FindArgumentValue(arguments, QStringLiteral("--repo-root")),
                FindArgumentValue(
                    arguments, QStringLiteral("--evidence-root")));
        }
        return RunSelfTest(modulePath);
    }

    QApplication application(argc, argv);
    const QStringList arguments = application.arguments();
    if (HasArgument(arguments, QStringLiteral("--help")))
    {
        QTextStream(stdout)
            << "slicer_ui_host_sim [--module <slicer_module.dll>] "
            << "[--self-test | --capability-self-test "
            << "--repo-root <path> --evidence-root <path> | "
            << "--hostflow-import-ui-self-test | "
            << "--hostflow-profile-ui-self-test | "
            << "--hostflow-settings-ui-self-test | "
            << "--hostflow-job-ui-self-test | "
            << "--hostflow-result-ui-self-test | "
            << "--hostflow-workspace-ui-self-test | "
            << "--rip-module-self-test [--rip-module <path>] | "
            << "--rip-ui-self-test | "
            << "--rip-job-self-test --package <path> "
            << "--rip-module <path> [--transparent-mode <mode>] "
            << "[--gray-bits <1|2>] [--timeout-seconds <n>] "
            << "[--cancel-after-ms <n>] "
            << "[--expect <success|cancel|timeout|failure>]]"
            << Qt::endl;
        return 0;
    }

    const QString requestedPath = FindArgumentValue(
        arguments,
        QStringLiteral("--module"));
    const QString modulePath = requestedPath.isEmpty()
        ? DefaultModulePath() : requestedPath;
    if (HasArgument(
            arguments,
            QStringLiteral("--hostflow-import-ui-self-test")))
    {
        return RunHostFlowImportUiSmoke(modulePath);
    }
    if (HasArgument(
            arguments,
            QStringLiteral("--hostflow-profile-ui-self-test")))
    {
        return RunHostFlowProfileUiSmoke(modulePath);
    }
    if (HasArgument(
            arguments,
            QStringLiteral("--hostflow-settings-ui-self-test")))
    {
        return RunHostFlowSettingsUiSmoke(modulePath);
    }
    if (HasArgument(
            arguments,
            QStringLiteral("--hostflow-job-ui-self-test")))
    {
        return RunHostFlowJobUiSmoke(modulePath);
    }
    if (HasArgument(
            arguments,
            QStringLiteral("--hostflow-result-ui-self-test")))
    {
        return RunHostFlowResultUiSmoke(modulePath);
    }
    if (HasArgument(
            arguments,
            QStringLiteral("--hostflow-workspace-ui-self-test")))
    {
        return RunHostFlowWorkspaceUiSmoke(modulePath);
    }
    if (HasArgument(arguments, QStringLiteral("--rip-module-self-test")))
    {
        const QString requestedRipModule = FindArgumentValue(
            arguments, QStringLiteral("--rip-module"));
        return RunRipModuleSelfTest(
            requestedRipModule.isEmpty()
                ? HostRipJobController::DefaultModuleDirectory()
                : requestedRipModule);
    }
    if (HasArgument(arguments, QStringLiteral("--rip-ui-self-test")))
    {
        return RunRipUiSmoke(modulePath);
    }
    if (HasArgument(arguments, QStringLiteral("--rip-job-self-test")))
    {
        const QString transparentMode = FindArgumentValue(
            arguments, QStringLiteral("--transparent-mode"));
        bool grayBitsValid{false};
        const int grayBits = FindArgumentValue(
            arguments, QStringLiteral("--gray-bits")).toInt(
                &grayBitsValid);
        bool timeoutValid{false};
        const int timeoutSeconds = FindArgumentValue(
            arguments, QStringLiteral("--timeout-seconds")).toInt(
                &timeoutValid);
        bool cancelValid{false};
        const int cancelAfterMs = FindArgumentValue(
            arguments, QStringLiteral("--cancel-after-ms")).toInt(
                &cancelValid);
        const QString expectedOutcome = FindArgumentValue(
            arguments, QStringLiteral("--expect"));
        return RunRipJobSelfTest(
            FindArgumentValue(arguments, QStringLiteral("--package")),
            FindArgumentValue(arguments, QStringLiteral("--rip-module")),
            transparentMode.isEmpty()
                ? QStringLiteral("explicit_transparent")
                : transparentMode,
            grayBitsValid ? grayBits : 2,
            timeoutValid ? timeoutSeconds : 60,
            cancelValid ? cancelAfterMs : -1,
            expectedOutcome.isEmpty()
                ? QStringLiteral("success")
                : expectedOutcome);
    }
    HostMainWindow window(modulePath);
    window.show();
    return application.exec();
}
