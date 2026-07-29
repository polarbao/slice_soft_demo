#include "MainWindow.h"
#include "services/UiSmokeTestRunner.h"
#include "services/WorkspaceLayoutState.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace
{

QString OptionValue(const QStringList& args, const QString& name)
{
    const int index = args.indexOf(name);
    if (index < 0 || index + 1 >= args.size())
    {
        return {};
    }
    return args.at(index + 1);
}

QString ResolveApplicationRoot()
{
    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QString packagedScenarioRegistry =
        applicationDir.filePath("samples/scenarios/slicer_scenarios.json");
    if (QFileInfo::exists(packagedScenarioRegistry))
    {
        return applicationDir.absolutePath();
    }
    return QDir::currentPath();
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(
        WorkspaceLayoutState::OrganizationName());
    QCoreApplication::setApplicationName(
        WorkspaceLayoutState::ApplicationName());
    const QStringList args = QCoreApplication::arguments();
    const QString applicationRoot = ResolveApplicationRoot();
    if (args.contains("--ui-smoke-test"))
    {
        UiSmokeTestOptions options;
        options.repo_root = applicationRoot;
        options.case_name = OptionValue(args, "--case");
        options.config_path = OptionValue(args, "--config");
        options.package_path = OptionValue(args, "--package");
        options.package_a_path = OptionValue(args, "--package-a");
        options.package_b_path = OptionValue(args, "--package-b");
        options.output_path = OptionValue(args, "--output");
        options.yes = args.contains("--yes");
        return UiSmokeTestRunner().run(options);
    }

    if (args.contains("--self-test"))
    {
        UiSmokeTestRunner runner;
        UiSmokeTestOptions options;
        options.repo_root = applicationRoot;
        options.case_name = "startup";
        const int startupResult = runner.run(options);
        if (startupResult != 0)
        {
            return startupResult;
        }
        options.case_name = "experimental-report-summary";
        return runner.run(options);
    }

    MainWindow window(applicationRoot);
    window.show();
    return app.exec();
}
