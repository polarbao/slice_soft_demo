#include "MainWindow.h"
#include "services/UiSmokeTestRunner.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QStringList>

namespace {

QString optionValue(const QStringList& args, const QString& name) {
    const int index = args.indexOf(name);
    if (index < 0 || index + 1 >= args.size()) {
        return {};
    }
    return args.at(index + 1);
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.contains("--ui-smoke-test")) {
        UiSmokeTestOptions options;
        options.repo_root = QDir::currentPath();
        options.case_name = optionValue(args, "--case");
        options.config_path = optionValue(args, "--config");
        options.package_path = optionValue(args, "--package");
        options.package_a_path = optionValue(args, "--package-a");
        options.package_b_path = optionValue(args, "--package-b");
        options.output_path = optionValue(args, "--output");
        options.yes = args.contains("--yes");
        return UiSmokeTestRunner().run(options);
    }

    MainWindow window(QDir::currentPath());
    if (args.contains("--self-test")) {
        return 0;
    }
    window.show();
    return app.exec();
}
