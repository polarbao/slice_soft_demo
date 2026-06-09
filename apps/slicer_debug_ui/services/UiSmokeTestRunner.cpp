#include "UiSmokeTestRunner.h"

#include "../MainWindow.h"
#include "../widgets/ChannelChartPanel.h"
#include "../widgets/PreviewOverlayPanel.h"
#include "ConfigDocument.h"
#include "PackageLoader.h"
#include "PreviewReportIndex.h"
#include "ToolPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>

int UiSmokeTestRunner::run(const UiSmokeTestOptions& options) {
    if (options.case_name == "startup") {
        return startup(options);
    }
    if (options.case_name == "load-package") {
        return loadPackage(options);
    }
    if (options.case_name == "save-as-config") {
        return saveAsConfig(options);
    }
    if (options.case_name == "chart-load") {
        return chartLoad(options);
    }
    if (options.case_name == "overlay-load") {
        return overlayLoad(options);
    }
    if (options.case_name == "compare-profiles") {
        return compareProfiles(options);
    }
    return fail("未知 ui smoke test case：" + options.case_name);
}

QString UiSmokeTestRunner::absoluteFromRepo(const UiSmokeTestOptions& options, const QString& path) const {
    if (path.isEmpty()) {
        return {};
    }
    const QFileInfo info(path);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }
    return QDir(options.repo_root).filePath(path);
}

int UiSmokeTestRunner::startup(const UiSmokeTestOptions& options) {
    MainWindow window(options.repo_root);
    Q_UNUSED(window);
    return pass("startup");
}

int UiSmokeTestRunner::loadPackage(const UiSmokeTestOptions& options) {
    const QString package_path = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(package_path);
    if (package.manifest_path.isEmpty()) {
        return fail("load-package 未找到 manifest：" + package_path);
    }
    return pass("load-package reports=" + QString::number(package.report_paths.size()));
}

int UiSmokeTestRunner::saveAsConfig(const UiSmokeTestOptions& options) {
    const QString config_path = absoluteFromRepo(options, options.config_path);
    const QString output_path = absoluteFromRepo(options, options.output_path);
    if (config_path.isEmpty() || output_path.isEmpty()) {
        return fail("save-as-config 需要 --config 和 --output。");
    }
    ConfigDocument document;
    if (!document.load(config_path)) {
        return fail(document.errorString());
    }
    document.setValue({"materialProcessProfile", "varnish", "topLayers"}, 3);
    document.setValue({"materialPolicy", "varnish", "topLayers"}, 3);
    QDir().mkpath(QFileInfo(output_path).absolutePath());
    if (!document.saveAs(output_path, nullptr, SaveOptions{options.yes})) {
        return fail(document.errorString());
    }
    return QFileInfo::exists(output_path) ? pass("save-as-config " + output_path) : fail("save-as-config 未生成输出文件。");
}

int UiSmokeTestRunner::chartLoad(const UiSmokeTestOptions& options) {
    const QString package_path = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(package_path);
    ChannelChartPanel panel;
    panel.loadPackage(package);
    if (panel.layerStatCount() <= 0) {
        return fail("chart-load 未读取到 material_process_report layers。");
    }
    return pass("chart-load layers=" + QString::number(panel.layerStatCount()));
}

int UiSmokeTestRunner::overlayLoad(const UiSmokeTestOptions& options) {
    const QString package_path = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(package_path);
    PreviewOverlayPanel panel;
    panel.loadPackage(package);
    if (panel.imageCount() > 0) {
        return pass("overlay-load images=" + QString::number(panel.imageCount()));
    }
    PreviewReportIndex index;
    if (index.load(package.package_dir) || QFileInfo::exists(QDir(package.package_dir).filePath("reports/preview_report.json"))) {
        return pass("overlay-load graceful-empty-preview");
    }
    return fail("overlay-load 未找到 preview 图像或 preview_report。");
}

int UiSmokeTestRunner::compareProfiles(const UiSmokeTestOptions& options) {
    const QString package_a = absoluteFromRepo(options, options.package_a_path);
    const QString package_b = absoluteFromRepo(options, options.package_b_path);
    const QString output = absoluteFromRepo(options, options.output_path);
    if (package_a.isEmpty() || package_b.isEmpty() || output.isEmpty()) {
        return fail("compare-profiles 需要 --package-a、--package-b 和 --output。");
    }
    QDir().mkpath(QFileInfo(output).absolutePath());
    const ToolPaths paths = ToolPaths::fromRepoRoot(options.repo_root);
    QProcess process;
    process.setWorkingDirectory(options.repo_root);
    process.start(paths.powershell,
                  QStringList{"-ExecutionPolicy",
                              "Bypass",
                              "-File",
                              "scripts/compare_material_profiles.ps1",
                              "-PackageA",
                              package_a,
                              "-PackageB",
                              package_b,
                              "-Output",
                              output});
    if (!process.waitForFinished(30000)) {
        return fail("compare-profiles 超时。");
    }
    if (process.exitCode() != 0) {
        return fail("compare-profiles 失败：" + QString::fromLocal8Bit(process.readAllStandardError()));
    }
    return QFileInfo::exists(output) ? pass("compare-profiles " + output) : fail("compare-profiles 未生成输出。");
}

int UiSmokeTestRunner::fail(const QString& message) const {
    QTextStream(stderr) << "FAIL " << message << Qt::endl;
    return 1;
}

int UiSmokeTestRunner::pass(const QString& message) const {
    QTextStream(stdout) << "PASS " << message << Qt::endl;
    return 0;
}
