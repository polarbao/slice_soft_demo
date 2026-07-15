#include "UiSmokeTestRunner.h"

#include "../MainWindow.h"
#include "../widgets/ChannelChartPanel.h"
#include "../widgets/ConfigEditorPanel.h"
#include "../widgets/DiagnosticsDock.h"
#include "../widgets/LayerPreviewPanel.h"
#include "../widgets/PreviewOverlayPanel.h"
#include "../widgets/PreviewPanel.h"
#include "../widgets/QuickConfigPanel.h"
#include "../widgets/ReportPanel.h"
#include "../widgets/SettingHelpPanel.h"
#include "../widgets/PreviewWorkspace.h"
#include "ConfigDocument.h"
#include "EffectiveConfigGenerator.h"
#include "HelpTextProvider.h"
#include "PackageLoader.h"
#include "PreviewReportIndex.h"
#include "ReportLoader.h"
#include "ScenarioRegistry.h"
#include "SliceSettingsModel.h"
#include "ToolPaths.h"

#include <QComboBox>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>
#include <QRect>
#include <QSet>
#include <QSize>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTabWidget>
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

QJsonObject BuildOpenVdbUtilityItem(
    const bool available,
    const bool executed,
    const QString& status,
    const QString& source,
    const QString& promoteDecision,
    const QStringList& blockers)
{
    QJsonObject utility;
    utility[QStringLiteral("available")] = available;
    utility[QStringLiteral("executed")] = executed;
    utility[QStringLiteral("status")] = status;
    utility[QStringLiteral("source")] = source;
    utility[QStringLiteral("promoteDecision")] = promoteDecision;
    utility[QStringLiteral("metrics")] = QJsonObject{};
    utility[QStringLiteral("timingsMs")] = QJsonObject{};
    utility[QStringLiteral("blockers")] = StringArray(blockers);
    utility[QStringLiteral("warnings")] = QJsonArray{};
    utility[QStringLiteral("notes")] = QJsonArray{};
    return utility;
}

QJsonObject BuildOpenVdbUtilityReportFixture(const bool openVdbAvailable)
{
    QJsonObject build;
    build[QStringLiteral("buildType")] = QStringLiteral("Debug");
    build[QStringLiteral("useOpenVdb")] = openVdbAvailable;
    build[QStringLiteral("openVdbAvailable")] = openVdbAvailable;
    build[QStringLiteral("openVdbVersion")] =
        openVdbAvailable ? QJsonValue{QStringLiteral("12.0.1")} : QJsonValue{QJsonValue::Null};
    build[QStringLiteral("openVdbUnavailableReason")] =
        openVdbAvailable ? QJsonValue{QJsonValue::Null} : QJsonValue{QStringLiteral("use_openvdb_off")};

    QJsonObject outputPolicy;
    outputPolicy[QStringLiteral("writesProductionPackage")] = false;
    outputPolicy[QStringLiteral("writesProductionTiff")] = false;
    outputPolicy[QStringLiteral("writesPreview")] = false;
    outputPolicy[QStringLiteral("writesUtilityReport")] = true;
    outputPolicy[QStringLiteral("modifiesLegacyOutput")] = false;
    outputPolicy[QStringLiteral("protocolSchemaTouched")] = false;

    QJsonObject utilities;
    if (openVdbAvailable)
    {
        utilities[QStringLiteral("outerVarnishShell")] = BuildOpenVdbUtilityItem(
            true,
            true,
            QStringLiteral("pass"),
            QStringLiteral("openvdb_sdf_shell"),
            QStringLiteral("promote"),
            {});
        utilities[QStringLiteral("clearanceDistance")] = BuildOpenVdbUtilityItem(
            false,
            false,
            QStringLiteral("not_evaluated"),
            QStringLiteral("openvdb_sdf_distance"),
            QStringLiteral("keep_experimental"),
            {QStringLiteral("clearance_utility_not_implemented")});
        utilities[QStringLiteral("topologyDiagnostic")] = BuildOpenVdbUtilityItem(
            true,
            true,
            QStringLiteral("pass"),
            QStringLiteral("mesh_diagnostics_plus_openvdb_admission"),
            QStringLiteral("promote"),
            {});
        utilities[QStringLiteral("materialClosureAssist")] = BuildOpenVdbUtilityItem(
            false,
            false,
            QStringLiteral("not_evaluated"),
            QStringLiteral("semantic_mask_plus_sdf_assist"),
            QStringLiteral("keep_experimental"),
            {QStringLiteral("material_closure_assist_not_implemented")});
    }
    else
    {
        const QStringList blocker{QStringLiteral("use_openvdb_off")};
        utilities[QStringLiteral("outerVarnishShell")] = BuildOpenVdbUtilityItem(
            false,
            false,
            QStringLiteral("unavailable"),
            QStringLiteral("openvdb_sdf_shell"),
            QStringLiteral("not_evaluated"),
            blocker);
        utilities[QStringLiteral("clearanceDistance")] = BuildOpenVdbUtilityItem(
            false,
            false,
            QStringLiteral("unavailable"),
            QStringLiteral("openvdb_sdf_distance"),
            QStringLiteral("not_evaluated"),
            blocker);
        utilities[QStringLiteral("topologyDiagnostic")] = BuildOpenVdbUtilityItem(
            false,
            false,
            QStringLiteral("unavailable"),
            QStringLiteral("mesh_diagnostics_plus_openvdb_admission"),
            QStringLiteral("not_evaluated"),
            blocker);
        utilities[QStringLiteral("materialClosureAssist")] = BuildOpenVdbUtilityItem(
            false,
            false,
            QStringLiteral("unavailable"),
            QStringLiteral("semantic_mask_plus_sdf_assist"),
            QStringLiteral("not_evaluated"),
            blocker);
    }

    QJsonObject decision;
    decision[QStringLiteral("openVdbRole")] =
        openVdbAvailable ? QStringLiteral("sdf_utility_candidate") : QStringLiteral("unavailable");
    decision[QStringLiteral("productionReplacementAllowed")] = false;
    decision[QStringLiteral("recommendedNextStep")] = openVdbAvailable
        ? QStringLiteral("promote_outer_shell_and_topology_utility_design")
        : QStringLiteral("configure_or_run_openvdb_on_lane");
    decision[QStringLiteral("capabilitySummary")] = QJsonObject{};

    QJsonObject legacyGuard;
    legacyGuard[QStringLiteral("ran")] = false;
    legacyGuard[QStringLiteral("reason")] =
        QStringLiteral("utility_probe_does_not_run_or_modify_legacy_output");
    QJsonObject validation;
    validation[QStringLiteral("schemaValid")] = true;
    validation[QStringLiteral("legacyGuard")] = legacyGuard;

    QJsonArray issues;
    if (!openVdbAvailable)
    {
        QJsonObject issue;
        issue[QStringLiteral("severity")] = QStringLiteral("warning");
        issue[QStringLiteral("code")] = QStringLiteral("use_openvdb_off");
        issue[QStringLiteral("message")] = QStringLiteral("OpenVDB utility is unavailable.");
        issues.append(issue);
    }

    QJsonObject root;
    root[QStringLiteral("schema")] = QStringLiteral("slicesoft.openvdb_sdf_utility.12b_r2.1");
    root[QStringLiteral("build")] = build;
    root[QStringLiteral("outputPolicy")] = outputPolicy;
    root[QStringLiteral("utilities")] = utilities;
    root[QStringLiteral("decision")] = decision;
    root[QStringLiteral("validation")] = validation;
    root[QStringLiteral("issues")] = issues;
    return root;
}

bool WriteJsonFixture(const QString& path, const QJsonObject& object)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
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

QRect GlobalRect(const QWidget* widget)
{
    return QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size());
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
    if (options.case_name == "slice-settings-model") {
        return sliceSettingsModel(options);
    }
    if (options.case_name == "setting-help-metadata")
    {
        return SettingHelpMetadataCase(options);
    }
    if (options.case_name == "preview-workspace-shared-layer")
    {
        return PreviewWorkspaceSharedLayer(options);
    }
    if (options.case_name == "preview-legend-probe-context")
    {
        return PreviewLegendProbeContext(options);
    }
    if (options.case_name == "diagnostics-collapse")
    {
        return DiagnosticsCollapse(options);
    }
    if (options.case_name == "openvdb-utility-summary")
    {
        return OpenVdbUtilitySummary(options);
    }
    if (options.case_name == "workspace-layout-sizes")
    {
        return WorkspaceLayoutSizes(options);
    }
    if (options.case_name == "generated-effective-config") {
        return GeneratedEffectiveConfig(options);
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
    const QString overlayStatus = panel.StatusForTest();
    if (!overlayStatus.contains("semantic:") || !overlayStatus.contains("sourcePolicy:"))
    {
        return fail("overlay-load-real 状态栏未显示 semantic/sourcePolicy：" + overlayStatus);
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
        if (!probe.contains("semantic=") || !probe.contains("sourcePolicy="))
        {
            return fail("layer-preview-load 生产 RGB 像素探针未返回 semantic/sourcePolicy：" + probe);
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
    QStringList normalIds;
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
        else if (scenario.visibility == "normal")
        {
            ++defaultVisibleCount;
            normalIds.push_back(scenario.id);
            if (scenario.displayname.isEmpty()
                || scenario.category.isEmpty()
                || scenario.inputformats.isEmpty()
                || scenario.materialcapabilities.isEmpty()
                || scenario.productionsafety.isEmpty()
                || scenario.docpath.isEmpty())
            {
                return fail("scenario-registry 稳定 Profile 元数据不完整：" + scenario.id);
            }
            if (!QFileInfo::exists(QDir(options.repo_root).filePath(scenario.docpath)))
            {
                return fail("scenario-registry Profile 文档不存在：" + scenario.docpath);
            }
            if (!QFileInfo::exists(QDir(options.repo_root).filePath(scenario.configpath)))
            {
                return fail("scenario-registry Profile 模板不存在：" + scenario.configpath);
            }
            const QString expectedSafety = scenario.id == "production_rgb_inspection"
                ? QStringLiteral("diagnostic")
                : QStringLiteral("production");
            if (scenario.productionsafety != expectedSafety
                || scenario.experimental
                || scenario.requiresopenvdb)
            {
                return fail("scenario-registry Profile 生产安全标记不符合冻结决策：" + scenario.id);
            }
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
    QStringList expectedNormalIds{
        "production_rgb_inspection",
        "single_material_relief",
        "textured_nail_rgb_varnish_lower_support",
        "textured_nail_rgb_white_lower_support",
    };
    normalIds.sort();
    expectedNormalIds.sort();
    if (normalIds != expectedNormalIds)
    {
        return fail(QString("scenario-registry 稳定 Profile 集不符合冻结决策，实际=%1")
                        .arg(normalIds.join(",")));
    }
    const ScenarioEntry* defaultScenario = registry.FindById(registry.DefaultScenarioId());
    if (defaultScenario == nullptr || defaultScenario->visibility != "normal")
    {
        return fail("scenario-registry 默认 Profile 不是稳定 Profile。");
    }
    if (!registry.Warnings().isEmpty())
    {
        return fail("scenario-registry 存在索引告警：" + registry.Warnings().join("; "));
    }
    const ScenarioEntry* openVdbScenario = registry.FindById("openvdb_surface_shell_3mf_real");
    if (openVdbScenario == nullptr
        || openVdbScenario->visibility != "advanced"
        || openVdbScenario->productionsafety != "experimental_only")
    {
        return fail("scenario-registry OpenVDB 实验场景边界不正确。");
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

int UiSmokeTestRunner::sliceSettingsModel(const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    struct ProfileExpectation
    {
        QString id;
        ModelFillMaterial fillmaterial;
        int previewinterval;
    };
    const QVector<ProfileExpectation> expectations{
        {QStringLiteral("textured_nail_rgb_white_lower_support"), ModelFillMaterial::White, 10},
        {QStringLiteral("textured_nail_rgb_varnish_lower_support"), ModelFillMaterial::Varnish, 10},
        {QStringLiteral("single_material_relief"), ModelFillMaterial::White, 10},
        {QStringLiteral("production_rgb_inspection"), ModelFillMaterial::White, 1},
    };

    SliceSettingsModel model;
    for (const ProfileExpectation& expectation : expectations)
    {
        if (!model.ApplyProfileDefaults(expectation.id))
        {
            return fail("slice-settings-model 无法应用稳定 Profile：" + expectation.id);
        }
        const SliceSettingsState& defaults = model.State();
        if (defaults.profileid != expectation.id
            || defaults.modelfillmaterial != expectation.fillmaterial
            || defaults.preview.interval != expectation.previewinterval
            || !defaults.support.enabled
            || defaults.support.placement != SupportPlacement::Lower
            || !defaults.support.internalvoidenabled
            || defaults.surfacevarnish.enabled
            || defaults.outervarnish.enabled
            || defaults.outervarnish.thicknessmm != 0.0
            || defaults.enginerole != SliceEngineRole::LegacyProduction)
        {
            return fail("slice-settings-model Profile 默认值错误：" + expectation.id);
        }
    }

    const SliceSettingsState previousState = model.State();
    if (model.ApplyProfileDefaults(QStringLiteral("unknown_profile"))
        || model.State().profileid != previousState.profileid)
    {
        return fail("slice-settings-model 未知 Profile 不应改变状态。");
    }

    SliceSettingsState validState = model.State();
    validState.modelpath = QStringLiteral("model/obj/sample.obj");
    validState.outputdirectory = QStringLiteral("output/ui_sessions/sample/package");
    model.SetState(validState);
    if (!model.Validate().IsValid())
    {
        return fail("slice-settings-model 安全 legacy 设置未通过校验。");
    }

    SliceSettingsState candidateState = validState;
    candidateState.enginerole = SliceEngineRole::OpenVdbUtilityCandidate;
    model.SetState(candidateState);
    const SliceSettingsValidationResult candidateValidation = model.Validate();
    if (!candidateValidation.IsValid()
        || !candidateValidation.warnings.join(" ").contains("productionReplacementAllowed=false"))
    {
        return fail("slice-settings-model OpenVDB candidate 边界未固化。");
    }

    SliceSettingsState invalidState = validState;
    invalidState.layerthicknessmm = 0.0;
    invalidState.support.enabled = false;
    invalidState.support.internalvoidenabled = true;
    invalidState.outervarnish.enabled = true;
    invalidState.outervarnish.thicknessmm = 0.0;
    model.SetState(invalidState);
    if (model.Validate().IsValid())
    {
        return fail("slice-settings-model 非法设置未被阻断。");
    }

    return pass("slice-settings-model profiles=4 legacy-default=true openvdb=candidate-only");
}

int UiSmokeTestRunner::SettingHelpMetadataCase(const UiSmokeTestOptions& options)
{
    const QStringList requiredKeys{
        QStringLiteral("modelFill.material"),
        QStringLiteral("support.enabled"),
        QStringLiteral("support.placement"),
        QStringLiteral("support.internalVoid.enabled"),
        QStringLiteral("surfaceVarnish.enabled"),
        QStringLiteral("outerVarnish.enabled"),
        QStringLiteral("preview.enabled"),
        QStringLiteral("preview.interval"),
        QStringLiteral("engine.legacy"),
        QStringLiteral("engine.openvdbCandidate"),
    };

    const QVector<SettingHelpMetadata>& entries = HelpTextProvider::All();
    QSet<QString> seenKeys;
    for (const SettingHelpMetadata& entry : entries)
    {
        if (!entry.IsComplete())
        {
            return fail(QStringLiteral("setting-help-metadata 字段不完整：") + entry.key);
        }
        if (seenKeys.contains(entry.key))
        {
            return fail(QStringLiteral("setting-help-metadata 重复 key：") + entry.key);
        }
        seenKeys.insert(entry.key);

        if (!QFileInfo(QDir(options.repo_root).filePath(entry.docpath)).isFile())
        {
            return fail(QStringLiteral("setting-help-metadata 文档不存在：") + entry.docpath);
        }
        if (!ContainsAll(
                entry.ToolTipText(),
                {entry.title, entry.description, QStringLiteral("影响："),
                 QStringLiteral("默认："), QStringLiteral("生产安全："), entry.docpath}))
        {
            return fail(QStringLiteral("setting-help-metadata tooltip 字段缺失：") + entry.key);
        }
    }

    SettingHelpPanel helpPanel;
    for (const QString& key : requiredKeys)
    {
        const SettingHelpMetadata* metadata = HelpTextProvider::Find(key);
        if (metadata == nullptr || !helpPanel.SelectKey(key))
        {
            return fail(QStringLiteral("setting-help-metadata 缺少必需设置：") + key);
        }
        if (!ContainsAll(
                helpPanel.CurrentText(),
                {metadata->title, metadata->description, metadata->defaultvalue,
                 metadata->productionsafety, metadata->docpath}))
        {
            return fail(QStringLiteral("setting-help-metadata 说明面板字段缺失：") + key);
        }
    }

    ConfigDocument document;
    QuickConfigPanel quickPanel(&document);
    const QVector<QPair<QString, QString>> tooltipBindings{
        {QStringLiteral("modelFillMaterialCombo"), QStringLiteral("modelFill.material")},
        {QStringLiteral("supportEnabledCheck"), QStringLiteral("support.enabled")},
        {QStringLiteral("supportPlacementCombo"), QStringLiteral("support.placement")},
        {QStringLiteral("surfaceVarnishEnabledCheck"), QStringLiteral("surfaceVarnish.enabled")},
        {QStringLiteral("outerVarnishEnabledCheck"), QStringLiteral("outerVarnish.enabled")},
        {QStringLiteral("previewIntervalSpin"), QStringLiteral("preview.interval")},
        {QStringLiteral("openVdbCandidateCheck"), QStringLiteral("engine.openvdbCandidate")},
    };
    for (const QPair<QString, QString>& binding : tooltipBindings)
    {
        QWidget* widget = quickPanel.findChild<QWidget*>(binding.first);
        if (widget == nullptr || widget->toolTip() != HelpTextProvider::ToolTip(binding.second))
        {
            return fail(QStringLiteral("setting-help-metadata tooltip 未复用集中元数据：") + binding.first);
        }
    }

    const SettingHelpMetadata* openVdb = HelpTextProvider::Find(
        QStringLiteral("engine.openvdbCandidate"));
    if (openVdb == nullptr
        || !ContainsAll(
            openVdb->DetailText(),
            {QStringLiteral("关闭"), QStringLiteral("非生产"),
             QStringLiteral("productionReplacementAllowed=false")}))
    {
        return fail(QStringLiteral("setting-help-metadata OpenVDB 安全边界不完整。"));
    }

    return pass(QStringLiteral("setting-help-metadata entries=%1 required=%2 tooltips=%3")
                    .arg(entries.size())
                    .arg(requiredKeys.size())
                    .arg(tooltipBindings.size()));
}

int UiSmokeTestRunner::PreviewWorkspaceSharedLayer(const UiSmokeTestOptions& options)
{
    const QString packagePath = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(packagePath);
    if (package.manifest_path.isEmpty())
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 未找到 manifest：") + packagePath);
    }

    PreviewWorkspace workspace;
    workspace.LoadPackage(package);
    auto* production = workspace.findChild<LayerPreviewPanel*>(QStringLiteral("productionLayerView"));
    auto* overlay = workspace.findChild<PreviewOverlayPanel*>(QStringLiteral("materialOverlayView"));
    auto* raw = workspace.findChild<PreviewPanel*>(QStringLiteral("rawPreviewView"));
    if (production == nullptr || overlay == nullptr || raw == nullptr)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 未复用三个既有预览面板。"));
    }
    if (workspace.LayerIndices().isEmpty()
        || workspace.LayerIndices() != production->LayerIndices())
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 未优先使用生产真实层范围。"));
    }

    const QVector<int> previewLayers = overlay->LayerIndices();
    if (previewLayers.isEmpty())
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 夹具缺少 overlay preview 层。"));
    }
    const int previewLayer = previewLayers.first();
    if (!workspace.SelectLayer(previewLayer)
        || production->CurrentLayerIndex() != previewLayer
        || overlay->CurrentLayerIndex() != previewLayer
        || raw->CurrentLayerIndex() != previewLayer)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 有图层未同步到三个模式。"));
    }

    workspace.SetMode(PreviewWorkspaceMode::MaterialOverlay);
    workspace.SetMode(PreviewWorkspaceMode::RawPreview);
    workspace.SetMode(PreviewWorkspaceMode::ProductionLayer);
    if (workspace.CurrentLayerIndex() != previewLayer
        || production->CurrentLayerIndex() != previewLayer
        || overlay->CurrentLayerIndex() != previewLayer
        || raw->CurrentLayerIndex() != previewLayer)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 模式切换改变了真实层号。"));
    }

    QComboBox* rawChannel = raw->findChild<QComboBox*>(QStringLiteral("rawPreviewChannelSelector"));
    QComboBox* overlayMode = overlay->findChild<QComboBox*>(QStringLiteral("overlayModeSelector"));
    if (rawChannel == nullptr || overlayMode == nullptr)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 缺少既有视图模式控件。"));
    }
    rawChannel->setCurrentText(QStringLiteral("RGB"));
    overlayMode->setCurrentText(QStringLiteral("RGB + W 白墨"));

    int sparseLayer = -1;
    for (const int layerIndex : workspace.LayerIndices())
    {
        if (!raw->LayerIndices().contains(layerIndex))
        {
            sparseLayer = layerIndex;
            break;
        }
    }
    if (sparseLayer < 0 || !workspace.SelectLayer(sparseLayer))
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 夹具未覆盖稀疏 preview 缺层。"));
    }
    if (production->CurrentLayerIndex() != sparseLayer
        || overlay->CurrentLayerIndex() != sparseLayer
        || raw->CurrentLayerIndex() != sparseLayer)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 缺图层没有保持共享 layerIndex。"));
    }
    if (!ContainsAll(
            raw->StatusForTest(),
            {QStringLiteral("layer=%1").arg(sparseLayer), QStringLiteral("未跨层兜底")})
        || !ContainsAll(
            workspace.StatusForTest(),
            {QStringLiteral("共享 layer=%1").arg(sparseLayer), QStringLiteral("缺图不跨层兜底")}))
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 缺图状态未明确禁止跨层兜底。"));
    }

    int overlaySparseLayer = -1;
    for (const int layerIndex : workspace.LayerIndices())
    {
        workspace.SelectLayer(layerIndex);
        if (overlay->StatusForTest().contains(QStringLiteral("未跨层兜底")))
        {
            overlaySparseLayer = layerIndex;
            break;
        }
    }
    if (overlaySparseLayer < 0
        || !ContainsAll(
            overlay->StatusForTest(),
            {QStringLiteral("layer=%1").arg(overlaySparseLayer), QStringLiteral("未跨层兜底")}))
    {
        return fail(QStringLiteral("preview-workspace-shared-layer overlay 未覆盖同层材料缺失。"));
    }

    const int panelDrivenLayer = previewLayers.last();
    production->SelectLayer(panelDrivenLayer);
    if (workspace.CurrentLayerIndex() != panelDrivenLayer
        || overlay->CurrentLayerIndex() != panelDrivenLayer
        || raw->CurrentLayerIndex() != panelDrivenLayer)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer panel 信号未回写共享状态。"));
    }

    return pass(
        QStringLiteral("preview-workspace-shared-layer layers=%1 rawSparse=%2 overlaySparse=%3 preview=%4 modes=3")
            .arg(workspace.LayerIndices().size())
            .arg(sparseLayer)
            .arg(overlaySparseLayer)
            .arg(previewLayer));
}

int UiSmokeTestRunner::PreviewLegendProbeContext(const UiSmokeTestOptions& options)
{
    const QString packagePath = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(packagePath);
    if (package.manifest_path.isEmpty())
    {
        return fail(QStringLiteral("preview-legend-probe-context 未找到 manifest：") + packagePath);
    }

    PreviewWorkspace workspace;
    workspace.LoadPackage(package);
    auto* production = workspace.findChild<LayerPreviewPanel*>(QStringLiteral("productionLayerView"));
    auto* legendBar = workspace.findChild<QWidget*>(QStringLiteral("previewLegendBar"));
    auto* protocolHint = workspace.findChild<QLabel*>(QStringLiteral("previewProtocolHint"));
    auto* probeContext = workspace.findChild<QLabel*>(QStringLiteral("previewProbeContext"));
    if (production == nullptr || legendBar == nullptr || protocolHint == nullptr || probeContext == nullptr)
    {
        return fail(QStringLiteral("preview-legend-probe-context 缺少图例或探针上下文控件。"));
    }

    const QString legend = workspace.LegendTextForTest();
    if (!ContainsAll(
            legend,
            {QStringLiteral("RGB 模型颜色/填充"), QStringLiteral("W 白墨模型填充"),
             QStringLiteral("S 模型外支撑或内部镂空支撑"), QStringLiteral("V 光油或光油模型填充"),
             QStringLiteral("真实空白无材料"), QStringLiteral("black_is_print"),
             QStringLiteral("0=打印"), QStringLiteral("255=不打印"),
             QStringLiteral("显示颜色不等于生产值")})
        || !ContainsAll(
            protocolHint->text(),
            {QStringLiteral("RGBWSV uint8"), QStringLiteral("black_is_print"),
             QStringLiteral("0=打印"), QStringLiteral("255=不打印"),
             QStringLiteral("不等于 TIFF 生产值")}))
    {
        return fail(QStringLiteral("preview-legend-probe-context 图例或生产/显示值说明不完整。"));
    }

    const auto probeSemantic = [production](
                                   const QString& channel,
                                   const QString& expectedSemantic) -> QString
    {
        for (const int layerIndex : production->LayerIndices())
        {
            production->SelectLayer(layerIndex);
            if (!production->SelectChannelForTest(channel))
            {
                return {};
            }
            const QImage image = production->CurrentImageForTest();
            for (int y = 0; y < image.height(); ++y)
            {
                for (int x = 0; x < image.width(); ++x)
                {
                    const QColor color = image.pixelColor(x, y);
                    if (color.alpha() == 0
                        || (color.red() > 245 && color.green() > 245 && color.blue() > 245))
                    {
                        continue;
                    }
                    const QString context = production->ProbePixelForTest(x, y);
                    if (context.contains(expectedSemantic))
                    {
                        return context;
                    }
                }
            }
        }
        return {};
    };

    const QString rgbProbe = probeSemantic(QStringLiteral("rgb"), QStringLiteral("RGB模型颜色或填充"));
    const QString whiteProbe = probeSemantic(QStringLiteral("white"), QStringLiteral("白墨模型填充"));
    const QString supportProbe = probeSemantic(QStringLiteral("support"), QStringLiteral("支撑填充"));
    const QString varnishProbe = probeSemantic(QStringLiteral("varnish"), QStringLiteral("光油表面或外侧层"));
    if (rgbProbe.isEmpty() || whiteProbe.isEmpty() || supportProbe.isEmpty() || varnishProbe.isEmpty())
    {
        return fail(
            QStringLiteral("preview-legend-probe-context 未在真实 RGBWSV 夹具中识别完整 RGB/W/S/V 语义。"));
    }

    QString emptyProbe;
    production->SelectLayer(production->LayerIndices().first());
    production->SelectChannelForTest(QStringLiteral("support"));
    const QImage supportImage = production->CurrentImageForTest();
    for (int y = 0; y < supportImage.height() && emptyProbe.isEmpty(); ++y)
    {
        for (int x = 0; x < supportImage.width(); ++x)
        {
            const QColor color = supportImage.pixelColor(x, y);
            if (color.red() <= 245 || color.green() <= 245 || color.blue() <= 245)
            {
                continue;
            }
            const QString context = production->ProbePixelForTest(x, y);
            if (context.contains(QStringLiteral("材料语义=真实空白")))
            {
                emptyProbe = context;
                break;
            }
        }
    }
    if (emptyProbe.isEmpty())
    {
        return fail(QStringLiteral("preview-legend-probe-context 未在真实 RGBWSV 夹具中识别真实空白。"));
    }

    if (!ContainsAll(
            workspace.ProbeContextForTest(),
            {QStringLiteral("生产值 RGBWSV="), QStringLiteral("打印通道=无"),
             QStringLiteral("材料语义=真实空白"), QStringLiteral("black_is_print"),
             QStringLiteral("显示颜色=伪彩或真彩预览")})
        || workspace.ProbeContextForTest() != probeContext->text())
    {
        return fail(QStringLiteral("preview-legend-probe-context 六通道探针未同步到统一工作区。"));
    }

    return pass(
        QStringLiteral("preview-legend-probe-context legend=RGBWSV probes=RGB,W,S,V,Empty layers=%1")
            .arg(production->LayerIndices().size()));
}

int UiSmokeTestRunner::DiagnosticsCollapse(const UiSmokeTestOptions& options)
{
    const QString packagePath = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(packagePath);
    if (package.manifest_path.isEmpty())
    {
        return fail(QStringLiteral("diagnostics-collapse 未找到 manifest：") + packagePath);
    }

    MainWindow window(options.repo_root);
    auto* dock = window.findChild<DiagnosticsDock*>(QStringLiteral("diagnosticsDock"));
    auto* diagnosticsTabs = window.findChild<QTabWidget*>(QStringLiteral("diagnosticsTabs"));
    auto* workspaceTabs = window.findChild<QTabWidget*>(QStringLiteral("mainWorkspaceTabs"));
    auto* diagnosticsAction = window.findChild<QAction*>(QStringLiteral("diagnosticsToggleAction"));
    auto* preview = window.findChild<PreviewWorkspace*>(QStringLiteral("previewWorkspace"));
    if (dock == nullptr || diagnosticsTabs == nullptr || workspaceTabs == nullptr
        || diagnosticsAction == nullptr || preview == nullptr)
    {
        return fail(QStringLiteral("diagnostics-collapse 缺少 dock、菜单入口或中央工作区。"));
    }

    QStringList workspaceTitles;
    for (int index = 0; index < workspaceTabs->count(); ++index)
    {
        workspaceTitles.push_back(workspaceTabs->tabText(index));
    }
    if (workspaceTitles != QStringList{QStringLiteral("预览"), QStringLiteral("配置")})
    {
        return fail(QStringLiteral("diagnostics-collapse 中央页签仍包含历史报告/曲线入口：")
                    + workspaceTitles.join(QStringLiteral(",")));
    }
    if (dock->TabTitles()
        != QStringList{QStringLiteral("报告"), QStringLiteral("曲线"), QStringLiteral("日志")})
    {
        return fail(QStringLiteral("diagnostics-collapse 诊断页签集合不正确。"));
    }
    if (window.findChildren<ReportPanel*>().size() != 1
        || window.findChildren<ChannelChartPanel*>().size() != 1
        || window.findChildren<LogPanel*>().size() != 1)
    {
        return fail(QStringLiteral("diagnostics-collapse 存在重复诊断 panel 实例。"));
    }
    if (dock->IsExpanded() || !dock->isHidden() || diagnosticsAction->isChecked())
    {
        return fail(QStringLiteral("diagnostics-collapse 诊断区域未默认折叠。"));
    }

    dock->LoadPackage(package);
    preview->LoadPackage(package);
    if (dock->ChartView()->layerStatCount() <= 0 || preview->LayerIndices().isEmpty())
    {
        return fail(QStringLiteral("diagnostics-collapse 输出包未加载到曲线或预览。"));
    }
    const int layerIndex = preview->LayerIndices().last();
    preview->SelectLayer(layerIndex);

    dock->SetExpanded(true);
    QApplication::processEvents();
    if (!dock->IsExpanded() || dock->isHidden())
    {
        return fail(QStringLiteral("diagnostics-collapse 无法展开诊断区域。"));
    }
    dock->SetExpanded(false);
    QApplication::processEvents();
    if (dock->IsExpanded() || !dock->isHidden())
    {
        return fail(QStringLiteral("diagnostics-collapse 无法收起诊断区域。"));
    }
    if (preview->CurrentLayerIndex() != layerIndex)
    {
        return fail(QStringLiteral("diagnostics-collapse 折叠操作改变了预览真实 layerIndex。"));
    }

    return pass(
        QStringLiteral("diagnostics-collapse default=collapsed tabs=%1 workspace=%2 layer=%3")
            .arg(dock->TabTitles().join(QStringLiteral(",")))
            .arg(workspaceTitles.join(QStringLiteral(",")))
            .arg(layerIndex));
}

int UiSmokeTestRunner::OpenVdbUtilitySummary(const UiSmokeTestOptions& options)
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        return fail(QStringLiteral("openvdb-utility-summary 无法创建临时目录。"));
    }

    QDir packageDir(tempDir.filePath(QStringLiteral("package")));
    if (!packageDir.mkpath(QStringLiteral("reports")))
    {
        return fail(QStringLiteral("openvdb-utility-summary 无法创建 reports 目录。"));
    }
    const QString offPath = packageDir.filePath(QStringLiteral("reports/openvdb_utility_off.json"));
    const QString onPath = packageDir.filePath(QStringLiteral("reports/openvdb_utility_on.json"));
    QJsonObject badSchema = BuildOpenVdbUtilityReportFixture(true);
    badSchema[QStringLiteral("schema")] = QStringLiteral("slicesoft.openvdb_sdf_utility.99.1");
    const QString badSchemaPath = tempDir.filePath(QStringLiteral("openvdb_utility_bad_schema.json"));
    QJsonObject badReplacement = BuildOpenVdbUtilityReportFixture(true);
    QJsonObject badDecision = badReplacement.value(QStringLiteral("decision")).toObject();
    badDecision[QStringLiteral("productionReplacementAllowed")] = true;
    badReplacement[QStringLiteral("decision")] = badDecision;
    const QString badReplacementPath =
        tempDir.filePath(QStringLiteral("openvdb_utility_bad_replacement.json"));
    if (!WriteJsonFixture(offPath, BuildOpenVdbUtilityReportFixture(false))
        || !WriteJsonFixture(onPath, BuildOpenVdbUtilityReportFixture(true))
        || !WriteJsonFixture(badSchemaPath, badSchema)
        || !WriteJsonFixture(badReplacementPath, badReplacement))
    {
        return fail(QStringLiteral("openvdb-utility-summary 无法写入报告 fixture。"));
    }

    const PackageSummary package = PackageLoader().load(packageDir.path());
    if (!package.report_paths.contains(QFileInfo(offPath).absoluteFilePath())
        || !package.report_paths.contains(QFileInfo(onPath).absoluteFilePath()))
    {
        return fail(QStringLiteral("openvdb-utility-summary PackageLoader 未发现 ON/OFF 报告。"));
    }

    MainWindow window(options.repo_root);
    auto* dock = window.findChild<DiagnosticsDock*>(QStringLiteral("diagnosticsDock"));
    auto* preview = window.findChild<PreviewWorkspace*>(QStringLiteral("previewWorkspace"));
    if (dock == nullptr || preview == nullptr)
    {
        return fail(QStringLiteral("openvdb-utility-summary 缺少诊断区域或预览工作区。"));
    }
    ReportPanel* panel = dock->ReportView();
    panel->loadPackage(package);
    if (panel->ReportCount() != 2)
    {
        return fail(QStringLiteral("openvdb-utility-summary package 报告数量不正确。"));
    }

    const int layerIndexBefore = preview->CurrentLayerIndex();
    panel->LoadReportPath(offPath);
    const QString offSummary = panel->CurrentSummary();
    const JsonReport offReport = ReportLoader().load(offPath);
    const QString offWarnings =
        ReportLoader::collectWarningsAndFailures(offReport.document.object());
    if (!ContainsAll(
            offSummary,
            {QStringLiteral("报告角色: 当前不可用"),
             QStringLiteral("OpenVDB 编译/运行可用: 否 / 否"),
             QStringLiteral("生产替代许可: 否 (productionReplacementAllowed=false)"),
             QStringLiteral("默认生产路径: Legacy"),
             QStringLiteral("当前构建不可用"),
             QStringLiteral("use_openvdb_off")}))
    {
        return fail(QStringLiteral("openvdb-utility-summary OFF 摘要不完整：\n") + offSummary);
    }
    if (!offWarnings.contains(QStringLiteral("utilities.outerVarnishShell.blockers: use_openvdb_off"))
        || !offWarnings.contains(QStringLiteral("issues.use_openvdb_off")))
    {
        return fail(
            QStringLiteral("openvdb-utility-summary OFF blocker/issues 未进入警告上下文：\n")
            + offWarnings);
    }

    panel->LoadReportPath(onPath);
    const QString onSummary = panel->CurrentSummary();
    if (!ContainsAll(
            onSummary,
            {QStringLiteral("报告角色: OpenVDB SDF 辅助工具候选"),
             QStringLiteral("Utility 验证通过（非生产）"),
             QStringLiteral("建议推进为辅助 Utility"),
             QStringLiteral("保持实验能力"),
             QStringLiteral("生产结论: 仅 Utility 诊断，不形成生产验收结论"),
             QStringLiteral("默认生产路径: Legacy")})
        || onSummary.contains(QStringLiteral("生产验收：通过")))
    {
        return fail(QStringLiteral("openvdb-utility-summary ON 摘要越过非生产边界：\n") + onSummary);
    }

    panel->LoadReportPath(badSchemaPath);
    const QString badSchemaSummary = panel->CurrentSummary();
    if (!ContainsAll(
            badSchemaSummary,
            {QStringLiteral("报告状态: 无效，禁止作为生产证据"),
             QStringLiteral("安全要求: productionReplacementAllowed=false"),
             QStringLiteral("不支持的 OpenVDB Utility schema")}))
    {
        return fail(QStringLiteral("openvdb-utility-summary 未阻断错误 schema：\n") + badSchemaSummary);
    }

    panel->LoadReportPath(badReplacementPath);
    const QString badReplacementSummary = panel->CurrentSummary();
    if (!ContainsAll(
            badReplacementSummary,
            {QStringLiteral("报告状态: 无效，禁止作为生产证据"),
             QStringLiteral("decision.productionReplacementAllowed: 必须为 false")}))
    {
        return fail(
            QStringLiteral("openvdb-utility-summary 未阻断非法生产替代标志：\n")
            + badReplacementSummary);
    }
    if (panel->ReportCount() != 4 || preview->CurrentLayerIndex() != layerIndexBefore)
    {
        return fail(QStringLiteral("openvdb-utility-summary 独立报告加载破坏报告去重或预览层状态。"));
    }

    return pass(QStringLiteral("openvdb-utility-summary on=valid off=valid badSchema=blocked replacement=blocked"));
}

int UiSmokeTestRunner::WorkspaceLayoutSizes(const UiSmokeTestOptions& options)
{
    const QString packagePath = absoluteFromRepo(options, options.package_path);
    const PackageSummary package = PackageLoader().load(packagePath);
    if (package.manifest_path.isEmpty())
    {
        return fail(QStringLiteral("workspace-layout-sizes 未找到 manifest：") + packagePath);
    }

    MainWindow window(options.repo_root);
    auto* splitter = window.findChild<QSplitter*>(QStringLiteral("mainSplitter"));
    auto* projectPanel = window.findChild<QWidget*>(QStringLiteral("projectPanel"));
    auto* workspaceTabs = window.findChild<QTabWidget*>(QStringLiteral("mainWorkspaceTabs"));
    auto* rightPanel = window.findChild<QTabWidget*>(QStringLiteral("rightDiagnosticsPanel"));
    auto* preview = window.findChild<PreviewWorkspace*>(QStringLiteral("previewWorkspace"));
    auto* configPanel = window.findChild<ConfigEditorPanel*>();
    auto* dock = window.findChild<DiagnosticsDock*>(QStringLiteral("diagnosticsDock"));
    auto* diagnosticsAction = window.findChild<QAction*>(QStringLiteral("diagnosticsToggleAction"));
    if (splitter == nullptr || projectPanel == nullptr || workspaceTabs == nullptr || rightPanel == nullptr
        || preview == nullptr || configPanel == nullptr || dock == nullptr || diagnosticsAction == nullptr)
    {
        return fail(QStringLiteral("workspace-layout-sizes 缺少稳定布局对象。"));
    }

    preview->LoadPackage(package);
    dock->LoadPackage(package);
    if (preview->LayerIndices().isEmpty())
    {
        return fail(QStringLiteral("workspace-layout-sizes 输出包没有生产层。"));
    }
    const int layerIndex = preview->LayerIndices().last();
    preview->SelectLayer(layerIndex);

    window.show();
    QApplication::processEvents();
    const QList<QSize> targetSizes{
        QSize(1440, 900),
        QSize(1280, 720),
        QSize(1024, 768),
    };
    QStringList verifiedSizes;
    for (const QSize& targetSize : targetSizes)
    {
        dock->SetExpanded(false);
        window.resize(targetSize);
        QApplication::processEvents();

        if (window.width() > targetSize.width() || window.height() > targetSize.height())
        {
            return fail(
                QStringLiteral(
                    "workspace-layout-sizes 窗口被 minimumSizeHint 强制放大：requested=%1x%2 actual=%3x%4 "
                    "windowHint=%5x%6 projectHint=%7x%8 workspaceHint=%9x%10 rightHint=%11x%12 "
                    "previewHint=%13x%14 configHint=%15x%16")
                    .arg(targetSize.width())
                    .arg(targetSize.height())
                    .arg(window.width())
                    .arg(window.height())
                    .arg(window.minimumSizeHint().width())
                    .arg(window.minimumSizeHint().height())
                    .arg(projectPanel->minimumSizeHint().width())
                    .arg(projectPanel->minimumSizeHint().height())
                    .arg(workspaceTabs->minimumSizeHint().width())
                    .arg(workspaceTabs->minimumSizeHint().height())
                    .arg(rightPanel->minimumSizeHint().width())
                    .arg(rightPanel->minimumSizeHint().height())
                    .arg(preview->minimumSizeHint().width())
                    .arg(preview->minimumSizeHint().height())
                    .arg(configPanel->minimumSizeHint().width())
                    .arg(configPanel->minimumSizeHint().height()));
        }
        if (!projectPanel->isVisible() || !workspaceTabs->isVisible() || !rightPanel->isVisible())
        {
            return fail(QStringLiteral("workspace-layout-sizes 三列区域存在不可见项。"));
        }

        const QRect splitterRect = GlobalRect(splitter);
        const QRect projectRect = GlobalRect(projectPanel);
        const QRect workspaceRect = GlobalRect(workspaceTabs);
        const QRect rightRect = GlobalRect(rightPanel);
        if (projectRect.width() < 280 || workspaceRect.width() < 400 || rightRect.width() < 240)
        {
            return fail(
                QStringLiteral("workspace-layout-sizes 三列宽度低于冻结边界：%1/%2/%3")
                    .arg(projectRect.width())
                    .arg(workspaceRect.width())
                    .arg(rightRect.width()));
        }
        if (projectRect.intersects(workspaceRect) || projectRect.intersects(rightRect)
            || workspaceRect.intersects(rightRect))
        {
            return fail(QStringLiteral("workspace-layout-sizes 三列区域发生重叠。"));
        }
        if (!splitterRect.contains(projectRect) || !splitterRect.contains(workspaceRect)
            || !splitterRect.contains(rightRect))
        {
            return fail(QStringLiteral("workspace-layout-sizes 三列区域超出 mainSplitter。"));
        }
        if (dock->IsExpanded() || diagnosticsAction->isChecked())
        {
            return fail(QStringLiteral("workspace-layout-sizes 诊断区域未保持默认隐藏。"));
        }

        dock->SetExpanded(true);
        QApplication::processEvents();
        const QRect expandedWorkspaceRect = GlobalRect(workspaceTabs);
        const QRect dockRect = GlobalRect(dock);
        if (!dock->IsExpanded() || !diagnosticsAction->isChecked() || dockRect.height() <= 0
            || expandedWorkspaceRect.height() < 200 || expandedWorkspaceRect.intersects(dockRect))
        {
            return fail(QStringLiteral("workspace-layout-sizes 诊断区域展开后覆盖或压垮中央工作区。"));
        }
        dock->SetExpanded(false);
        QApplication::processEvents();
        if (preview->CurrentLayerIndex() != layerIndex)
        {
            return fail(QStringLiteral("workspace-layout-sizes resize/dock toggle 改变了真实 layerIndex。"));
        }

        verifiedSizes.push_back(
            QStringLiteral("%1x%2=%3/%4/%5")
                .arg(targetSize.width())
                .arg(targetSize.height())
                .arg(projectRect.width())
                .arg(workspaceRect.width())
                .arg(rightRect.width()));
    }

    window.hide();
    return pass(
        QStringLiteral("workspace-layout-sizes sizes=%1 layer=%2")
            .arg(verifiedSizes.join(QStringLiteral(",")))
            .arg(layerIndex));
}

int UiSmokeTestRunner::GeneratedEffectiveConfig(const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    QTemporaryDir tempdir;
    if (!tempdir.isValid())
    {
        return fail("generated-effective-config 无法创建临时目录。");
    }

    const QJsonArray channelorder{
        QStringLiteral("R"),
        QStringLiteral("G"),
        QStringLiteral("B"),
        QStringLiteral("W"),
        QStringLiteral("S"),
        QStringLiteral("V"),
    };
    QJsonObject root{
        {"input", QJsonObject{{"modelPath", "template.obj"}, {"format", "auto"}}},
        {"output",
         QJsonObject{{"packageDir", "output/template"},
                     {"layerThicknessMm", 0.01},
                     {"channelOrder", channelorder},
                     {"bitDepth", 8},
                     {"storageMode", "stripped"}}},
        {"background", QJsonObject{{"value", 255}}},
        {"modelFill",
         QJsonObject{{"enabled", true},
                     {"material", "white"},
                     {"scope", "below_texture_surface"},
                     {"value", 0},
                     {"emptyAllowedInProduction", false},
                     {"legacyRgbFallback", false}}},
        {"support",
         QJsonObject{{"enabled", true},
                     {"mode", "bottom_projection"},
                     {"placement", "lower"},
                     {"internalVoid",
                      QJsonObject{{"enabled", true},
                                  {"minAreaPx", 16},
                                  {"fillRule", "all_internal_voids"}}}}},
        {"surfaceVarnish",
         QJsonObject{{"enabled", false},
                     {"thicknessPx", 0},
                     {"outerSurface", true},
                     {"innerSurface", true},
                     {"value", 0},
                     {"source", "explicit"}}},
        {"outerVarnish",
         QJsonObject{{"enabled", false},
                     {"thicknessMm", 0.0},
                     {"thicknessStepMm", 0.01},
                     {"pixelPitchUm", 42.3},
                     {"allowXYExpansion", true},
                     {"conflictPolicy", "varnish_shell_wins"},
                     {"value", 0}}},
        {"preview", QJsonObject{{"enabled", true}, {"interval", 10}}},
        {"texture", QJsonObject{{"nonSurfaceRgbPolicy", "model_material"}}},
    };

    const QString templatepath = tempdir.filePath("profile.template.json");
    QFile templatefile(templatepath);
    if (!templatefile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        return fail("generated-effective-config 无法写入模板 fixture。");
    }
    const QByteArray templatebytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    templatefile.write(templatebytes);
    templatefile.close();

    ConfigDocument document;
    if (!document.load(templatepath))
    {
        return fail("generated-effective-config 无法加载模板 fixture。");
    }
    document.setValue({"texture", "nonSurfaceRgbPolicy"}, "empty");

    SliceSettingsModel settingsmodel;
    if (!settingsmodel.ApplyProfileDefaults(QStringLiteral("textured_nail_rgb_varnish_lower_support")))
    {
        return fail("generated-effective-config 无法应用 Profile 默认值。");
    }
    SliceSettingsState settings = settingsmodel.State();
    settings.modelpath = QStringLiteral("model/obj/nail.obj");
    settings.outputdirectory = tempdir.filePath("session/package");
    settings.layerthicknessmm = 0.02;
    settings.support.placement = SupportPlacement::Both;
    settings.support.internalvoidminareapx = 24;
    settings.surfacevarnish.enabled = true;
    settings.surfacevarnish.thicknesspx = 2;
    settings.outervarnish.enabled = true;
    settings.outervarnish.thicknessmm = 0.05;
    settings.preview.interval = 3;

    const QString generatedpath = tempdir.filePath("session/slice_config.generated.json");
    EffectiveConfigRequest request;
    request.profileid = settings.profileid;
    request.templatepath = templatepath;
    request.generatedconfigpath = generatedpath;
    request.originaldocument = document.originalDocument();
    request.overridedocument = document.document();
    request.settings = settings;

    const EffectiveConfigResult result = EffectiveConfigGenerator().Generate(request);
    if (!result.IsValid())
    {
        return fail("generated-effective-config 生成失败：" + result.errors.join("; "));
    }
    if (!QFileInfo::exists(generatedpath))
    {
        return fail("generated-effective-config 未写入 session 配置。");
    }

    QFile unchangedtemplate(templatepath);
    if (!unchangedtemplate.open(QIODevice::ReadOnly | QIODevice::Text)
        || unchangedtemplate.readAll() != templatebytes)
    {
        return fail("generated-effective-config 修改了原始模板。");
    }

    const QJsonObject generated = result.document.object();
    const QJsonObject output = generated.value("output").toObject();
    const QJsonObject support = generated.value("support").toObject();
    const QJsonObject openvdb = generated.value("experimental").toObject().value("openvdbPipeline").toObject();
    if (generated.value("input").toObject().value("modelPath").toString() != settings.modelpath
        || output.value("packageDir").toString() != settings.outputdirectory
        || output.value("layerThicknessMm").toDouble() != settings.layerthicknessmm
        || output.value("channelOrder").toArray() != channelorder
        || output.value("bitDepth").toInt() != 8
        || generated.value("background").toObject().value("value").toInt() != 255
        || generated.value("modelFill").toObject().value("material").toString() != "varnish"
        || support.value("placement").toString() != "both"
        || support.value("internalVoid").toObject().value("minAreaPx").toInt() != 24
        || !generated.value("surfaceVarnish").toObject().value("enabled").toBool()
        || generated.value("surfaceVarnish").toObject().value("thicknessPx").toInt() != 2
        || !generated.value("outerVarnish").toObject().value("enabled").toBool()
        || generated.value("outerVarnish").toObject().value("thicknessMm").toDouble() != 0.05
        || generated.value("preview").toObject().value("interval").toInt() != 3
        || generated.value("texture").toObject().value("nonSurfaceRgbPolicy").toString() != "empty"
        || openvdb.value("enabled").toBool(true)
        || openvdb.value("writeProductionRgbwsv").toBool(true))
    {
        return fail("generated-effective-config 未完整合成 Profile、dirty UI override 或安全边界。");
    }
    if (!result.summary.contains(settings.profileid)
        || result.differences.isEmpty())
    {
        return fail("generated-effective-config 未提供生效摘要或差异。");
    }
    ConfigDocument viewdocument;
    if (!viewdocument.load(templatepath))
    {
        return fail("generated-effective-config 无法加载 UI 展示文档。");
    }
    ConfigEditorPanel configpanel(&viewdocument);
    configpanel.ShowEffectiveConfig(result);
    if (!configpanel.EffectiveConfigText().contains(settings.profileid)
        || !configpanel.EffectiveConfigText().contains(QStringLiteral("modelFill.material")))
    {
        return fail("generated-effective-config UI 未显示生效摘要和差异。");
    }

    EffectiveConfigRequest invalidrequest = request;
    invalidrequest.generatedconfigpath = tempdir.filePath("invalid/slice_config.generated.json");
    invalidrequest.settings.outervarnish.enabled = true;
    invalidrequest.settings.outervarnish.thicknessmm = 0.0;
    const EffectiveConfigResult invalidresult = EffectiveConfigGenerator().Generate(invalidrequest);
    if (invalidresult.IsValid() || QFileInfo::exists(invalidrequest.generatedconfigpath))
    {
        return fail("generated-effective-config 非法设置未在写文件前阻断。");
    }

    EffectiveConfigRequest protocolrequest = request;
    protocolrequest.generatedconfigpath = tempdir.filePath("bad_protocol/slice_config.generated.json");
    QJsonObject badprotocolroot = protocolrequest.overridedocument.object();
    QJsonObject badprotocoloutput = badprotocolroot.value("output").toObject();
    badprotocoloutput.insert("bitDepth", 16);
    badprotocolroot.insert("output", badprotocoloutput);
    protocolrequest.overridedocument = QJsonDocument(badprotocolroot);
    const EffectiveConfigResult protocolresult = EffectiveConfigGenerator().Generate(protocolrequest);
    if (protocolresult.IsValid() || QFileInfo::exists(protocolrequest.generatedconfigpath))
    {
        return fail("generated-effective-config 未阻断 RGBWSV 固定协议偏差。");
    }

    return pass(QString("generated-effective-config differences=%1 template-readonly=true")
                    .arg(result.differences.size()));
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
