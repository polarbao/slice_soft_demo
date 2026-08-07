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
    LayerPreviewPanel panel;
    panel.LoadPackage(package);
    if (panel.LayerCount() <= 0) {
        return fail("layer-preview-load 未读取到 layerCount。");
    }

    const QStringList channels = panel.AvailableChannels();
    const QStringList required_channels{
        QStringLiteral("rgb"),
        QStringLiteral("red"),
        QStringLiteral("green"),
        QStringLiteral("blue"),
        QStringLiteral("white"),
        QStringLiteral("support"),
        QStringLiteral("varnish"),
        QStringLiteral("rgb_support_white_varnish"),
        QStringLiteral("occupancy"),
        QStringLiteral("empty"),
    };
    for (const QString& channel : required_channels) {
        if (!channels.contains(channel)) {
            return fail("layer-preview-load 缺少通道：" + channel + "，实际通道：" + channels.join(","));
        }
    }
    const QVector<int> layerIndices = panel.LayerIndices();
    const QList<int> layer_indices{
        layerIndices.first(),
        layerIndices.at(layerIndices.size() / 2),
        layerIndices.last(),
    };
    for (const int layer_index : layer_indices) {
        if (!panel.SelectLayerForTest(layer_index)) {
            return fail("layer-preview-load 无法选择层：" + QString::number(layer_index));
        }
        if (!WaitForCondition(
                [&panel, layer_index]()
                {
                    return panel.IsLayerReadyForTest()
                        && panel.LoadedLayerIndexForTest()
                            == layer_index;
                }))
        {
            return fail(
                QStringLiteral(
                    "layer-preview-load TIFF 异步读取超时：layer=%1 status=%2")
                    .arg(layer_index)
                    .arg(panel.StatusForTest()));
        }
        for (const QString& channel : required_channels) {
            if (!panel.SelectChannelForTest(channel)) {
                return fail("layer-preview-load 无法选择通道：" + channel);
            }
            const QImage image = panel.CurrentImageForTest();
            if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
                return fail(QString("layer-preview-load 渲染为空：layer=%1 channel=%2").arg(layer_index).arg(channel));
            }
        }
    }

    if (channels.contains(QStringLiteral("rgb")))
    {
        if (!panel.SelectChannelForTest(QStringLiteral("rgb")))
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
    const ToolPaths paths = ToolPaths::FromRepoRoot(options.repo_root);
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
    if (registry.DefaultScenarioId()
            != QStringLiteral("textured_nail_rgb_only_lower_support")
        || registry.FindById(registry.DefaultScenarioId()) == nullptr)
    {
        return fail(
            "scenario-registry 默认 Profile 必须是全实体 RGB、无白墨。");
    }
    QStringList expectedNormalIds{
        "production_rgb_inspection",
        "single_material_relief",
        "textured_nail_rgb_only_lower_support",
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
    QJsonObject defaultProfile;
    if (!ReadJsonObject(
            QDir(options.repo_root).filePath(defaultScenario->configpath),
            &defaultProfile))
    {
        return fail("scenario-registry 无法读取默认 Profile 配置。");
    }
    const QJsonObject defaultOutput =
        defaultProfile.value(QStringLiteral("output")).toObject();
    if (defaultOutput.value(QStringLiteral("dpiX")).toInt()
            != slicer_core::kDefaultOutputDpiX
        || defaultOutput.value(QStringLiteral("dpiY")).toInt()
            != slicer_core::kDefaultOutputDpiY
        || std::abs(
               defaultOutput.value(
                   QStringLiteral("layerThicknessMm")).toDouble()
               - slicer_core::kDefaultLayerThicknessMm)
            > 1.0e-9
        || defaultProfile
               .value(QStringLiteral("modelFill"))
               .toObject()
               .value(QStringLiteral("material"))
               .toString()
            != QStringLiteral("rgb"))
    {
        return fail(
            "scenario-registry 默认 Profile 的 635x600 DPI、0.038 mm "
            "层高或全 RGB 材料不正确。");
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
