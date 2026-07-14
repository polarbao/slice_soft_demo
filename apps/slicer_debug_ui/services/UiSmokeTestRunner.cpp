#include "UiSmokeTestRunner.h"

#include "../MainWindow.h"
#include "../widgets/ChannelChartPanel.h"
#include "../widgets/ConfigEditorPanel.h"
#include "../widgets/LayerPreviewPanel.h"
#include "../widgets/PreviewOverlayPanel.h"
#include "ConfigDocument.h"
#include "EffectiveConfigGenerator.h"
#include "PackageLoader.h"
#include "PreviewReportIndex.h"
#include "ReportLoader.h"
#include "ScenarioRegistry.h"
#include "SliceSettingsModel.h"
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
    if (options.case_name == "slice-settings-model") {
        return sliceSettingsModel(options);
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
