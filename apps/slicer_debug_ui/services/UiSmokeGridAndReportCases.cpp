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

int UiSmokeTestRunner::SceneGridLayout(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    QWidget workspace;
    SceneDocument document;
    SceneLayoutPanel panel(&document, &workspace);

    slicer_core::ModelInstance first;
    first.instanceid = "layout-instance-1";
    first.modelid = "layout-model";
    first.sourcetransformidentity = "layout-source";
    first.sourcebboxmm = {
        {0.0, 0.0, 0.0},
        {8.0, 4.0, 1.0}};
    first.effectivebboxmm = first.sourcebboxmm;
    document.SetLoading(1U, QStringLiteral("layout.obj"));
    if (!document.SetSceneContext(
            1U,
            QStringLiteral("layout-scene"),
            1U,
            QStringLiteral("layout-cache"),
            QStringLiteral("layout-source-hash"),
            QStringLiteral("layout-resource-hash"),
            first))
    {
        return fail(
            QStringLiteral("scene-grid-layout first context failed"));
    }

    slicer_core::SceneViewGeometry geometry;
    geometry.sceneid = "layout-scene";
    geometry.modelid = first.modelid;
    geometry.instanceid = first.instanceid;
    geometry.scenerevision = 1U;
    geometry.sourcebboxmm = first.sourcebboxmm;
    geometry.effectivebboxmm = first.effectivebboxmm;
    geometry.worldboundsmm = {{0.0, 0.0}, {8.0, 4.0}};
    geometry.triangles.push_back(
        {{0.0, 0.0}, {8.0, 0.0}, {0.0, 4.0}});
    geometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;
    if (!document.SetGeometry(1U, geometry))
    {
        return fail(
            QStringLiteral("scene-grid-layout first geometry failed"));
    }

    for (int index = 2; index <= 12; ++index)
    {
        const SceneDocumentOperationResult duplicated =
            document.DuplicateInstance(
                document.CurrentInstanceId(),
                QStringLiteral("layout-instance-%1").arg(index),
                document.SceneRevision());
        if (!duplicated.IsValid())
        {
            return fail(
                QStringLiteral(
                    "scene-grid-layout duplicate %1 failed")
                    .arg(index));
        }
    }

    auto* columns = panel.findChild<QSpinBox*>(
        QStringLiteral("sceneLayoutColumnCountSpin"));
    auto* rows = panel.findChild<QSpinBox*>(
        QStringLiteral("sceneLayoutRowCountSpin"));
    auto* columnGap = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("sceneLayoutColumnGapSpin"));
    auto* rowGap = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("sceneLayoutRowGapSpin"));
    auto* apply = panel.findChild<QPushButton*>(
        QStringLiteral("sceneLayoutApplyButton"));
    auto* restore = panel.findChild<QPushButton*>(
        QStringLiteral("sceneLayoutRestoreButton"));
    if (columns == nullptr
        || rows == nullptr
        || columnGap == nullptr
        || rowGap == nullptr
        || apply == nullptr
        || restore == nullptr)
    {
        return fail(
            QStringLiteral("scene-grid-layout controls missing"));
    }

    columns->setValue(11);
    rows->setValue(1);
    const quint64 capacityRevision = document.SceneRevision();
    apply->click();
    if (document.SceneRevision() != capacityRevision)
    {
        return fail(
            QStringLiteral(
                "scene-grid-layout capacity failure mutated scene"));
    }

    rows->setValue(2);
    columnGap->setValue(20.0);
    rowGap->setValue(30.0);
    apply->click();
    if (document.Items().at(10U).layoutrow != 0
        || document.Items().at(10U).layoutcolumn != 10
        || document.Items().at(11U).layoutrow != 1
        || document.Items().at(11U).layoutcolumn != 0
        || std::abs(
            document.Items().at(10U)
                .instance.effectivebboxmm.min.x
            - 280.0)
            > 1.0e-9
        || std::abs(
            document.Items().at(11U)
                .instance.effectivebboxmm.min.y
            - 34.0)
            > 1.0e-9
        || !restore->isEnabled())
    {
        return fail(
            QStringLiteral(
                "scene-grid-layout row-major placement mismatch"));
    }

    restore->click();
    if (document.CanRestoreGridLayout()
        || std::abs(
            document.Items().at(11U)
                .instance.effectivebboxmm.min.y)
            > 1.0e-9)
    {
        return fail(
            QStringLiteral("scene-grid-layout restore mismatch"));
    }

    MainWindow window(options.repo_root);
    auto* integratedPanel = window.findChild<SceneLayoutPanel*>(
        QStringLiteral("sceneLayoutPanel"));
    auto* inspector = window.findChild<ContextInspector*>(
        QStringLiteral("contextInspector"));
    auto* inspectorTabs = window.findChild<QTabWidget*>(
        QStringLiteral("contextInspectorTabs"));
    if (integratedPanel == nullptr
        || inspector == nullptr
        || inspectorTabs == nullptr
        || !inspector->PageTitles().contains(
            QStringLiteral("排版")))
    {
        return fail(
            QStringLiteral(
                "scene-grid-layout workspace integration missing"));
    }

    const QList<QSize> sizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    for (const QSize& size : sizes)
    {
        window.resize(size);
        window.show();
        inspectorTabs->setCurrentWidget(integratedPanel);
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        if (!integratedPanel->isVisible()
            || integratedPanel->width() < 200
            || inspector->geometry().right()
                >= window.centralWidget()->width())
        {
            return fail(
                QStringLiteral(
                    "scene-grid-layout overlap at %1x%2")
                    .arg(size.width())
                    .arg(size.height()));
        }
    }

    return pass(QStringLiteral(
        "scene-grid-layout capacity/11x2/edge-gap/restore/"
        "workspace/three-window-sizes"));
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
