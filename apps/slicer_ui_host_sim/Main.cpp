#include "CapabilityCoverageRunner.h"
#include "HostMainWindow.h"
#include "HostWorkspaceState.h"
#include "ModuleClient.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTextStream>
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
    if (dpiXSpin == nullptr || dpiYSpin == nullptr || layerSpin == nullptr
        || profilePreview == nullptr || validationLabel == nullptr
        || dpiXSpin->value() != 635 || dpiYSpin->value() != 600
        || std::abs(layerSpin->value() - 0.038) > 1.0e-9
        || validationLabel->text().isEmpty())
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
    if (startButton == nullptr || cancelButton == nullptr
        || progressBar == nullptr || statusLabel == nullptr
        || startButton->isEnabled() || cancelButton->isEnabled()
        || progressBar->minimum() != 0 || progressBar->maximum() != 100
        || statusLabel->text().isEmpty())
    {
        QTextStream(stderr)
            << "HOSTFLOW_HB06_UI_FAILED: job panel is incomplete"
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
    if (panel == nullptr || layerSlider == nullptr || previewMode == nullptr
        || reportCombo == nullptr || chart == nullptr
        || layerSlider->isEnabled() || previewMode->count() < 7
        || reportCombo->count() < 3)
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
        || HostWorkspaceState::SchemaVersion() != 1)
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
            << "--hostflow-workspace-ui-self-test]"
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
    HostMainWindow window(modulePath);
    window.show();
    return application.exec();
}
