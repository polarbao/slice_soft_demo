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

int UiSmokeTestRunner::MaterialClosureDiagnostics(const UiSmokeTestOptions& options)
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        return fail(QStringLiteral("material-closure-diagnostics 无法创建临时目录。"));
    }

    const auto writePackage =
        [&tempDir](
            const QString& name,
            const QJsonObject& report,
            const bool includeReport,
            const bool includeGapPreview) -> QString
    {
        QDir packageDir(tempDir.filePath(name));
        if (!packageDir.mkpath(QStringLiteral("reports"))
            || !packageDir.mkpath(QStringLiteral("preview")))
        {
            return {};
        }

        QJsonArray layers;
        layers.append(
            QJsonObject{
                {QStringLiteral("index"), 0},
                {QStringLiteral("zMm"), 0.0}});
        layers.append(
            QJsonObject{
                {QStringLiteral("index"), 7},
                {QStringLiteral("zMm"), 0.07}});
        QJsonObject manifest;
        manifest[QStringLiteral("schema")] = QStringLiteral("p0.rgbwsv.2");
        manifest[QStringLiteral("grid")] =
            QJsonObject{
                {QStringLiteral("widthPx"), 12},
                {QStringLiteral("heightPx"), 12},
                {QStringLiteral("layerCount"), 8},
                {QStringLiteral("layerThicknessMm"), 0.01}};
        manifest[QStringLiteral("layers")] = layers;
        if (!WriteJsonFixture(
                packageDir.filePath(QStringLiteral("manifest.json")),
                manifest))
        {
            return {};
        }

        if (includeReport
            && !WriteJsonFixture(
                packageDir.filePath(
                    QStringLiteral("reports/material_closure_report.json")),
                report))
        {
            return {};
        }

        if (includeGapPreview)
        {
            QImage image(12, 12, QImage::Format_ARGB32);
            image.fill(Qt::white);
            for (int coordinate{3}; coordinate <= 8; ++coordinate)
            {
                image.setPixelColor(coordinate, coordinate, QColor(255, 0, 255));
            }
            if (!image.save(
                    packageDir.filePath(
                        QStringLiteral(
                            "preview/material_closure_gap_000007.png"))))
            {
                return {};
            }
        }
        return packageDir.absolutePath();
    };

    const QString gapPreviewPath =
        QStringLiteral("preview/material_closure_gap_000007.png");
    const QString exactPassPackage = writePackage(
        QStringLiteral("exact_pass"),
        BuildMaterialClosureReportFixture(
            QStringLiteral("exact"),
            QStringLiteral("pass"),
            QStringLiteral("passed"),
            0,
            0,
            0,
            {}),
        true,
        false);
    const QString exactFailPackage = writePackage(
        QStringLiteral("exact_fail"),
        BuildMaterialClosureReportFixture(
            QStringLiteral("exact"),
            QStringLiteral("fail"),
            QStringLiteral("failed"),
            5,
            0,
            5,
            gapPreviewPath),
        true,
        true);
    const QString repairedPackage = writePackage(
        QStringLiteral("repaired_with_remaining"),
        BuildMaterialClosureReportFixture(
            QStringLiteral("exact"),
            QStringLiteral("fail"),
            QStringLiteral("failed"),
            5,
            3,
            2,
            {}),
        true,
        false);
    const QString candidatePackage = writePackage(
        QStringLiteral("candidate_only"),
        BuildMaterialClosureReportFixture(
            QStringLiteral("candidate"),
            QStringLiteral("warning"),
            QStringLiteral("not_evaluated"),
            5,
            0,
            5,
            {}),
        true,
        false);
    const QString missingPackage = writePackage(
        QStringLiteral("report_missing"),
        {},
        false,
        false);
    if (exactPassPackage.isEmpty()
        || exactFailPackage.isEmpty()
        || repairedPackage.isEmpty()
        || candidatePackage.isEmpty()
        || missingPackage.isEmpty())
    {
        return fail(QStringLiteral("material-closure-diagnostics 无法写入报告夹具。"));
    }

    MainWindow window(options.repo_root);
    auto* dock = window.findChild<DiagnosticsDock*>(
        QStringLiteral("diagnosticsDock"));
    auto* panel = window.findChild<MaterialClosurePanel*>(
        QStringLiteral("materialClosurePanel"));
    auto* workspace = window.findChild<PreviewWorkspace*>(
        QStringLiteral("previewWorkspace"));
    auto* overlay = window.findChild<PreviewOverlayPanel*>(
        QStringLiteral("materialOverlayView"));
    if (dock == nullptr || panel == nullptr || workspace == nullptr || overlay == nullptr)
    {
        return fail(
            QStringLiteral(
                "material-closure-diagnostics 缺少闭环面板或统一预览。"));
    }

    const auto loadFixture =
        [dock, workspace](const QString& path) -> PackageSummary
    {
        const PackageSummary package = PackageLoader().load(path);
        dock->LoadPackage(package);
        workspace->LoadPackage(package);
        return package;
    };

    loadFixture(exactPassPackage);
    if (!ContainsAll(
            panel->SummaryForTest(),
            {QStringLiteral("闭环状态：通过 (pass)"),
             QStringLiteral("证据置信度：精确语义证据 (exact)"),
             QStringLiteral("生产验收：通过 (passed)"),
             QStringLiteral("Worst Layers：0")})
        || panel->WorstLayerCountForTest() != 0)
    {
        return fail(
            QStringLiteral("material-closure-diagnostics exact pass 显示不完整：\n")
            + panel->SummaryForTest());
    }

    loadFixture(exactFailPackage);
    if (!ContainsAll(
            panel->SummaryForTest(),
            {QStringLiteral("闭环状态：失败 (fail)"),
             QStringLiteral("生产验收：未通过 (failed)"),
             QStringLiteral("颜色/填充=1"),
             QStringLiteral("模型/支撑=1"),
             QStringLiteral("颜色/支撑=1"),
             QStringLiteral("内部镂空=1"),
             QStringLiteral("光油/支撑=1"),
             QStringLiteral("外部背景保护像素：128"),
             QStringLiteral("Worst Layers：1")})
        || panel->WorstLayerCountForTest() != 1
        || !panel->SelectWorstLayerForTest(0)
        || !panel->TriggerSelectedLayerForTest())
    {
        return fail(
            QStringLiteral("material-closure-diagnostics exact fail 或定位入口无效：\n")
            + panel->SummaryForTest());
    }
    QApplication::processEvents();
    if (workspace->CurrentLayerIndex() != 7
        || workspace->CurrentMode()
            != PreviewWorkspaceMode::Diagnostic
        || workspace->CurrentDiagnosticMode()
            != DiagnosticPreviewMode::MaterialOverlay
        || !overlay->StatusForTest().contains(QStringLiteral("RGB + 闭环 Gap")))
    {
        return fail(
            QStringLiteral(
                "material-closure-diagnostics worst layer 未跳转真实 layerIndex 或未显示 Gap 伪彩图：")
            + overlay->StatusForTest());
    }

    loadFixture(repairedPackage);
    if (!ContainsAll(
            panel->SummaryForTest(),
            {QStringLiteral("修复：启用=是"),
             QStringLiteral("已尝试=是"),
             QStringLiteral("已修复=3"),
             QStringLiteral("剩余 Gap=2")}))
    {
        return fail(
            QStringLiteral(
                "material-closure-diagnostics repaired-with-remaining 显示不完整：\n")
            + panel->SummaryForTest());
    }

    loadFixture(candidatePackage);
    if (!ContainsAll(
            panel->SummaryForTest(),
            {QStringLiteral("证据置信度：候选推断 (candidate)"),
             QStringLiteral("生产验收：未评估 (not_evaluated)"),
             QStringLiteral("候选诊断，不能作为生产通过依据")}))
    {
        return fail(
            QStringLiteral("material-closure-diagnostics candidate 安全提示缺失：\n")
            + panel->SummaryForTest());
    }

    loadFixture(missingPackage);
    if (!panel->SummaryForTest().contains(
            QStringLiteral(
                "当前输出包未生成 reports/material_closure_report.json"))
        || panel->WorstLayerCountForTest() != 0)
    {
        return fail(
            QStringLiteral("material-closure-diagnostics report-missing 状态不明确：\n")
            + panel->SummaryForTest());
    }

    return pass(
        QStringLiteral(
            "material-closure-diagnostics exactPass/exactFail/repaired/candidate/missing + layer=7 gapPreview"));
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
