#include "CapabilityCoverageRunner.h"
#include "HostMainWindow.h"
#include "ModuleClient.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QStringList>
#include <QTextStream>

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
            << "--repo-root <path> --evidence-root <path>]"
            << Qt::endl;
        return 0;
    }

    const QString requestedPath = FindArgumentValue(
        arguments,
        QStringLiteral("--module"));
    HostMainWindow window(
        requestedPath.isEmpty() ? DefaultModulePath() : requestedPath);
    window.show();
    return application.exec();
}
