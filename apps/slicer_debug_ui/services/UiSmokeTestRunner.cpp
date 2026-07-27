#include "UiSmokeTestRunner.h"

#include "../MainWindow.h"
#include "../controllers/SceneTransformController.h"
#include "../models/SceneDocument.h"
#include "../models/SceneSelectionModel.h"
#include "../widgets/ChannelChartPanel.h"
#include "../widgets/ConfigEditorPanel.h"
#include "../widgets/DiagnosticsDock.h"
#include "../widgets/LayerPreviewPanel.h"
#include "../widgets/MaterialClosurePanel.h"
#include "../widgets/ModelListPanel.h"
#include "../widgets/ModelPreflightPanel.h"
#include "../widgets/SceneLayoutPanel.h"
#include "../widgets/ModelTransformPanel.h"
#include "../widgets/ModelTopViewWidget.h"
#include "../widgets/PreviewOverlayPanel.h"
#include "../widgets/PreviewPanel.h"
#include "../widgets/ProductionModePanel.h"
#include "../widgets/QuickConfigPanel.h"
#include "../widgets/ReportPanel.h"
#include "../widgets/SettingHelpPanel.h"
#include "../widgets/SliceTimingPanel.h"
#include "../widgets/PreviewWorkspace.h"
#include "ConfigDocument.h"
#include "EffectiveConfigGenerator.h"
#include "HelpTextProvider.h"
#include "ModelPreflightController.h"
#include "ModelPreflightPresenter.h"
#include "ModelTopViewLoader.h"
#include "PackageLoader.h"
#include "PreviewReportIndex.h"
#include "ReportLoader.h"
#include "ScenarioRegistry.h"
#include "SceneModelRepository.h"
#include "SliceSettingsModel.h"
#include "SliceProgressProtocolParser.h"
#include "SlicePreflightCoordinator.h"
#include "ToolPaths.h"
#include "TransformedModelPreflightLoader.h"
#include "slicer_core/config.h"

#include <QComboBox>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QRect>
#include <QSet>
#include <QSize>
#include <QSplitter>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QThread>
#include <QToolButton>

#include <cmath>
#include <functional>

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

QJsonObject BuildMaterialClosureReportFixture(
    const QString& confidence,
    const QString& closureStatus,
    const QString& productionAcceptance,
    const int totalGapPixels,
    const int repairedPixels,
    const int remainingGapPixels,
    const QString& gapPreviewPath)
{
    const bool candidateOnly = confidence == QStringLiteral("candidate");
    const bool hasRemainingGap = remainingGapPixels > 0;

    QJsonObject repair;
    repair[QStringLiteral("enabled")] = repairedPixels > 0;
    repair[QStringLiteral("maxGapPx")] = 1;
    repair[QStringLiteral("attempted")] = repairedPixels > 0;
    repair[QStringLiteral("repairedPixels")] = repairedPixels;

    QJsonObject totals;
    totals[QStringLiteral("layerCount")] = 10;
    totals[QStringLiteral("evaluatedLayerCount")] = 10;
    totals[QStringLiteral("passLayerCount")] = hasRemainingGap ? 9 : 10;
    totals[QStringLiteral("warningLayerCount")] =
        closureStatus == QStringLiteral("warning") ? 1 : 0;
    totals[QStringLiteral("failLayerCount")] =
        closureStatus == QStringLiteral("fail") ? 1 : 0;
    totals[QStringLiteral("totalGapPixels")] = totalGapPixels;
    totals[QStringLiteral("colorFillGapPixels")] = totalGapPixels > 0 ? 1 : 0;
    totals[QStringLiteral("modelSupportGapPixels")] = totalGapPixels > 1 ? 1 : 0;
    totals[QStringLiteral("colorSupportGapPixels")] = totalGapPixels > 2 ? 1 : 0;
    totals[QStringLiteral("internalVoidGapPixels")] = totalGapPixels > 3 ? 1 : 0;
    totals[QStringLiteral("varnishSupportGapPixels")] = totalGapPixels > 4 ? 1 : 0;
    totals[QStringLiteral("repairedPixels")] = repairedPixels;
    totals[QStringLiteral("remainingGapPixels")] = remainingGapPixels;
    totals[QStringLiteral("repairRejectedTooWidePixels")] = 0;
    totals[QStringLiteral("externalBackgroundProtectedPixels")] = 128;

    QJsonArray types;
    if (hasRemainingGap)
    {
        types.append(QStringLiteral("COLOR_FILL_GAP"));
        types.append(QStringLiteral("INTERNAL_VOID_GAP"));
    }

    QJsonArray worstLayers;
    if (hasRemainingGap)
    {
        worstLayers.append(
            QJsonObject{
                {QStringLiteral("layerIndex"), 7},
                {QStringLiteral("zMm"), 0.07},
                {QStringLiteral("gapPixels"), remainingGapPixels},
                {QStringLiteral("types"), types}});
    }

    QJsonObject layerRepair;
    layerRepair[QStringLiteral("attempted")] = repairedPixels > 0;
    layerRepair[QStringLiteral("repairedPixels")] = repairedPixels;
    layerRepair[QStringLiteral("repairedColorFillPixels")] = repairedPixels;
    layerRepair[QStringLiteral("repairedModelSupportPixels")] = 0;
    layerRepair[QStringLiteral("repairedInternalVoidPixels")] = 0;
    layerRepair[QStringLiteral("repairedVarnishSupportPixels")] = 0;
    layerRepair[QStringLiteral("remainingGapPixels")] = remainingGapPixels;
    layerRepair[QStringLiteral("rejectedTooWidePixels")] = 0;

    QJsonArray layers;
    layers.append(
        QJsonObject{
            {QStringLiteral("layerIndex"), 7},
            {QStringLiteral("zMm"), 0.07},
            {QStringLiteral("closureStatus"), closureStatus},
            {QStringLiteral("gapPixels"), totalGapPixels},
            {QStringLiteral("repair"), layerRepair},
            {QStringLiteral("externalBackgroundProtectedPixels"), 32},
            {QStringLiteral("gapPreviewPath"), gapPreviewPath}});

    QJsonArray diagnostics;
    if (candidateOnly)
    {
        diagnostics.append(
            QJsonObject{
                {QStringLiteral("severity"), QStringLiteral("warning")},
                {QStringLiteral("code"), QStringLiteral("MATERIAL_CLOSURE_CANDIDATE_ONLY")}});
    }
    if (hasRemainingGap)
    {
        diagnostics.append(
            QJsonObject{
                {QStringLiteral("severity"), QStringLiteral("error")},
                {QStringLiteral("code"), QStringLiteral("COLOR_FILL_GAP")},
                {QStringLiteral("layerIndex"), 7},
                {QStringLiteral("pixelCount"), remainingGapPixels}});
    }

    QJsonObject root;
    root[QStringLiteral("schema")] = QStringLiteral("p0.material_closure.1");
    root[QStringLiteral("packageProtocol")] = QStringLiteral("p0.rgbwsv.2");
    root[QStringLiteral("enabled")] = true;
    root[QStringLiteral("mode")] =
        repairedPixels > 0
        ? QStringLiteral("repair_then_report")
        : QStringLiteral("diagnostic");
    root[QStringLiteral("source")] =
        candidateOnly
        ? QStringLiteral("rgbwsv_tiff_inferred")
        : QStringLiteral("semantic_masks");
    root[QStringLiteral("confidence")] = confidence;
    root[QStringLiteral("closureStatus")] = closureStatus;
    root[QStringLiteral("productionAcceptance")] = productionAcceptance;
    root[QStringLiteral("repair")] = repair;
    root[QStringLiteral("totals")] = totals;
    root[QStringLiteral("worstLayers")] = worstLayers;
    root[QStringLiteral("layers")] = layers;
    root[QStringLiteral("diagnostics")] = diagnostics;
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

QByteArray ClosedBoxObjFixture()
{
    return QByteArrayLiteral(
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "v 0 0 1\n"
        "v 1 0 1\n"
        "v 1 1 1\n"
        "v 0 1 1\n"
        "f 1 3 2\n"
        "f 1 4 3\n"
        "f 5 6 7\n"
        "f 5 7 8\n"
        "f 1 2 6\n"
        "f 1 6 5\n"
        "f 2 3 7\n"
        "f 2 7 6\n"
        "f 3 4 8\n"
        "f 3 8 7\n"
        "f 4 1 5\n"
        "f 4 5 8\n");
}

QByteArray OpenTriangleObjFixture()
{
    return QByteArrayLiteral(
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n");
}

QString WritePreflightFixture(
    const QString& directory,
    const QString& name,
    const QByteArray& modelContent)
{
    const QDir dir(directory);
    const QString modelPath = dir.filePath(name + QStringLiteral(".obj"));
    QFile modelFile(modelPath);
    if (!modelFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return {};
    }
    modelFile.write(modelContent);
    modelFile.close();

    const QString configPath = dir.filePath(name + QStringLiteral(".json"));
    const QJsonObject config{
        {QStringLiteral("input"),
         QJsonObject{{QStringLiteral("modelPath"),
                      QDir::fromNativeSeparators(modelPath)},
                     {QStringLiteral("format"), QStringLiteral("obj")}}},
        {QStringLiteral("modelTransform"),
         QJsonObject{{QStringLiteral("unit"), QStringLiteral("mm")},
                     {QStringLiteral("scale"), QJsonArray{1.0, 1.0, 1.0}},
                     {QStringLiteral("rotationDeg"), QJsonArray{0.0, 0.0, 0.0}},
                     {QStringLiteral("translationMm"), QJsonArray{0.0, 0.0, 0.0}}}},
        {QStringLiteral("texture"),
         QJsonObject{{QStringLiteral("missingTexturePolicy"),
                      QStringLiteral("fail_fast")}}},
    };
    return WriteJsonFixture(configPath, config) ? configPath : QString{};
}

bool WaitForCondition(
    const std::function<bool()>& condition,
    const int timeoutMs = 30000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs)
    {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    QApplication::processEvents(QEventLoop::AllEvents, 20);
    return condition();
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
    if (options.case_name == "preview-physical-aspect")
    {
        return PreviewPhysicalAspect(options);
    }
    if (options.case_name == "diagnostics-collapse")
    {
        return DiagnosticsCollapse(options);
    }
    if (options.case_name == "material-closure-diagnostics")
    {
        return MaterialClosureDiagnostics(options);
    }
    if (options.case_name == "openvdb-utility-summary")
    {
        return OpenVdbUtilitySummary(options);
    }
    if (options.case_name == "workspace-layout-sizes")
    {
        return WorkspaceLayoutSizes(options);
    }
    if (options.case_name == "production-mode-selector")
    {
        return ProductionModeSelector(options);
    }
    if (options.case_name == "generated-effective-config") {
        return GeneratedEffectiveConfig(options);
    }
    if (options.case_name == "slice-progress-timing")
    {
        return SliceProgressTiming(options);
    }
    if (options.case_name == "model-preflight-states")
    {
        return ModelPreflightStates(options);
    }
    if (options.case_name == "model-preflight-one-click-gate")
    {
        return ModelPreflightOneClickGate(options);
    }
    if (options.case_name == "model-preflight-lifecycle")
    {
        return ModelPreflightLifecycle(options);
    }
    if (options.case_name == "model-top-view")
    {
        return ModelTopView(options);
    }
    if (options.case_name == "model-top-view-transform")
    {
        return ModelTopViewTransform(options);
    }
    if (options.case_name == "model-transform-preflight")
    {
        return ModelTransformPreflight(options);
    }
    if (options.case_name == "multi-model-list")
    {
        return MultiModelList(options);
    }
    if (options.case_name == "scene-grid-layout")
    {
        return SceneGridLayout(options);
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
    if (registry.DefaultScenarioId().isEmpty() || registry.FindById(registry.DefaultScenarioId()) == nullptr)
    {
        return fail("scenario-registry defaultScenarioId 无效。");
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
        {QStringLiteral("textured_nail_rgb_only_lower_support"), ModelFillMaterial::Rgb, 10},
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
            || defaults.dpix != slicer_core::kDefaultOutputDpiX
            || defaults.dpiy != slicer_core::kDefaultOutputDpiY
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

    SliceSettingsState invalidDpiState = validState;
    invalidDpiState.dpix = slicer_core::kMaximumOutputDpi + 1;
    model.SetState(invalidDpiState);
    if (model.Validate().IsValid())
    {
        return fail("slice-settings-model 非法 X/Y DPI 未被阻断。");
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

    return pass("slice-settings-model profiles=5 legacy-default=true openvdb=candidate-only");
}

int UiSmokeTestRunner::SettingHelpMetadataCase(const UiSmokeTestOptions& options)
{
    const QStringList requiredKeys{
        QStringLiteral("output.dpiX"),
        QStringLiteral("output.dpiY"),
        QStringLiteral("modelTransform.scale"),
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

    QTemporaryDir configTempDir;
    if (!configTempDir.isValid())
    {
        return fail(QStringLiteral("setting-help-metadata 无法创建配置测试目录。"));
    }
    const QString configPath = configTempDir.filePath(QStringLiteral("quick-config.json"));
    const QJsonObject quickConfigFixture{
        {QStringLiteral("input"), QJsonObject{{QStringLiteral("modelPath"), QStringLiteral("fixture.obj")}}},
        {QStringLiteral("output"), QJsonObject{{QStringLiteral("packageDir"), QStringLiteral("fixture-package")}}},
        {QStringLiteral("modelTransform"),
         QJsonObject{
             {QStringLiteral("scale"), QJsonArray{0.8, 0.8, 0.8}}}},
        {QStringLiteral("materialPolicy"),
         QJsonObject{
             {QStringLiteral("enabled"), false},
             {QStringLiteral("white"),
              QJsonObject{
                  {QStringLiteral("enabled"), false},
                  {QStringLiteral("mode"), QStringLiteral("disabled")}}}}},
        {QStringLiteral("materialProcessProfile"), QJsonObject{}},
    };
    if (!WriteJsonFixture(configPath, quickConfigFixture))
    {
        return fail(QStringLiteral("setting-help-metadata 无法写入配置测试夹具。"));
    }

    ConfigDocument document;
    if (!document.load(configPath))
    {
        return fail(QStringLiteral("setting-help-metadata 无法加载配置测试夹具。"));
    }
    QuickConfigPanel quickPanel(&document);
    quickPanel.LoadFromDocument();
    const QVector<QPair<QString, QString>> tooltipBindings{
        {QStringLiteral("modelScaleXSpin"), QStringLiteral("modelTransform.scale")},
        {QStringLiteral("outputDpiXSpin"), QStringLiteral("output.dpiX")},
        {QStringLiteral("outputDpiYSpin"), QStringLiteral("output.dpiY")},
        {QStringLiteral("modelFillMaterialCombo"), QStringLiteral("modelFill.material")},
        {QStringLiteral("whitePolicyEnabledCheck"), QStringLiteral("materialPolicy.white.enabled")},
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

    QDoubleSpinBox* modelScaleXSpin = quickPanel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelScaleXSpin"));
    QDoubleSpinBox* modelScaleYSpin = quickPanel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelScaleYSpin"));
    QDoubleSpinBox* modelScaleZSpin = quickPanel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelScaleZSpin"));
    QPushButton* resetModelScaleButton = quickPanel.findChild<QPushButton*>(
        QStringLiteral("resetModelScaleButton"));
    QSpinBox* outputDpiXSpin = quickPanel.findChild<QSpinBox*>(
        QStringLiteral("outputDpiXSpin"));
    QSpinBox* outputDpiYSpin = quickPanel.findChild<QSpinBox*>(
        QStringLiteral("outputDpiYSpin"));
    QLabel* outputPixelSizeLabel = quickPanel.findChild<QLabel*>(
        QStringLiteral("outputPixelSizeLabel"));
    if (modelScaleXSpin == nullptr
        || modelScaleYSpin == nullptr
        || modelScaleZSpin == nullptr
        || resetModelScaleButton == nullptr
        || modelScaleXSpin->value() != 0.8
        || modelScaleYSpin->value() != 0.8
        || modelScaleZSpin->value() != 0.8)
    {
        return fail(QStringLiteral("setting-help-metadata 模型缩放控件未正确加载配置值。"));
    }
    if (outputDpiXSpin == nullptr
        || outputDpiYSpin == nullptr
        || outputPixelSizeLabel == nullptr
        || outputDpiXSpin->minimum() != 72
        || outputDpiXSpin->maximum() != 2400
        || outputDpiYSpin->minimum() != 72
        || outputDpiYSpin->maximum() != 2400
        || outputDpiXSpin->value() != 635
        || outputDpiYSpin->value() != 600
        || !ContainsAll(
            outputPixelSizeLabel->text(),
            {QStringLiteral("X 0.040000 mm/px"),
             QStringLiteral("Y 0.042333 mm/px")}))
    {
        return fail(QStringLiteral("setting-help-metadata X/Y DPI 默认值或物理像素提示错误。"));
    }
    outputDpiXSpin->setValue(600);
    outputDpiYSpin->setValue(1200);
    if (document.value({QStringLiteral("output"), QStringLiteral("dpiX")}).toInt() != 600
        || document.value({QStringLiteral("output"), QStringLiteral("dpiY")}).toInt() != 1200
        || !ContainsAll(
            outputPixelSizeLabel->text(),
            {QStringLiteral("X 0.042333 mm/px"),
             QStringLiteral("Y 0.021167 mm/px")}))
    {
        return fail(QStringLiteral("setting-help-metadata X/Y DPI 修改未写入配置或刷新物理像素。"));
    }
    if (!document.save(nullptr, SaveOptions{true}))
    {
        return fail(QStringLiteral("setting-help-metadata X/Y DPI 配置保存失败。"));
    }
    ConfigDocument reloadedDocument;
    if (!reloadedDocument.load(configPath)
        || reloadedDocument.value(
               {QStringLiteral("output"), QStringLiteral("dpiX")}).toInt()
            != 600
        || reloadedDocument.value(
               {QStringLiteral("output"), QStringLiteral("dpiY")}).toInt()
            != 1200)
    {
        return fail(QStringLiteral("setting-help-metadata X/Y DPI 未能独立保存并重新加载。"));
    }
    document.setValue(
        {QStringLiteral("output"), QStringLiteral("dpiX")},
        slicer_core::kMaximumOutputDpi + 1);
    if (document.validate().isValid())
    {
        return fail(QStringLiteral("setting-help-metadata 超范围 DPI 未被配置校验阻断。"));
    }
    document.setValue(
        {QStringLiteral("output"), QStringLiteral("dpiX")},
        600);
    resetModelScaleButton->click();
    const QJsonArray resetScale = document.value(
        {QStringLiteral("modelTransform"), QStringLiteral("scale")}).toArray();
    if (resetScale != QJsonArray{1.0, 1.0, 1.0})
    {
        return fail(QStringLiteral("setting-help-metadata 模型缩放未恢复为 1:1。"));
    }

    QCheckBox* whitePolicyCheck = quickPanel.findChild<QCheckBox*>(
        QStringLiteral("whitePolicyEnabledCheck"));
    if (whitePolicyCheck == nullptr)
    {
        return fail(QStringLiteral("setting-help-metadata 未找到白墨叠加策略控件。"));
    }
    whitePolicyCheck->setChecked(true);
    if (!document.value({QStringLiteral("materialPolicy"), QStringLiteral("enabled")}).toBool()
        || !document.value(
                {QStringLiteral("materialPolicy"), QStringLiteral("white"), QStringLiteral("enabled")})
                .toBool()
        || document.value(
               {QStringLiteral("materialPolicy"), QStringLiteral("white"), QStringLiteral("mode")})
               .toString()
            != QStringLiteral("all_model")
        || document.value(
               {QStringLiteral("materialProcessProfile"), QStringLiteral("white"), QStringLiteral("mode")})
               .toString()
            != QStringLiteral("all_model")
        || !document.value(
                {QStringLiteral("materialProcessProfile"), QStringLiteral("validation"),
                 QStringLiteral("requireWhitePixels")})
                .toBool())
    {
        return fail(QStringLiteral("setting-help-metadata 白墨叠加开关未写入完整材料策略。"));
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

int UiSmokeTestRunner::PreviewPhysicalAspect(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
    {
        return fail(QStringLiteral(
            "preview-physical-aspect 无法创建临时目录。"));
    }

    const auto createPackage =
        [&temporaryDirectory](
            const QString& name,
            const bool includePhysicalScale) -> PackageSummary
    {
        const QDir root(temporaryDirectory.path());
        const QString packagePath = root.filePath(name);
        const QDir packageDirectory(packagePath);
        if (!QDir().mkpath(packageDirectory.filePath(
                QStringLiteral("preview"))))
        {
            return {};
        }

        QImage image(QSize(100, 100), QImage::Format_ARGB32);
        image.fill(QColor(32, 96, 160));
        if (!image.save(
                packageDirectory.filePath(
                    QStringLiteral("preview/rgb_layer_000000.png"))))
        {
            return {};
        }

        QJsonObject grid{
            {QStringLiteral("widthPx"), 100},
            {QStringLiteral("heightPx"), 100},
            {QStringLiteral("layerCount"), 1},
            {QStringLiteral("layerThicknessMm"), 0.01},
        };
        if (includePhysicalScale)
        {
            grid.insert(QStringLiteral("dpiX"), 635);
            grid.insert(QStringLiteral("dpiY"), 600);
            grid.insert(
                QStringLiteral("pixelSizeXmm"),
                25.4 / 635.0);
            grid.insert(
                QStringLiteral("pixelSizeYmm"),
                25.4 / 600.0);
        }

        const QJsonObject manifest{
            {QStringLiteral("schema"),
             QStringLiteral("p0.rgbwsv.2")},
            {QStringLiteral("grid"), grid},
            {QStringLiteral("layers"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("index"), 0},
                     {QStringLiteral("zMm"), 0.0}}}},
        };
        if (!WriteJsonFixture(
                packageDirectory.filePath(
                    QStringLiteral("manifest.json")),
                manifest))
        {
            return {};
        }
        return PackageLoader().load(packagePath);
    };

    const PackageSummary physicalPackage =
        createPackage(QStringLiteral("physical"), true);
    if (physicalPackage.manifest_path.isEmpty())
    {
        return fail(QStringLiteral(
            "preview-physical-aspect 无法创建非等方夹具。"));
    }

    LayerPreviewPanel layerPanel;
    layerPanel.LoadPackage(physicalPackage);
    if (!layerPanel.SelectChannelForTest(QStringLiteral("rgb"))
        || layerPanel.PhysicalDisplaySizeForTest() != QSize(94, 100)
        || !ContainsAll(
            layerPanel.StatusForTest(),
            {QStringLiteral("DPI=635x600"),
             QStringLiteral("像素=0.040000x0.042333 mm")}))
    {
        return fail(
            QStringLiteral(
                "preview-physical-aspect 生产层未按 635/600 校正：")
            + layerPanel.StatusForTest());
    }

    PreviewOverlayPanel overlayPanel;
    overlayPanel.loadPackage(physicalPackage);
    if (overlayPanel.PhysicalDisplaySizeForTest() != QSize(94, 100)
        || !ContainsAll(
            overlayPanel.StatusForTest(),
            {QStringLiteral("DPI=635x600"),
             QStringLiteral("像素=0.040000x0.042333 mm")}))
    {
        return fail(
            QStringLiteral(
                "preview-physical-aspect 叠加层未按 635/600 校正：")
            + overlayPanel.StatusForTest());
    }

    const PackageSummary fallbackPackage =
        createPackage(QStringLiteral("fallback"), false);
    LayerPreviewPanel fallbackLayerPanel;
    fallbackLayerPanel.LoadPackage(fallbackPackage);
    fallbackLayerPanel.SelectChannelForTest(QStringLiteral("rgb"));
    PreviewOverlayPanel fallbackOverlayPanel;
    fallbackOverlayPanel.loadPackage(fallbackPackage);
    const QString fallbackText =
        QStringLiteral("缺少 grid 物理像素元数据，按方形像素显示");
    if (fallbackLayerPanel.PhysicalDisplaySizeForTest()
            != QSize(100, 100)
        || fallbackOverlayPanel.PhysicalDisplaySizeForTest()
            != QSize(100, 100)
        || !fallbackLayerPanel.StatusForTest().contains(fallbackText)
        || !fallbackOverlayPanel.StatusForTest().contains(fallbackText))
    {
        return fail(QStringLiteral(
            "preview-physical-aspect 缺失元数据时未明确降级。"));
    }

    return pass(QStringLiteral(
        "preview-physical-aspect corrected=94x100 fallback=100x100"));
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
        != QStringList{
            QStringLiteral("报告"),
            QStringLiteral("材料闭环"),
            QStringLiteral("曲线"),
            QStringLiteral("日志")})
    {
        return fail(QStringLiteral("diagnostics-collapse 诊断页签集合不正确。"));
    }
    if (window.findChildren<ReportPanel*>().size() != 1
        || window.findChildren<MaterialClosurePanel*>().size() != 1
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
        || workspace->CurrentMode() != PreviewWorkspaceMode::MaterialOverlay
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

int UiSmokeTestRunner::ProductionModeSelector(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    auto* workspaceTabs =
        window.findChild<QTabWidget*>(QStringLiteral("mainWorkspaceTabs"));
    auto* modePanel =
        window.findChild<ProductionModePanel*>(QStringLiteral("productionModePanel"));
    auto* modeCombo =
        window.findChild<QComboBox*>(QStringLiteral("productionModeCombo"));
    auto* profileCombo =
        window.findChild<QComboBox*>(QStringLiteral("productionProfileCombo"));
    auto* capabilityLabel =
        window.findChild<QLabel*>(QStringLiteral("productionCapabilityLabel"));
    auto* admissionLabel =
        window.findChild<QLabel*>(QStringLiteral("productionAdmissionLabel"));
    auto* resourceLabel =
        window.findChild<QLabel*>(QStringLiteral("productionResourceLabel"));
    auto* resultIdentityLabel =
        window.findChild<QLabel*>(
            QStringLiteral("productionResultIdentityLabel"));
    auto* resultOutputLabel =
        window.findChild<QLabel*>(
            QStringLiteral("productionResultOutputLabel"));
    auto* resultResourceLabel =
        window.findChild<QLabel*>(
            QStringLiteral("productionResultResourceLabel"));
    auto* supportCheck =
        window.findChild<QCheckBox*>(QStringLiteral("supportEnabledCheck"));
    auto* surfaceVarnishCheck =
        window.findChild<QCheckBox*>(QStringLiteral("surfaceVarnishEnabledCheck"));
    auto* openVdbCheck =
        window.findChild<QCheckBox*>(QStringLiteral("openVdbCandidateCheck"));
    if (workspaceTabs == nullptr || modePanel == nullptr || modeCombo == nullptr
        || profileCombo == nullptr || capabilityLabel == nullptr
        || admissionLabel == nullptr || resourceLabel == nullptr
        || resultIdentityLabel == nullptr || resultOutputLabel == nullptr
        || resultResourceLabel == nullptr
        || supportCheck == nullptr || surfaceVarnishCheck == nullptr
        || openVdbCheck == nullptr)
    {
        return fail(QStringLiteral("production-mode-selector 缺少稳定 UI 对象。"));
    }

    if (modeCombo->currentData().toString() != QStringLiteral("legacy")
        || modeCombo->currentText() != QStringLiteral("传统切片")
        || profileCombo->isEnabled()
        || !supportCheck->isEnabled()
        || !surfaceVarnishCheck->isEnabled())
    {
        return fail(QStringLiteral("production-mode-selector 未保持传统切片默认和能力透传。"));
    }
    if (!openVdbCheck->isHidden())
    {
        return fail(QStringLiteral("production-mode-selector 普通配置页仍暴露 OpenVDB backend 开关。"));
    }

    const int globalIndex =
        modeCombo->findData(QStringLiteral("global_surface_shell"));
    if (globalIndex < 0)
    {
        return fail(QStringLiteral("production-mode-selector 缺少全局纹理壳层模式。"));
    }
    modeCombo->setCurrentIndex(globalIndex);
    QApplication::processEvents();
    if (!profileCombo->isEnabled()
        || profileCombo->count() != 2
        || profileCombo->currentData().toString()
            != QStringLiteral("global_surface_shell_restricted_candidate")
        || supportCheck->isEnabled()
        || surfaceVarnishCheck->isEnabled()
        || !supportCheck->toolTip().contains(QStringLiteral("不支持 S 支撑"))
        || !surfaceVarnishCheck->toolTip().contains(QStringLiteral("不支持 V 光油"))
        || !resourceLabel->text().contains(QStringLiteral("高资源开销"))
        || !admissionLabel->text().contains(QStringLiteral("需要重新执行")))
    {
        return fail(QStringLiteral("production-mode-selector restricted Profile 能力锁定或状态提示错误。"));
    }

    const int parityIndex = profileCombo->findData(
        QStringLiteral("global_surface_shell_material_parity_candidate"));
    profileCombo->setCurrentIndex(parityIndex);
    QApplication::processEvents();
    if (parityIndex < 0
        || !capabilityLabel->text().contains(QStringLiteral("内部镂空支撑"))
        || supportCheck->isEnabled()
        || surfaceVarnishCheck->isEnabled()
        || !supportCheck->toolTip().contains(QStringLiteral("Profile 已锁定"))
        || !surfaceVarnishCheck->toolTip().contains(QStringLiteral("Profile 已锁定")))
    {
        return fail(QStringLiteral("production-mode-selector material-parity Profile 能力锁定错误。"));
    }

    ProductionModeUiDto result;
    result.requestedmode =
        slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    result.effectivemode =
        slicer_core::SlicePipelineMode::GlobalSurfaceShell;
    result.productionoutputwritten = true;
    result.fallbackapplied = false;
    result.resourcecost = ProductionResourceCostLevel::High;
    result.measuredtotalms = 1525.0;
    result.measuredpeakworkingsetbytes = 96U * 1024U * 1024U;
    result.sessionid = "ui-smoke-session";
    result.packagepath = "output/ui-smoke/package";
    modePanel->ShowProductionResult(result);
    if (!resultIdentityLabel->text().contains(QStringLiteral("全局纹理壳层"))
        || !resultIdentityLabel->text().contains(QStringLiteral("ui-smoke-session"))
        || !resultOutputLabel->text().contains(QStringLiteral("TIFF=已写入"))
        || !resultOutputLabel->text().contains(QStringLiteral("fallback=否"))
        || !resultResourceLabel->text().contains(QStringLiteral("1.52 s"))
        || !resultResourceLabel->text().contains(QStringLiteral("96.0 MiB"))
        || !resultResourceLabel->text().contains(QStringLiteral("高开销")))
    {
        return fail(QStringLiteral("production-mode-selector 未显示当前生产结果与实际资源。"));
    }

    window.show();
    workspaceTabs->setCurrentIndex(1);
    QApplication::processEvents();
    const QList<QSize> targetSizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    QStringList verifiedSizes;
    for (const QSize& targetSize : targetSizes)
    {
        window.resize(targetSize);
        QApplication::processEvents();
        if (window.width() > targetSize.width()
            || window.height() > targetSize.height()
            || !modePanel->isVisible()
            || modeCombo->width() < modeCombo->minimumSizeHint().width()
            || profileCombo->width() < profileCombo->minimumSizeHint().width())
        {
            return fail(
                QStringLiteral(
                    "production-mode-selector 中文文本在 %1x%2 被截断或模式面板不可见。")
                    .arg(targetSize.width())
                    .arg(targetSize.height()));
        }
        verifiedSizes.push_back(
            QStringLiteral("%1x%2").arg(targetSize.width()).arg(targetSize.height()));
    }
    window.hide();

    return pass(
        QStringLiteral(
            "production-mode-selector default=legacy profiles=2 backendHidden=true sizes=%1")
            .arg(verifiedSizes.join(QStringLiteral(","))));
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
        {"texture",
         QJsonObject{{"enabled", true},
                     {"applyMode", "solid_volume_from_top_surface"},
                     {"topSurfaceLayers", 8},
                     {"nonSurfaceRgbPolicy", "model_material"}}},
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
    settings.dpix = 635;
    settings.dpiy = 600;
    settings.layerthicknessmm = 0.02;
    settings.support.placement = SupportPlacement::Both;
    settings.support.internalvoidminareapx = 24;
    settings.surfacevarnish.enabled = true;
    settings.surfacevarnish.thicknesspx = 2;
    settings.outervarnish.enabled = true;
    settings.outervarnish.thicknessmm = 0.05;
    settings.preview.interval = 3;

    const QString generatedpath = tempdir.filePath("session/slice_config.effective.json");
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
    const QJsonObject texture = generated.value("texture").toObject();
    const QJsonObject openvdb = generated.value("experimental").toObject().value("openvdbPipeline").toObject();
    if (generated.value("input").toObject().value("modelPath").toString() != settings.modelpath
        || output.value("packageDir").toString() != settings.outputdirectory
        || output.value("dpiX").toInt() != settings.dpix
        || output.value("dpiY").toInt() != settings.dpiy
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
        || texture.value("applyMode").toString() != "top_surface_band"
        || texture.value("topSurfaceLayers").toInt() != 1
        || texture.value("nonSurfaceRgbPolicy").toString() != "empty"
        || openvdb.value("enabled").toBool(true)
        || openvdb.value("writeProductionRgbwsv").toBool(true))
    {
        return fail("generated-effective-config 未完整合成 Profile、dirty UI override 或安全边界。");
    }
    if (!result.warnings.join(QStringLiteral(" ")).contains(QStringLiteral("模型内部填充")))
    {
        return fail("generated-effective-config 未说明实体纹理投影与模型内部填充的自动纠正。");
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

    EffectiveConfigRequest rgbOnlyRequest = request;
    rgbOnlyRequest.profileid = QStringLiteral("textured_nail_rgb_only_lower_support");
    rgbOnlyRequest.generatedconfigpath = tempdir.filePath("rgb-only/slice_config.effective.json");
    rgbOnlyRequest.settings.profileid = rgbOnlyRequest.profileid;
    rgbOnlyRequest.settings.outputdirectory = tempdir.filePath("rgb-only/package");
    rgbOnlyRequest.settings.modelfillmaterial = ModelFillMaterial::Rgb;
    const EffectiveConfigResult rgbOnlyResult = EffectiveConfigGenerator().Generate(rgbOnlyRequest);
    const QJsonObject rgbOnlyRoot = rgbOnlyResult.document.object();
    if (!rgbOnlyResult.IsValid()
        || rgbOnlyRoot.value("modelFill").toObject().value("material").toString() != "rgb"
        || rgbOnlyRoot.value("texture").toObject().value("applyMode").toString()
            != "solid_volume_from_top_surface"
        || rgbOnlyResult.warnings.join(QStringLiteral(" ")).contains(QStringLiteral("已改为 1 层顶面纹理带")))
    {
        return fail("generated-effective-config 全实体 RGB Profile 被错误改写为白墨/光油填充或顶面纹理带。");
    }

    EffectiveConfigRequest invalidrequest = request;
    invalidrequest.generatedconfigpath = tempdir.filePath("invalid/slice_config.effective.json");
    invalidrequest.settings.outervarnish.enabled = true;
    invalidrequest.settings.outervarnish.thicknessmm = 0.0;
    const EffectiveConfigResult invalidresult = EffectiveConfigGenerator().Generate(invalidrequest);
    if (invalidresult.IsValid() || QFileInfo::exists(invalidrequest.generatedconfigpath))
    {
        return fail("generated-effective-config 非法设置未在写文件前阻断。");
    }

    EffectiveConfigRequest protocolrequest = request;
    protocolrequest.generatedconfigpath = tempdir.filePath("bad_protocol/slice_config.effective.json");
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

int UiSmokeTestRunner::SliceProgressTiming(const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    SliceProgressProtocolParser parser;
    SliceProtocolUpdate update = parser.Append(
        QStringLiteral("ordinary log\nSLICE_PROGRESS phase=layer_processing current=7 total=20 percent=55 elap"));
    if (!update.progress.isEmpty() || !update.timings.isEmpty())
    {
        return fail(QStringLiteral("切片进度协议不应解析未完成的行。"));
    }

    update = parser.Append(QStringLiteral(
        "sedMs=1234.500\n"
        "SLICE_TIMING engine=legacy profileLevel=detailed configLoadMs=1.000 modelLoadMs=20.000 "
        "sliceProcessingMs=800.000 layerComputeMs=500.000 tiffWriteMs=200.000 previewWriteMs=100.000 "
        "reportBuildMs=30.000 reportWriteMs=40.000 packagePublishMs=0.000 outputWriteMs=340.000 totalMs=1191.000 "
        "memoryAvailable=1 workingSetBytes=50331648 peakWorkingSetBytes=100663296\n"));
    if (update.progress.size() != 1 || update.timings.size() != 1)
    {
        return fail(QStringLiteral("切片进度协议事件数量错误。"));
    }
    const SliceProgressEvent progress = update.progress.front();
    const SliceTimingEvent timing = update.timings.front();
    if (progress.phase != QStringLiteral("layer_processing")
        || progress.current != 7
        || progress.total != 20
        || progress.percent != 55)
    {
        return fail(QStringLiteral("切片进度字段解析错误。"));
    }
    if (timing.engine != QStringLiteral("legacy")
        || qAbs(timing.sliceprocessingms - 800.0) > 0.001
        || qAbs(timing.outputwritems - 340.0) > 0.001
        || qAbs(timing.totalms - 1191.0) > 0.001
        || !timing.memoryavailable
        || timing.peakworkingsetbytes != 100663296U)
    {
        return fail(QStringLiteral("切片耗时字段解析错误。"));
    }

    SliceTimingPanel panel;
    panel.Reset(QStringLiteral("运行切片"));
    panel.UpdateProgress(progress);
    panel.ShowTiming(timing);
    panel.Finish(true, 1250);
    const QString summary = panel.SummaryText();
    if (!summary.contains(QStringLiteral("传统切片引擎"))
        || !summary.contains(QStringLiteral("800.0 ms"))
        || !summary.contains(QStringLiteral("340.0 ms")))
    {
        return fail(QStringLiteral("切片耗时面板未显示解析后的数据：") + summary);
    }
    return pass(QStringLiteral("切片进度协议与耗时面板通过。"));
}

int UiSmokeTestRunner::ModelPreflightStates(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    slicer_core::ModelPreflightExecutionResult execution;
    execution.generation = 7U;
    execution.fastComplete = true;
    execution.fullComplete = true;
    execution.result.status = slicer_core::ModelPreflightStatus::Warning;
    execution.result.legacyAdmission.status =
        slicer_core::ModelPreflightAdmissionStatus::Warning;
    execution.result.legacyAdmission.warningCodes = {
        "MESH_BOUNDARY_EDGES"};
    slicer_core::ModelPreflightIssue unknownIssue;
    unknownIssue.code = "UNKNOWN_PREFLIGHT_CODE";
    unknownIssue.severity = slicer_core::ModelPreflightIssueSeverity::Error;
    unknownIssue.count = 2U;
    execution.result.issues.push_back(unknownIssue);

    const ModelPreflightPresentation warning =
        ModelPreflightPresenter::Present(
            execution,
            slicer_core::ModelPreflightPipelineMode::Legacy);
    if (warning.state != QStringLiteral("检测有警告")
        || warning.admission != QStringLiteral("需要确认风险")
        || warning.issues.size() != 2
        || !warning.issues.front().summary.contains(QStringLiteral("未识别问题")))
    {
        return fail(QStringLiteral("model-preflight-states 中文状态或未知码 fail-closed 映射错误。"));
    }

    ModelPreflightPanel panel;
    panel.ShowPresentation(warning);
    auto* state = panel.findChild<QLabel*>(
        QStringLiteral("modelPreflightState"));
    auto* issues = panel.findChild<QTableWidget*>(
        QStringLiteral("modelPreflightIssues"));
    if (state == nullptr || issues == nullptr
        || state->text() != QStringLiteral("检测有警告")
        || issues->rowCount() != 2)
    {
        return fail(QStringLiteral("model-preflight-states 面板未呈现完整状态和问题列表。"));
    }

    execution.result.status = slicer_core::ModelPreflightStatus::Running;
    execution.result.legacyAdmission.status =
        slicer_core::ModelPreflightAdmissionStatus::Blocked;
    const ModelPreflightPresentation running =
        ModelPreflightPresenter::Present(
            execution,
            slicer_core::ModelPreflightPipelineMode::Legacy);
    if (!running.running || !running.cancancel || running.canrecheck)
    {
        return fail(QStringLiteral("model-preflight-states 运行态按钮能力错误。"));
    }
    return pass(QStringLiteral("model-preflight-states 中文状态、未知码和面板展示通过。"));
}

int UiSmokeTestRunner::ModelPreflightOneClickGate(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        return fail(QStringLiteral("model-preflight-one-click-gate 无法创建临时目录。"));
    }
    const QString cleanConfig = WritePreflightFixture(
        tempDir.path(),
        QStringLiteral("clean"),
        ClosedBoxObjFixture());
    const QString openConfig = WritePreflightFixture(
        tempDir.path(),
        QStringLiteral("open"),
        OpenTriangleObjFixture());
    const QString missingConfig = tempDir.filePath(QStringLiteral("missing.json"));
    const QJsonObject missingRoot{
        {QStringLiteral("input"),
         QJsonObject{{QStringLiteral("modelPath"),
                      tempDir.filePath(QStringLiteral("absent.obj"))},
                     {QStringLiteral("format"), QStringLiteral("obj")}}},
    };
    if (cleanConfig.isEmpty() || openConfig.isEmpty()
        || !WriteJsonFixture(missingConfig, missingRoot))
    {
        return fail(QStringLiteral("model-preflight-one-click-gate 无法创建 fixture。"));
    }

    ModelPreflightController productionGlobalController;
    SlicePreflightCoordinator productionGlobalCoordinator(
        &productionGlobalController);
    int productionGlobalAdmittedCount{0};
    QObject::connect(
        &productionGlobalCoordinator,
        &SlicePreflightCoordinator::SigActionAdmitted,
        &productionGlobalCoordinator,
        [&productionGlobalAdmittedCount]()
        {
            ++productionGlobalAdmittedCount;
        });
    SlicePreflightAction cleanGlobalProductionAction;
    cleanGlobalProductionAction.kind =
        SlicePreflightActionKind::GlobalProduction;
    cleanGlobalProductionAction.configpath = cleanConfig;
    productionGlobalCoordinator.RequestAction(
        cleanGlobalProductionAction);
    if (!WaitForCondition(
            [&productionGlobalAdmittedCount]()
            {
                return productionGlobalAdmittedCount == 1;
            })
        || !productionGlobalController.LastCapabilityDiagnostic().contains(
            QStringLiteral("request-override=true")))
    {
        return fail(
            QStringLiteral(
                "model-preflight-one-click-gate Global production 未通过普通 slicer_cli 能力路径。"));
    }

    ModelPreflightController controller;
    controller.SetCapabilityOverrideForTests(true);
    SlicePreflightCoordinator coordinator(&controller);
    int admittedCount{0};
    int blockedCount{0};
    int confirmationCount{0};
    QObject::connect(
        &coordinator,
        &SlicePreflightCoordinator::SigActionAdmitted,
        &coordinator,
        [&admittedCount]()
        {
            ++admittedCount;
        });
    QObject::connect(
        &coordinator,
        &SlicePreflightCoordinator::SigActionBlocked,
        &coordinator,
        [&blockedCount]()
        {
            ++blockedCount;
        });
    QObject::connect(
        &coordinator,
        &SlicePreflightCoordinator::SigLegacyConfirmationRequired,
        &coordinator,
        [&confirmationCount]()
        {
            ++confirmationCount;
        });

    SlicePreflightAction cleanAction;
    cleanAction.kind = SlicePreflightActionKind::Legacy;
    cleanAction.configpath = cleanConfig;
    coordinator.RequestAction(cleanAction);
    if (!WaitForCondition([&admittedCount]() { return admittedCount == 1; }))
    {
        return fail(QStringLiteral("model-preflight-one-click-gate clean legacy 未放行。"));
    }

    SlicePreflightAction missingAction;
    missingAction.kind = SlicePreflightActionKind::Legacy;
    missingAction.configpath = missingConfig;
    coordinator.RequestAction(missingAction);
    if (!WaitForCondition([&blockedCount]() { return blockedCount == 1; })
        || admittedCount != 1)
    {
        return fail(QStringLiteral("model-preflight-one-click-gate fatal 输入启动了动作。"));
    }

    SlicePreflightAction globalAction;
    globalAction.kind = SlicePreflightActionKind::GlobalProduction;
    globalAction.configpath = openConfig;
    coordinator.RequestAction(globalAction);
    if (!WaitForCondition([&blockedCount]() { return blockedCount == 2; })
        || admittedCount != 1)
    {
        return fail(QStringLiteral("model-preflight-one-click-gate global topology blocker 未阻断。"));
    }

    SlicePreflightAction legacyWarningAction;
    legacyWarningAction.kind = SlicePreflightActionKind::Legacy;
    legacyWarningAction.configpath = openConfig;
    coordinator.RequestAction(legacyWarningAction);
    if (!WaitForCondition(
            [&confirmationCount]() { return confirmationCount == 1; })
        || admittedCount != 1)
    {
        return fail(QStringLiteral("model-preflight-one-click-gate legacy warning 未等待确认。"));
    }
    coordinator.ConfirmLegacyWarning(true);
    if (admittedCount != 2)
    {
        return fail(QStringLiteral("model-preflight-one-click-gate legacy 明确确认后未放行。"));
    }

    bool realCapabilityVerified{false};
    const QString candidateProgram =
        ToolPaths::FromRepoRoot(options.repo_root).openvdb_slicer_cli;
    if (QFileInfo::exists(candidateProgram))
    {
        ModelPreflightController realController;
        SlicePreflightCoordinator realCoordinator(&realController);
        int realAdmittedCount{0};
        QObject::connect(
            &realCoordinator,
            &SlicePreflightCoordinator::SigActionAdmitted,
            &realCoordinator,
            [&realAdmittedCount]()
            {
                ++realAdmittedCount;
            });
        SlicePreflightAction realGlobalAction;
        realGlobalAction.kind = SlicePreflightActionKind::OpenVdbCandidate;
        realGlobalAction.configpath = cleanConfig;
        realGlobalAction.capabilityprogram = candidateProgram;
        realCoordinator.RequestAction(realGlobalAction);
        if (!WaitForCondition(
                [&realAdmittedCount, &realController]()
                {
                    return realAdmittedCount == 1 || !realController.IsRunning();
                },
                90000)
            || realAdmittedCount != 1)
        {
            const slicer_core::ModelPreflightExecutionResult& realExecution =
                realController.CurrentExecution();
            QStringList blockerCodes;
            for (const std::string& code : realExecution.result.globalAdmission.blockerCodes)
            {
                blockerCodes.push_back(QString::fromStdString(code));
            }
            return fail(QStringLiteral(
                            "model-preflight-one-click-gate 真实 OpenVDB capability 探针未放行 clean global："
                            "status=%1 generation=%2 blockers=%3 program=%4")
                            .arg(QString::fromStdString(
                                slicer_core::ModelPreflightStatusName(
                                    realExecution.result.status)))
                            .arg(realExecution.generation)
                            .arg(blockerCodes.join(QStringLiteral(",")))
                            .arg(candidateProgram)
                        + QStringLiteral(" diagnostic=")
                        + realController.LastCapabilityDiagnostic());
        }
        realCapabilityVerified = true;
    }

    return pass(
        QStringLiteral(
            "model-preflight-one-click-gate admitted=2 blocked=2 process-start-before-admission=0 "
            "global-production=admitted real-capability=%1")
            .arg(realCapabilityVerified ? QStringLiteral("verified")
                                        : QStringLiteral("skipped")));
}

int UiSmokeTestRunner::ModelPreflightLifecycle(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        return fail(QStringLiteral("model-preflight-lifecycle 无法创建临时目录。"));
    }
    const QString cleanConfig = WritePreflightFixture(
        tempDir.path(),
        QStringLiteral("clean"),
        ClosedBoxObjFixture());
    const QString openConfig = WritePreflightFixture(
        tempDir.path(),
        QStringLiteral("open"),
        OpenTriangleObjFixture());
    if (cleanConfig.isEmpty() || openConfig.isEmpty())
    {
        return fail(QStringLiteral("model-preflight-lifecycle 无法创建 fixture。"));
    }

    ModelPreflightController controller;
    controller.SetCapabilityOverrideForTests(false);
    UiModelPreflightRequest first;
    first.configpath = openConfig;
    UiModelPreflightRequest second;
    second.configpath = cleanConfig;
    UiModelPreflightRequest third = second;
    controller.RequestPreflight(first);
    controller.RequestPreflight(second);
    controller.RequestPreflight(third);
    if (!WaitForCondition(
            [&controller]()
            {
                return !controller.IsRunning()
                    && controller.CurrentExecution().generation == 3U;
            }))
    {
        return fail(QStringLiteral("model-preflight-lifecycle 最新 generation 未完成。"));
    }
    if (controller.CurrentExecution().result.status
            != slicer_core::ModelPreflightStatus::Passed
        || controller.CurrentExecution().generation != 3U)
    {
        return fail(QStringLiteral("model-preflight-lifecycle 旧 generation 覆盖了最新结果。"));
    }

    controller.RequestPreflight(first);
    controller.Cancel();
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    if (controller.CurrentExecution().result.status
        != slicer_core::ModelPreflightStatus::Cancelled)
    {
        return fail(QStringLiteral("model-preflight-lifecycle 取消状态未保持。"));
    }

    auto* disposable = new ModelPreflightController();
    disposable->SetCapabilityOverrideForTests(false);
    disposable->RequestPreflight(first);
    delete disposable;
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    return pass(QStringLiteral(
        "model-preflight-lifecycle latest-generation=3 cancel=stable close=no-crash"));
}

int UiSmokeTestRunner::ModelTopView(
    const UiSmokeTestOptions& options)
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    ModelTopViewLoader loader(&document, &repository);
    ModelTopViewWidget widget(&document, &selection);

    if (document.State() != SceneDocumentState::Unloaded
        || widget.HasRenderableGeometry())
    {
        return fail(QStringLiteral("model top view initial state mismatch"));
    }

    ModelTopViewLoadRequest request;
    request.configpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/material_process_top2_fixture.json"));
    request.modelpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    request.sceneid = QStringLiteral("smoke-scene");
    request.modelid = QStringLiteral("smoke-model");
    request.instanceid = QStringLiteral("smoke-instance");
    request.scenerevision = 3U;
    request.transformrevision = 0U;
    loader.RequestLoad(request);
    if (document.State() != SceneDocumentState::Loading)
    {
        return fail(QStringLiteral("model top view did not enter loading"));
    }
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            }))
    {
        return fail(QStringLiteral("model top view load timed out"));
    }
    if (document.State() != SceneDocumentState::Ready
        || !widget.HasRenderableGeometry()
        || !document.Geometry().has_value()
        || document.Geometry()->sceneid != "smoke-scene"
        || document.Geometry()->instanceid != "smoke-instance"
        || document.Geometry()->scenerevision != 3U)
    {
        return fail(QStringLiteral("model top view ready geometry mismatch"));
    }

    const QList<QSize> sizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    for (const QSize& size : sizes)
    {
        widget.resize(size);
        QImage image(size, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        widget.render(&image);
        bool foundGeometryPixel = false;
        for (int y = 0; y < image.height() && !foundGeometryPixel; y += 2)
        {
            for (int x = 0; x < image.width(); x += 2)
            {
                const QColor pixel = image.pixelColor(x, y);
                if (pixel.green() > 110
                    && pixel.blue() > 100
                    && pixel.red() < 120)
                {
                    foundGeometryPixel = true;
                    break;
                }
            }
        }
        if (!foundGeometryPixel)
        {
            return fail(
                QStringLiteral("model top view rendered blank at %1x%2")
                    .arg(size.width())
                    .arg(size.height()));
        }
    }

    selection.Clear();
    QMouseEvent selectEvent(
        QEvent::MouseButtonPress,
        QPointF(
            widget.width() * 0.5,
            (52.0 + widget.height() - 34.0) * 0.5),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(&widget, &selectEvent);
    if (selection.SelectedInstance() != QStringLiteral("smoke-instance"))
    {
        return fail(QStringLiteral("model top view selection mismatch"));
    }

    ModelTopViewLoadRequest staleRequest = request;
    staleRequest.modelpath = QDir(options.repo_root).filePath(
        QStringLiteral("samples/models/missing-stale.obj"));
    staleRequest.sceneid = QStringLiteral("stale-scene");
    ModelTopViewLoadRequest latestRequest = request;
    latestRequest.sceneid = QStringLiteral("latest-scene");
    loader.RequestLoad(staleRequest);
    loader.RequestLoad(latestRequest);
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready
        || !document.Geometry().has_value()
        || document.Geometry()->sceneid != "latest-scene")
    {
        return fail(QStringLiteral("stale model top view result was published"));
    }

    slicer_core::SceneViewGeometry blockedGeometry =
        document.Geometry().value();
    const slicer_core::ModelInstance blockedInstance =
        document.Instance().value();
    const QString blockedCacheKey = document.SourceCacheKey();
    const QString blockedSourceHash = document.SourceHash();
    const QString blockedResourceHash = document.ResourceHash();
    blockedGeometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Blocked;
    const quint64 blockedGeneration = document.Generation() + 1U;
    document.SetLoading(
        blockedGeneration,
        QStringLiteral(
            "C:/很长的中文模型路径/用于验证界面不会遮挡/"
            "模型资产_版本_最终候选.obj"));
    document.SetSceneContext(
        blockedGeneration,
        QString::fromStdString(blockedGeometry.sceneid),
        blockedGeometry.scenerevision,
        blockedCacheKey,
        blockedSourceHash,
        blockedResourceHash,
        blockedInstance);
    document.SetGeometry(
        blockedGeneration,
        std::move(blockedGeometry));
    if (document.State() != SceneDocumentState::Blocked
        || !widget.HasRenderableGeometry())
    {
        return fail(QStringLiteral("blocked model was not viewable"));
    }

    const quint64 failedGeneration = document.Generation() + 1U;
    document.SetLoading(failedGeneration, QStringLiteral("missing.obj"));
    document.SetFailure(
        failedGeneration,
        QStringLiteral("fixture failure"));
    if (document.State() != SceneDocumentState::Failed
        || widget.HasRenderableGeometry())
    {
        return fail(QStringLiteral("model top view failure state mismatch"));
    }

    loader.RequestLoad(request);
    loader.Cancel();
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    QThread::msleep(20);
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    if (document.State() != SceneDocumentState::Cancelled
        || loader.IsRunning())
    {
        return fail(QStringLiteral("model top view cancellation mismatch"));
    }

    MainWindow window(options.repo_root);
    auto* importButton = window.findChild<QPushButton*>(
        QStringLiteral("importModelPreviewButton"));
    auto* workspace = window.findChild<QWidget*>(
        QStringLiteral("modelTopViewWorkspace"));
    auto* canvas = window.findChild<ModelTopViewWidget*>(
        QStringLiteral("modelTopViewWidget"));
    if (importButton == nullptr
        || workspace == nullptr
        || canvas == nullptr
        || importButton->text() != QStringLiteral("导入模型预览"))
    {
        return fail(QStringLiteral("model top view workspace integration missing"));
    }

    return pass(QStringLiteral(
        "model-top-view async/+Z/grid/identity/selection/blocked/cancel"));
}

int UiSmokeTestRunner::ModelTopViewTransform(
    const UiSmokeTestOptions& options)
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    ModelTopViewLoader loader(&document, &repository);
    SceneTransformController controller(
        &document,
        &selection,
        &repository);
    controller.SetProjectionRequester(
        [&loader](const SceneProjectionRequest& request)
        {
            loader.RequestProjection(request);
        });

    QWidget workspace;
    workspace.setObjectName(
        QStringLiteral("modelTransformSmokeWorkspace"));
    ModelTopViewWidget canvas(&document, &selection, &workspace);
    ModelTransformPanel panel(
        &document,
        &selection,
        &controller,
        &workspace);

    ModelTopViewLoadRequest request;
    request.configpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/material_process_top2_fixture.json"));
    request.modelpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    request.sceneid = QStringLiteral("transform-smoke-scene");
    request.modelid = QStringLiteral("transform-smoke-model");
    request.instanceid = QStringLiteral("transform-smoke-instance");
    request.scenerevision = 1U;
    loader.RequestLoad(request);
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready)
    {
        return fail(QStringLiteral(
            "model transform fixture did not become ready"));
    }
    selection.SetSelectedInstance(request.instanceid);

    auto* translateX = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelTransformTranslateX"));
    auto* translateY = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelTransformTranslateY"));
    auto* rotateZ = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelTransformRotateZ"));
    auto* uniformScale = panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("modelTransformUniformScale"));
    auto* applyButton = panel.findChild<QPushButton*>(
        QStringLiteral("modelTransformApplyButton"));
    auto* centerButton = panel.findChild<QPushButton*>(
        QStringLiteral("modelTransformCenterButton"));
    auto* resetButton = panel.findChild<QPushButton*>(
        QStringLiteral("modelTransformResetButton"));
    auto* saveButton = panel.findChild<QPushButton*>(
        QStringLiteral("modelTransformSaveButton"));
    if (translateX == nullptr
        || translateY == nullptr
        || rotateZ == nullptr
        || uniformScale == nullptr
        || applyButton == nullptr
        || centerButton == nullptr
        || resetButton == nullptr
        || saveButton == nullptr)
    {
        return fail(QStringLiteral(
            "model transform controls are incomplete"));
    }

    translateX->setValue(12.34);
    translateY->setValue(-5.67);
    rotateZ->setValue(45.0);
    uniformScale->setValue(1.25);
    applyButton->click();
    translateX->setValue(13.34);
    applyButton->click();
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready
        || !document.Instance().has_value()
        || document.SceneRevision() != 3U
        || document.Instance()->transformrevision != 2U
        || std::abs(
               document.Instance()->transform.translatexmm - 13.34)
            > 1.0e-9
        || std::abs(
               document.Instance()->transform.translateymm + 5.67)
            > 1.0e-9
        || !document.IsDirty()
        || document.IsGeometryStale())
    {
        return fail(QStringLiteral(
            "precise transform did not publish latest geometry"));
    }

    centerButton->click();
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready
        || document.SceneRevision() != 4U)
    {
        return fail(QStringLiteral(
            "scene-origin center command failed"));
    }
    resetButton->click();
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready
        || document.SceneRevision() != 5U
        || !slicer_core::ModelTransformsEquivalent(
            document.Instance()->transform,
            slicer_core::ModelTransform{}))
    {
        return fail(QStringLiteral("transform reset failed"));
    }

    const QList<QSize> sizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    for (const QSize& size : sizes)
    {
        workspace.resize(size);
        panel.setGeometry(
            size.width() - 300,
            0,
            300,
            size.height());
        canvas.setGeometry(
            0,
            0,
            size.width() - 300,
            size.height());
        if (panel.geometry().right() >= size.width()
            || canvas.geometry().right() >= panel.geometry().left())
        {
            return fail(QStringLiteral(
                "model transform panel overlaps canvas at %1x%2")
                    .arg(size.width())
                    .arg(size.height()));
        }
    }

    ModelTopViewLoadRequest lockedRequest = request;
    lockedRequest.sceneid = QStringLiteral("locked-transform-scene");
    lockedRequest.locked = true;
    loader.RequestLoad(lockedRequest);
    if (!WaitForCondition(
            [&document]()
            {
                return document.State() != SceneDocumentState::Loading;
            })
        || document.State() != SceneDocumentState::Ready)
    {
        return fail(QStringLiteral(
            "locked transform fixture did not load"));
    }
    selection.SetSelectedInstance(lockedRequest.instanceid);
    if (applyButton->isEnabled()
        || centerButton->isEnabled()
        || resetButton->isEnabled()
        || saveButton->isEnabled())
    {
        return fail(QStringLiteral(
            "locked instance transform controls remain enabled"));
    }

    MainWindow window(options.repo_root);
    if (window.findChild<ModelTransformPanel*>(
            QStringLiteral("modelTransformPanel"))
            == nullptr
        || window.findChild<QSplitter*>(
            QStringLiteral("modelTransformWorkspaceSplitter"))
            == nullptr)
    {
        return fail(QStringLiteral(
            "model transform workspace integration missing"));
    }

    return pass(QStringLiteral(
        "model-top-view-transform x/y/rotate/scale/center/reset/"
        "locked/dirty/latest-generation"));
}

int UiSmokeTestRunner::ModelTransformPreflight(
    const UiSmokeTestOptions& options)
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    ModelTopViewLoader topViewLoader(&document, &repository);
    TransformedModelPreflightLoader preflightLoader(
        &document,
        &repository);
    SceneTransformController controller(
        &document,
        &selection,
        &repository);
    controller.SetProjectionRequester(
        [&topViewLoader](const SceneProjectionRequest& request)
        {
            topViewLoader.RequestProjection(request);
        });
    QObject::connect(
        &topViewLoader,
        &ModelTopViewLoader::SigLoadingFinished,
        &preflightLoader,
        [&document, &preflightLoader]()
        {
            if (document.State() == SceneDocumentState::Ready
                && !document.IsGeometryStale())
            {
                preflightLoader.RequestCurrent();
            }
        });

    QWidget workspace;
    ModelTopViewWidget canvas(&document, &selection, &workspace);
    ModelTransformPanel panel(
        &document,
        &selection,
        &controller,
        &workspace);
    ModelTopViewLoadRequest request;
    request.modelpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    request.sceneid = QStringLiteral("preflight-smoke-scene");
    request.modelid = QStringLiteral("preflight-smoke-model");
    request.instanceid =
        QStringLiteral("preflight-smoke-instance");
    request.scenerevision = 1U;
    topViewLoader.RequestLoad(request);
    if (!WaitForCondition(
            [&document]()
            {
                return document.TransformedPreflightState()
                    == SceneTransformedPreflightState::Ready;
            },
            15000)
        || !document.TransformedPreflight().has_value())
    {
        return fail(QStringLiteral(
            "initial transformed preflight did not complete"));
    }
    selection.SetSelectedInstance(request.instanceid);

    auto* mirrorX = panel.findChild<QPushButton*>(
        QStringLiteral("modelTransformMirrorXButton"));
    auto* mirrorY = panel.findChild<QPushButton*>(
        QStringLiteral("modelTransformMirrorYButton"));
    auto* sourceStatus = panel.findChild<QLabel*>(
        QStringLiteral("modelTransformSourcePreflight"));
    auto* transformedStatus = panel.findChild<QLabel*>(
        QStringLiteral("modelTransformEffectivePreflight"));
    if (mirrorX == nullptr
        || mirrorY == nullptr
        || sourceStatus == nullptr
        || transformedStatus == nullptr)
    {
        return fail(QStringLiteral(
            "mirror or transformed preflight controls missing"));
    }

    mirrorX->click();
    mirrorY->click();
    if (!WaitForCondition(
            [&document]()
            {
                return document.TransformedPreflightState()
                    == SceneTransformedPreflightState::Ready
                    && document.TransformedPreflight().has_value()
                    && document.TransformedPreflight()
                           ->scenerevision
                        == document.SceneRevision();
            },
            15000)
        || !document.Instance()->transform.mirrorx
        || !document.Instance()->transform.mirrory
        || document.SceneRevision() != 3U
        || document.TransformedPreflight()
               ->transformrevision
            != 2U
        || document.TransformedPreflight()
               ->source.result.status
            != slicer_core::ModelPreflightStatus::Passed
        || document.TransformedPreflight()
               ->transformed.result.globalAdmission.status
            != slicer_core::ModelPreflightAdmissionStatus::Passed
        || !sourceStatus->text().contains(
            QStringLiteral("通过"))
        || !transformedStatus->text().contains(
            QStringLiteral("Global=通过")))
    {
        return fail(QStringLiteral(
            "latest mirrored transformed preflight mismatch"));
    }

    QTemporaryDir openMeshDirectory;
    const QString openMeshPath =
        openMeshDirectory.filePath(QStringLiteral("open_mesh.obj"));
    QFile openMesh(openMeshPath);
    if (!openMesh.open(QIODevice::WriteOnly | QIODevice::Text)
        || openMesh.write(
               "v 0 0 0\n"
               "v 10 0 0\n"
               "v 0 10 0\n"
               "f 1 2 3\n")
            <= 0)
    {
        return fail(QStringLiteral(
            "failed to create open mesh smoke fixture"));
    }
    openMesh.close();

    ModelTopViewLoadRequest blockedRequest = request;
    blockedRequest.modelpath = openMeshPath;
    blockedRequest.sceneid =
        QStringLiteral("blocked-preflight-smoke-scene");
    blockedRequest.modelid =
        QStringLiteral("blocked-preflight-smoke-model");
    blockedRequest.instanceid =
        QStringLiteral("blocked-preflight-smoke-instance");
    topViewLoader.RequestLoad(blockedRequest);
    if (!WaitForCondition(
            [&document]()
            {
                return document.TransformedPreflightState()
                    == SceneTransformedPreflightState::Ready;
            },
            15000)
        || !document.TransformedPreflight().has_value()
        || document.TransformedPreflight()
               ->transformed.result.globalAdmission.status
            != slicer_core::ModelPreflightAdmissionStatus::Blocked
        || !canvas.HasRenderableGeometry())
    {
        return fail(QStringLiteral(
            "blocked transformed model was not retained for viewing"));
    }

    const QList<QSize> sizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    for (const QSize& size : sizes)
    {
        workspace.resize(size);
        panel.setGeometry(
            size.width() - 300,
            0,
            300,
            size.height());
        canvas.setGeometry(
            0,
            0,
            size.width() - 300,
            size.height());
        if (panel.geometry().right() >= size.width()
            || canvas.geometry().right() >= panel.geometry().left()
            || !canvas.HasRenderableGeometry())
        {
            return fail(QStringLiteral(
                "preflight panel overlaps view or hides blocked model "
                "at %1x%2")
                    .arg(size.width())
                    .arg(size.height()));
        }
    }

    return pass(QStringLiteral(
        "model-transform-preflight mirror-x/y/source/effective/"
        "latest-revision/global-blocked-viewable/three-window-sizes"));
}

int UiSmokeTestRunner::MultiModelList(
    const UiSmokeTestOptions& options)
{
    QWidget workspace;
    SceneDocument document;
    SceneSelectionModel selection;
    ModelTopViewWidget canvas(&document, &selection, &workspace);
    ModelListPanel panel(&document, &selection, &workspace);

    slicer_core::ModelInstance first;
    first.instanceid = "multi-first";
    first.modelid = "multi-model-first";
    first.sourcetransformidentity = "first-source";
    first.sourcebboxmm = {{0.0, 0.0, 0.0}, {8.0, 4.0, 1.0}};
    first.effectivebboxmm = first.sourcebboxmm;
    document.SetLoading(1U, QStringLiteral("first.obj"));
    if (!document.SetSceneContext(
            1U,
            QStringLiteral("multi-scene"),
            1U,
            QStringLiteral("shared-cache"),
            QStringLiteral("first-source-hash"),
            QStringLiteral("first-resource-hash"),
            first))
    {
        return fail(QStringLiteral("multi-model-list first context failed"));
    }
    slicer_core::SceneViewGeometry firstGeometry;
    firstGeometry.sceneid = "multi-scene";
    firstGeometry.modelid = first.modelid;
    firstGeometry.instanceid = first.instanceid;
    firstGeometry.scenerevision = 1U;
    firstGeometry.worldboundsmm = {{0.0, 0.0}, {8.0, 4.0}};
    firstGeometry.triangles.push_back(
        {{0.0, 0.0}, {8.0, 0.0}, {0.0, 4.0}});
    firstGeometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;
    if (!document.SetGeometry(1U, firstGeometry))
    {
        return fail(QStringLiteral("multi-model-list first geometry failed"));
    }
    selection.SetSelectedInstance(QStringLiteral("multi-first"));

    const SceneDocumentOperationResult duplicated =
        document.DuplicateInstance(
            QStringLiteral("multi-first"),
            QStringLiteral("multi-copy"),
            document.SceneRevision());
    selection.SetSelectedInstance(document.CurrentInstanceId());
    if (!duplicated.IsValid()
        || document.InstanceCount() != 2U
        || document.Items().at(0U).sourcecachekey
            != document.Items().at(1U).sourcecachekey)
    {
        return fail(
            QStringLiteral("multi-model-list source sharing failed"));
    }

    QApplication::processEvents(QEventLoop::AllEvents, 50);
    auto* list = panel.findChild<QListWidget*>(
        QStringLiteral("modelInstanceList"));
    auto* visibility = panel.findChild<QToolButton*>(
        QStringLiteral("modelListVisibilityButton"));
    auto* lock = panel.findChild<QToolButton*>(
        QStringLiteral("modelListLockButton"));
    auto* remove = panel.findChild<QToolButton*>(
        QStringLiteral("modelListDeleteButton"));
    auto* add = panel.findChild<QToolButton*>(
        QStringLiteral("modelListAddButton"));
    if (list == nullptr
        || visibility == nullptr
        || lock == nullptr
        || remove == nullptr
        || add == nullptr
        || list->count() != 2)
    {
        return fail(
            QStringLiteral("multi-model-list controls or rows missing"));
    }

    list->setCurrentRow(1);
    visibility->click();
    lock->click();
    if (document.Items().at(1U).instance.visible
        || !document.Items().at(1U).instance.locked
        || remove->isEnabled())
    {
        return fail(
            QStringLiteral("multi-model-list visibility/lock mismatch"));
    }
    lock->click();
    remove->click();
    if (document.InstanceCount() != 1U
        || selection.SelectedInstance()
            != QStringLiteral("multi-first"))
    {
        return fail(
            QStringLiteral("multi-model-list delete/selection mismatch"));
    }

    bool addRequested{false};
    QObject::connect(
        &panel,
        &ModelListPanel::SigAddRequested,
        &workspace,
        [&addRequested]()
        {
            addRequested = true;
        });
    add->click();
    if (!addRequested)
    {
        return fail(QStringLiteral("multi-model-list add signal missing"));
    }

    SceneDocument importedDocument;
    SceneModelRepository repository;
    ModelTopViewLoader loader(&importedDocument, &repository);
    ModelTopViewLoadRequest firstRequest;
    firstRequest.configpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/material_process_top2_fixture.json"));
    firstRequest.modelpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    firstRequest.sceneid = QStringLiteral("import-scene");
    firstRequest.modelid = QStringLiteral("import-model-1");
    firstRequest.instanceid = QStringLiteral("import-instance-1");
    firstRequest.scenerevision = 1U;
    loader.RequestLoad(firstRequest);
    if (!WaitForCondition(
            [&importedDocument]()
            {
                return importedDocument.State()
                    != SceneDocumentState::Loading;
            })
        || importedDocument.State() != SceneDocumentState::Ready)
    {
        return fail(
            QStringLiteral("multi-model-list first async import failed"));
    }
    ModelTopViewLoadRequest secondRequest = firstRequest;
    secondRequest.modelid = QStringLiteral("import-model-2");
    secondRequest.instanceid = QStringLiteral("import-instance-2");
    secondRequest.scenerevision = 2U;
    secondRequest.appendtoscene = true;
    loader.RequestLoad(secondRequest);
    if (!WaitForCondition(
            [&importedDocument]()
            {
                return importedDocument.State()
                    != SceneDocumentState::Loading;
            })
        || importedDocument.State() != SceneDocumentState::Ready
        || importedDocument.InstanceCount() != 2U
        || repository.Size() != 1U)
    {
        return fail(
            QStringLiteral(
                "multi-model-list async append/source sharing failed"));
    }

    MainWindow window(options.repo_root);
    auto* integratedPanel = window.findChild<ModelListPanel*>(
        QStringLiteral("modelListPanel"));
    auto* integratedCanvas = window.findChild<ModelTopViewWidget*>(
        QStringLiteral("modelTopViewWidget"));
    auto* transformPanel = window.findChild<ModelTransformPanel*>(
        QStringLiteral("modelTransformPanel"));
    auto* sideTabs = window.findChild<QTabWidget*>(
        QStringLiteral("modelSceneSideTabs"));
    if (integratedPanel == nullptr
        || integratedCanvas == nullptr
        || transformPanel == nullptr
        || sideTabs == nullptr)
    {
        return fail(
            QStringLiteral("multi-model-list workspace integration missing"));
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
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        const QRect canvasRect =
            GlobalRect(integratedCanvas).adjusted(1, 1, -1, -1);
        const QRect sideRect =
            GlobalRect(sideTabs).adjusted(1, 1, -1, -1);
        if (canvasRect.intersects(sideRect)
            || sideTabs->width() < sideTabs->minimumWidth()
            || integratedCanvas->width() < integratedCanvas->minimumWidth()
            || sideTabs->count() != 3)
        {
            return fail(
                QStringLiteral(
                    "multi-model-list overlap at %1x%2 "
                    "canvas=%3,%4,%5,%6 side=%7,%8,%9,%10")
                    .arg(size.width())
                    .arg(size.height())
                    .arg(canvasRect.x())
                    .arg(canvasRect.y())
                    .arg(canvasRect.width())
                    .arg(canvasRect.height())
                    .arg(sideRect.x())
                    .arg(sideRect.y())
                    .arg(sideRect.width())
                    .arg(sideRect.height()));
        }
    }

    return pass(QStringLiteral(
        "multi-model-list add/share/duplicate/visibility/lock/delete/"
        "selection/three-window-sizes"));
}

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
    auto* sideTabs = window.findChild<QTabWidget*>(
        QStringLiteral("modelSceneSideTabs"));
    if (integratedPanel == nullptr
        || sideTabs == nullptr
        || sideTabs->count() != 3)
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
        sideTabs->setCurrentWidget(integratedPanel);
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        if (!integratedPanel->isVisible()
            || integratedPanel->width() < 200
            || sideTabs->geometry().right()
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
