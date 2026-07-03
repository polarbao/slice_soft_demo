#include "UiSmokeTestRunner.h"

#include "../MainWindow.h"
#include "../widgets/ChannelChartPanel.h"
#include "../widgets/LayerPreviewPanel.h"
#include "../widgets/PreviewOverlayPanel.h"
#include "ConfigDocument.h"
#include "PackageLoader.h"
#include "PreviewReportIndex.h"
#include "ReportLoader.h"
#include "ScenarioRegistry.h"
#include "ToolPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{

QJsonArray StringArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values)
    {
        array.append(value);
    }
    return array;
}

QJsonObject BuildExperimentalReportFixture()
{
    QJsonObject admission;
    admission["mode"] = "strict_closed";
    admission["status"] = "non_production_only";
    admission["productionAllowed"] = false;
    admission["nonProduction"] = true;
    admission["blockerCodes"] = StringArray(QStringList{"OPENVDB_UNAVAILABLE"});
    admission["warningCodes"] = StringArray(QStringList{"EXPERIMENTAL_CLI_DIAGNOSTIC_ONLY"});
    admission["reasonCodes"] = StringArray(QStringList{"OPENVDB_UNAVAILABLE", "EXPERIMENTAL_CLI_DIAGNOSTIC_ONLY"});

    QJsonObject openvdb;
    openvdb["enabled"] = false;
    openvdb["available"] = false;
    openvdb["version"] = "";

    QJsonObject legacyPath;
    legacyPath["executed"] = false;
    legacyPath["productionPackageWritten"] = false;

    QJsonObject root;
    root["schema"] = "p0.experimental_openvdb_shell_cli_report.1";
    root["experimentalOpenvdbShell"] = true;
    root["legacyPathExecuted"] = false;
    root["productionPackageWritten"] = false;
    root["writeProductionRgbwsv"] = false;
    root["openvdb"] = openvdb;
    root["productionAdmission"] = admission;
    root["legacyPath"] = legacyPath;
    return root;
}

bool ContainsAll(const QString& text, const QStringList& expected)
{
    for (const QString& value : expected)
    {
        if (!text.contains(value))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

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
    if (options.case_name == "overlay-load-real") {
        return overlayLoadReal(options);
    }
    if (options.case_name == "layer-preview-load") {
        return layerPreviewLoad(options);
    }
    if (options.case_name == "compare-profiles") {
        return compareProfiles(options);
    }
    if (options.case_name == "scenario-registry") {
        return scenarioRegistry(options);
    }
    if (options.case_name == "experimental-report-summary") {
        return experimentalReportSummary(options);
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

int UiSmokeTestRunner::overlayLoadReal(const UiSmokeTestOptions& options) {
    const QString package_path = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(package_path);
    if (package.manifest_path.isEmpty()) {
        return fail("overlay-load-real 未找到 manifest：" + package_path);
    }

    PreviewReportIndex index;
    if (!index.load(package.package_dir)) {
        return fail("overlay-load-real 无法读取 preview_report：" + index.errorString());
    }
    bool has_kind = false;
    bool has_layer = false;
    bool has_channel = false;
    for (const PreviewReportEntry& entry : index.entries()) {
        has_kind = has_kind || !entry.kind.isEmpty();
        has_layer = has_layer || entry.layer_index >= 0;
        has_channel = has_channel || !entry.channel.isEmpty();
    }
    if (!has_kind || !has_layer || !has_channel) {
        return fail("overlay-load-real preview_report 缺少 channel/layerIndex/kind 元数据。");
    }

    QFile preview_report_file(QDir(package.package_dir).filePath("reports/preview_report.json"));
    if (!preview_report_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail("overlay-load-real 无法打开 preview_report.json。");
    }
    const QJsonDocument preview_report = QJsonDocument::fromJson(preview_report_file.readAll());
    if (preview_report.object().value("schema").toString() != "p0.preview_report.1") {
        return fail("overlay-load-real preview_report.schema 不是 p0.preview_report.1。");
    }

    PreviewOverlayPanel panel;
    panel.loadPackage(package);
    if (panel.imageCount() <= 0) {
        return fail("overlay-load-real 没有加载到 preview 图像。");
    }
    const QStringList channels = panel.availableChannels();
    if (!channels.contains("rgb")) {
        return fail("overlay-load-real 缺少 RGB preview。");
    }
    QStringList passed_modes;
    for (const QString& mode : QStringList{"RGB + W 白墨", "RGB + V 光油", "RGB + S 支撑"}) {
        if (panel.canComposeMode(mode)) {
            passed_modes.push_back(mode);
        }
    }
    if (passed_modes.isEmpty()) {
        return fail("overlay-load-real 未能组合任何真实 overlay 图。");
    }
    return pass(QString("overlay-load-real images=%1 channels=%2 modes=%3")
                    .arg(panel.imageCount())
                    .arg(channels.join(","))
                    .arg(passed_modes.join(",")));
}

int UiSmokeTestRunner::layerPreviewLoad(const UiSmokeTestOptions& options) {
    const QString package_path = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(package_path);
    if (package.manifest_path.isEmpty()) {
        return fail("layer-preview-load 未找到 manifest：" + package_path);
    }
    if (package.preview_paths.isEmpty()) {
        return fail("layer-preview-load 未找到 preview 图像：" + package_path);
    }

    LayerPreviewPanel panel;
    panel.LoadPackage(package);
    if (panel.LayerCount() <= 0) {
        return fail("layer-preview-load 未读取到 layerCount。");
    }

    const QStringList channels = panel.AvailableChannels();
    const QStringList required_channels{"occupancy", "diagnostic"};
    for (const QString& channel : required_channels) {
        if (!channels.contains(channel)) {
            return fail("layer-preview-load 缺少通道：" + channel + "，实际通道：" + channels.join(","));
        }
    }
    QStringList render_channels;
    for (const QString& channel : QStringList{"production_rgb", "texture_rgb", "rgb", "support", "white", "varnish", "occupancy", "diagnostic"})
    {
        if (channels.contains(channel))
        {
            render_channels.push_back(channel);
        }
    }
    if (render_channels.size() <= required_channels.size())
    {
        return fail("layer-preview-load 未发现任何材料预览通道，实际通道：" + channels.join(","));
    }

    const QList<int> layer_indices{0, panel.LayerCount() / 2, panel.LayerCount() - 1};
    for (const int layer_index : layer_indices) {
        if (!panel.SelectLayerForTest(layer_index)) {
            return fail("layer-preview-load 无法选择层：" + QString::number(layer_index));
        }
        for (const QString& channel : render_channels) {
            if (!panel.SelectChannelForTest(channel)) {
                return fail("layer-preview-load 无法选择通道：" + channel);
            }
            const QImage image = panel.CurrentImageForTest();
            if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
                return fail(QString("layer-preview-load 渲染为空：layer=%1 channel=%2").arg(layer_index).arg(channel));
            }
        }
    }

    if (channels.contains("production_rgb"))
    {
        if (!panel.SelectChannelForTest("production_rgb"))
        {
            return fail("layer-preview-load 无法选择生产 RGB 通道。");
        }
        const QImage productionImage = panel.CurrentImageForTest();
        if (productionImage.isNull() || productionImage.width() <= 0 || productionImage.height() <= 0)
        {
            return fail("layer-preview-load 生产 RGB 渲染为空。");
        }
        const QString probe = panel.PixelProbeForTest(productionImage.width() / 2, productionImage.height() / 2);
        if (!probe.contains("RGBWSV="))
        {
            return fail("layer-preview-load 生产 RGB 像素探针未返回 RGBWSV。");
        }
    }

    return pass(QString("layer-preview-load layers=%1 channels=%2")
                    .arg(panel.LayerCount())
                    .arg(channels.join(",")));
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

int UiSmokeTestRunner::scenarioRegistry(const UiSmokeTestOptions& options)
{
    ScenarioRegistry registry;
    if (!registry.Load(options.repo_root))
    {
        return fail("scenario-registry 无法加载场景索引：" + registry.Warnings().join("; "));
    }
    int defaultVisibleCount = 0;
    int fixtureCount = 0;
    int advancedCount = 0;
    for (const ScenarioEntry& scenario : registry.Entries())
    {
        if (!scenario.enabled)
        {
            continue;
        }
        if (scenario.visibility == "fixture")
        {
            ++fixtureCount;
        }
        else if (scenario.visibility == "advanced")
        {
            ++advancedCount;
        }
        else
        {
            ++defaultVisibleCount;
        }
    }
    if (registry.DefaultScenarioId().isEmpty() || registry.FindById(registry.DefaultScenarioId()) == nullptr)
    {
        return fail("scenario-registry defaultScenarioId 无效。");
    }
    if (defaultVisibleCount <= 0 || fixtureCount <= 0 || advancedCount <= 0)
    {
        return fail(QString("scenario-registry 分层不足 default=%1 fixture=%2 advanced=%3")
                        .arg(defaultVisibleCount)
                        .arg(fixtureCount)
                        .arg(advancedCount));
    }
    return pass(QString("scenario-registry default=%1 fixture=%2 advanced=%3")
                    .arg(defaultVisibleCount)
                    .arg(fixtureCount)
                    .arg(advancedCount));
}

int UiSmokeTestRunner::experimentalReportSummary(const UiSmokeTestOptions& options) {
    Q_UNUSED(options);
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail("experimental-report-summary 无法创建临时目录。");
    }
    QDir packageDir(tempDir.path());
    if (!packageDir.mkpath("reports")) {
        return fail("experimental-report-summary 无法创建 reports 目录。");
    }

    const QString reportPath = packageDir.filePath("reports/experimental_openvdb_shell_report.json");
    QFile reportFile(reportPath);
    if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return fail("experimental-report-summary 无法写入 report fixture。");
    }
    reportFile.write(QJsonDocument(BuildExperimentalReportFixture()).toJson(QJsonDocument::Indented));
    reportFile.close();

    const PackageSummary package = PackageLoader().load(packageDir.path());
    if (!package.report_paths.contains(reportPath)) {
        return fail("experimental-report-summary PackageLoader 未发现 experimental report。");
    }

    const JsonReport report = ReportLoader().load(reportPath);
    const QString summary = ReportLoader::summarize(report);
    const QString warnings = ReportLoader::collectWarningsAndFailures(report.document.object());
    const QStringList expectedSummary{
        "OpenVDB 可用: 否",
        "准入状态: non_production_only",
        "允许生产: 否",
        "仅非生产: 是",
        "阻断码: OPENVDB_UNAVAILABLE",
        "警告码: EXPERIMENTAL_CLI_DIAGNOSTIC_ONLY",
        "legacyPathExecuted: 否",
        "productionPackageWritten: 否",
    };
    if (!ContainsAll(summary, expectedSummary)) {
        return fail("experimental-report-summary 摘要缺少关键字段：\n" + summary);
    }
    if (!warnings.contains("productionAdmission.blockerCodes: OPENVDB_UNAVAILABLE")
        || !warnings.contains("productionAdmission.warningCodes: EXPERIMENTAL_CLI_DIAGNOSTIC_ONLY")) {
        return fail("experimental-report-summary 警告视图缺少 blocker/warning codes：\n" + warnings);
    }
    return pass("experimental-report-summary");
}

int UiSmokeTestRunner::fail(const QString& message) const {
    QTextStream(stderr) << "FAIL " << message << Qt::endl;
    return 1;
}

int UiSmokeTestRunner::pass(const QString& message) const {
    QTextStream(stdout) << "PASS " << message << Qt::endl;
    return 0;
}
