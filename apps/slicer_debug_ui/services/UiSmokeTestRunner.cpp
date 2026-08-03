#include "UiSmokeTestRunner.h"

#include "../MainWindow.h"
#include "../controllers/SceneBatchImportController.h"
#include "../controllers/SceneTransformController.h"
#include "../models/SceneDocument.h"
#include "../models/SceneSelectionModel.h"
#include "../widgets/ChannelChartPanel.h"
#include "../widgets/ConfigEditorPanel.h"
#include "../widgets/ContextInspector.h"
#include "../widgets/DiagnosticsDock.h"
#include "../widgets/DiagnosticSemanticPreviewPanel.h"
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
#include "../widgets/ProductionTextureSettingsPanel.h"
#include "../widgets/QuickConfigPanel.h"
#include "../widgets/ReportPanel.h"
#include "../widgets/SettingHelpPanel.h"
#include "../widgets/SliceTimingPanel.h"
#include "../widgets/PreviewWorkspace.h"
#include "../widgets/ProjectToolsDock.h"
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
#include "WorkspaceLayoutState.h"
#include "slicer_core/config.h"

#include <QAbstractSpinBox>
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
#include <QFont>
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
#include <QSettings>
#include <QSet>
#include <QSize>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QThread>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>

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

bool ReadJsonObject(const QString& path, QJsonObject* object)
{
    if (object == nullptr)
    {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }
    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError
        || !document.isObject())
    {
        return false;
    }
    *object = document.object();
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
    if (options.case_name == "tiff-native-preview-all-materials")
    {
        return TiffNativePreviewAllMaterials(options);
    }
    if (options.case_name == "tiff-native-preview-no-png")
    {
        return TiffNativePreviewNoPng(options);
    }
    if (options.case_name == "preview-legend-probe-context")
    {
        return PreviewLegendProbeContext(options);
    }
    if (options.case_name == "preview-physical-aspect")
    {
        return PreviewPhysicalAspect(options);
    }
    if (options.case_name == "diagnostic-semantic-preview")
    {
        return DiagnosticSemanticPreview(options);
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
    if (options.case_name == "workbench-job-action-bar")
    {
        return WorkbenchJobActionBar(options);
    }
    if (options.case_name == "workbench-context-inspector")
    {
        return WorkbenchContextInspector(options);
    }
    if (options.case_name == "workbench-project-diagnostics")
    {
        return WorkbenchProjectDiagnostics(options);
    }
    if (options.case_name == "workbench-layout-restore")
    {
        return WorkbenchLayoutRestore(options);
    }
    if (options.case_name == "workbench-1280x720")
    {
        return Workbench1280x720(options);
    }
    if (options.case_name == "diagnostic-settings-controls")
    {
        return DiagnosticSettingsControls(options);
    }
    if (options.case_name == "production-texture-controls")
    {
        return ProductionTextureControls(options);
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
    if (options.case_name == "scene-batch-import-three")
    {
        return SceneBatchImportThree(options);
    }
    if (options.case_name == "scene-batch-import-real-meigui")
    {
        return SceneBatchImportRealMeigui(options);
    }
    if (options.case_name
        == "scene-batch-import-partial-failure")
    {
        return SceneBatchImportPartialFailure(options);
    }
    if (options.case_name == "scene-slice-current")
    {
        return SceneSliceCurrent(options);
    }
    if (options.case_name
        == "scene-slice-single-material-profile")
    {
        return SceneSliceSingleMaterialProfile(options);
    }
    if (options.case_name == "scene-slice-real-assets")
    {
        return SceneSliceRealAssets(options);
    }
    if (options.case_name == "scene-slice-stale")
    {
        return SceneSliceStale(options);
    }
    if (options.case_name == "scene-slice-cancel")
    {
        return SceneSliceCancel(options);
    }
    if (options.case_name == "scene-slice-no-fallback")
    {
        return SceneSliceNoFallback(options);
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

int UiSmokeTestRunner::WorkbenchJobActionBar(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents, 50);

    QWidget* actionBar = window.findChild<QWidget*>(
        QStringLiteral("sceneActionBar"));
    QWidget* projectPanel = window.findChild<QWidget*>(
        QStringLiteral("projectPanel"));
    QSplitter* mainSplitter = window.findChild<QSplitter*>(
        QStringLiteral("mainSplitter"));
    QPushButton* importButton =
        window.findChild<QPushButton*>(
            QStringLiteral("jobImportModelsButton"));
    QPushButton* saveButton =
        window.findChild<QPushButton*>(
            QStringLiteral("jobSaveSceneButton"));
    QPushButton* sliceButton =
        window.findChild<QPushButton*>(
            QStringLiteral("sliceCurrentSceneButton"));
    QPushButton* cancelButton =
        window.findChild<QPushButton*>(
            QStringLiteral("cancelCurrentSceneSliceButton"));
    QLabel* modeLabel = window.findChild<QLabel*>(
        QStringLiteral("jobModeSummaryLabel"));
    QComboBox* profileSelector =
        window.findChild<QComboBox*>(
            QStringLiteral("jobProfileSelector"));
    QLabel* statusLabel = window.findChild<QLabel*>(
        QStringLiteral("sceneSliceActionStateLabel"));
    if (actionBar == nullptr
        || projectPanel == nullptr
        || mainSplitter == nullptr
        || importButton == nullptr
        || saveButton == nullptr
        || sliceButton == nullptr
        || cancelButton == nullptr
        || modeLabel == nullptr
        || profileSelector == nullptr
        || statusLabel == nullptr)
    {
        return fail(QStringLiteral(
            "13D-01 top job action controls missing"));
    }
    if (actionBar->parentWidget() != window.centralWidget()
        || projectPanel->isAncestorOf(actionBar)
        || actionBar->geometry().bottom()
            > mainSplitter->geometry().top()
        || !actionBar->isVisibleTo(&window))
    {
        return fail(QStringLiteral(
            "13D-01 action bar is not fixed above main workspace"));
    }
    if (!importButton->isEnabled()
        || saveButton->isEnabled()
        || sliceButton->isEnabled()
        || cancelButton->isEnabled()
        || !modeLabel->text().contains(
            QStringLiteral("传统切片"))
        || profileSelector->findData(
               QStringLiteral(
                   "textured_nail_rgb_white_lower_support"))
               < 0
        || profileSelector->findData(
               QStringLiteral(
                   "textured_nail_rgb_only_lower_support"))
               < 0
        || profileSelector->currentData().toString()
            != QStringLiteral(
                "textured_nail_rgb_only_lower_support")
        || statusLabel->text().trimmed().isEmpty())
    {
        return fail(QStringLiteral(
            "13D-01 empty-scene action state mismatch"));
    }

    const int rgbOnlyIndex =
        profileSelector->findData(
            QStringLiteral(
                "textured_nail_rgb_only_lower_support"));
    profileSelector->setCurrentIndex(rgbOnlyIndex);
    QMetaObject::invokeMethod(
        profileSelector,
        "activated",
        Qt::DirectConnection,
        Q_ARG(int, rgbOnlyIndex));
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (window.m_currentProfileId
            != QStringLiteral(
                "textured_nail_rgb_only_lower_support")
        || window.config_document_
               .value(
                   {QStringLiteral("modelFill"),
                    QStringLiteral("material")})
               .toString()
            != QStringLiteral("rgb")
        || window.config_document_
               .value(
                   {QStringLiteral("materialProcessProfile"),
                    QStringLiteral("white"),
                    QStringLiteral("enabled")})
               .toBool(true))
    {
        return fail(QStringLiteral(
            "13D-R1 top Profile selector did not apply RGB-only production settings"));
    }

    for (int index = 0;
         index < window.m_mainWorkspaceTabs->count();
         ++index)
    {
        window.m_mainWorkspaceTabs->setCurrentIndex(index);
        QApplication::processEvents(
            QEventLoop::AllEvents,
            20);
        if (!actionBar->isVisibleTo(&window))
        {
            return fail(QStringLiteral(
                "13D-01 action bar disappeared after workspace switch"));
        }
    }

    const QString configPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    if (!window.config_editor_panel_->loadConfig(configPath))
    {
        return fail(QStringLiteral(
            "13D-01 fixture configuration unavailable"));
    }
    SceneBatchImportRequest request;
    request.batchid = QStringLiteral("13d-job-action-bar");
    request.configpath = configPath;
    request.files = QStringList{modelPath};
    request.autolayout = true;
    if (!window.m_sceneBatchImportController
             .Start(request)
             .IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.m_sceneBatchImportController
                            .IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "13D-01 one-model scene import did not complete"));
    }
    window.UpdateActionAvailability();
    if (!saveButton->isEnabled()
        || !sliceButton->isEnabled()
        || cancelButton->isEnabled()
        || !statusLabel->text().contains(
            QStringLiteral("可见 1")))
    {
        return fail(QStringLiteral(
            "13D-01 admitted-scene action state mismatch"));
    }

    return pass(QStringLiteral(
        "workbench-job-action-bar fixed/import/save/"
        "mode-profile/slice/cancel/state"));
}

int UiSmokeTestRunner::WorkbenchContextInspector(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    ContextInspector* inspector =
        window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"));
    QTabWidget* inspectorTabs =
        window.findChild<QTabWidget*>(
            QStringLiteral("contextInspectorTabs"));
    QSplitter* mainSplitter =
        window.findChild<QSplitter*>(
            QStringLiteral("mainSplitter"));
    QPushButton* openConfigButton =
        window.findChild<QPushButton*>(
            QStringLiteral(
                "contextInspectorOpenConfigButton"));
    QWidget* oldModelSideTabs =
        window.findChild<QWidget*>(
            QStringLiteral("modelSceneSideTabs"));
    if (inspector == nullptr
        || inspectorTabs == nullptr
        || mainSplitter == nullptr
        || openConfigButton == nullptr
        || oldModelSideTabs != nullptr
        || mainSplitter->count() != 2
        || mainSplitter->widget(1) != inspector)
    {
        return fail(QStringLiteral(
            "13D-02 single context inspector shell mismatch"));
    }
    const QStringList expectedPages{
        QStringLiteral("场景"),
        QStringLiteral("变换"),
        QStringLiteral("排版"),
        QStringLiteral("切片设置"),
        QStringLiteral("预检与诊断"),
    };
    if (inspector->PageTitles() != expectedPages
        || !inspector->isAncestorOf(
            window.m_modelListPanel)
        || !inspector->isAncestorOf(
            window.m_modelTransformPanel)
        || !inspector->isAncestorOf(
            window.m_sceneLayoutPanel)
        || !inspector->isAncestorOf(
            window.m_modelPreflightPanel)
        || window.m_modelTopViewWorkspace
               ->isAncestorOf(window.m_modelListPanel))
    {
        return fail(QStringLiteral(
            "13D-02 context pages were duplicated or misplaced"));
    }

    const QString configPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    if (!window.config_editor_panel_->loadConfig(configPath))
    {
        return fail(QStringLiteral(
            "13D-02 fixture configuration unavailable"));
    }
    SceneBatchImportRequest request;
    request.batchid =
        QStringLiteral("13d-context-inspector");
    request.configpath = configPath;
    request.files = QStringList{modelPath};
    request.autolayout = true;
    if (!window.m_sceneBatchImportController
             .Start(request)
             .IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.m_sceneBatchImportController
                            .IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "13D-02 fixture model import did not complete"));
    }
    const quint64 sceneRevision =
        window.m_sceneDocument.SceneRevision();
    const QString selectedInstance =
        window.m_sceneSelectionModel.SelectedInstance();
    if (selectedInstance.isEmpty())
    {
        return fail(QStringLiteral(
            "13D-02 imported instance was not selected"));
    }
    for (int index = 0;
         index < inspectorTabs->count();
         ++index)
    {
        inspectorTabs->setCurrentIndex(index);
        QApplication::processEvents(
            QEventLoop::AllEvents,
            20);
        if (window.m_sceneDocument.SceneRevision()
                != sceneRevision
            || window.m_sceneSelectionModel
                   .SelectedInstance()
                != selectedInstance)
        {
            return fail(QStringLiteral(
                "13D-02 page switch changed scene identity"));
        }
    }

    openConfigButton->click();
    if (window.m_mainWorkspaceTabs->currentWidget()
        != window.m_configWorkspace)
    {
        return fail(QStringLiteral(
            "13D-02 slice settings did not open central config"));
    }
    return pass(QStringLiteral(
        "workbench-context-inspector single-shell/"
        "five-contexts/identity/config-navigation"));
}

int UiSmokeTestRunner::WorkbenchProjectDiagnostics(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    ProjectToolsDock* projectDock =
        window.findChild<ProjectToolsDock*>(
            QStringLiteral("projectToolsDock"));
    DiagnosticsDock* diagnosticsDock =
        window.findChild<DiagnosticsDock*>(
            QStringLiteral("diagnosticsDock"));
    QWidget* projectPanel =
        window.findChild<QWidget*>(
            QStringLiteral("projectPanel"));
    QSplitter* mainSplitter =
        window.findChild<QSplitter*>(
            QStringLiteral("mainSplitter"));
    ContextInspector* inspector =
        window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"));
    QAction* projectAction =
        window.findChild<QAction*>(
            QStringLiteral("projectToolsToggleAction"));
    QAction* diagnosticsAction =
        window.findChild<QAction*>(
            QStringLiteral("diagnosticsToggleAction"));
    QWidget* legacyRightTools =
        window.findChild<QWidget*>(
            QStringLiteral("legacyRightToolsTabs"));
    if (projectDock == nullptr
        || diagnosticsDock == nullptr
        || projectPanel == nullptr
        || mainSplitter == nullptr
        || inspector == nullptr
        || projectAction == nullptr
        || diagnosticsAction == nullptr
        || legacyRightTools != nullptr)
    {
        return fail(QStringLiteral(
            "13D-03 project or diagnostics shell missing"));
    }

    const QStringList expectedInspectorPages{
        QStringLiteral("场景"),
        QStringLiteral("变换"),
        QStringLiteral("排版"),
        QStringLiteral("切片设置"),
        QStringLiteral("预检与诊断"),
    };
    const QStringList expectedDiagnosticPages{
        QStringLiteral("报告"),
        QStringLiteral("材料闭环"),
        QStringLiteral("曲线"),
        QStringLiteral("材料参数"),
        QStringLiteral("工艺对比"),
        QStringLiteral("切片耗时"),
        QStringLiteral("日志"),
    };
    if (mainSplitter->count() != 2
        || mainSplitter->widget(0)
            != window.m_mainWorkspaceTabs
        || mainSplitter->widget(1) != inspector
        || inspector->PageTitles()
            != expectedInspectorPages
        || diagnosticsDock->TabTitles()
            != expectedDiagnosticPages
        || !projectDock->isAncestorOf(projectPanel)
        || !diagnosticsDock->isAncestorOf(
            window.material_process_panel_)
        || !inspector->isAncestorOf(
            window.warnings_view_)
        || !diagnosticsDock->isAncestorOf(
            window.compare_view_)
        || !diagnosticsDock->isAncestorOf(
            window.m_sliceTimingPanel))
    {
        return fail(QStringLiteral(
            "13D-03 capability migration mismatch"));
    }
    if (!projectDock->isAncestorOf(window.build_button_)
        || !projectDock->isAncestorOf(
            window.m_importSliceButton)
        || !projectDock->isAncestorOf(
            window.m_importOpenVdbButton)
        || !projectDock->isAncestorOf(
            window.regression_button_)
        || !projectDock->isAncestorOf(
            window.run_rip_button_))
    {
        return fail(QStringLiteral(
            "13D-03 compatibility actions are unreachable"));
    }

    window.show();
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (projectDock->IsExpanded()
        || diagnosticsDock->IsExpanded()
        || projectAction->isChecked()
        || diagnosticsAction->isChecked()
        || window.dockWidgetArea(diagnosticsDock)
            != Qt::RightDockWidgetArea)
    {
        return fail(QStringLiteral(
            "13D-03 auxiliary regions are not hidden by default "
            "or task details are not docked on the right"));
    }

    const quint64 sceneRevision =
        window.m_sceneDocument.SceneRevision();
    projectDock->SetExpanded(true);
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (!projectDock->IsExpanded()
        || !projectAction->isChecked()
        || !projectPanel->isVisibleTo(projectDock))
    {
        return fail(QStringLiteral(
            "13D-03 project tools cannot be expanded"));
    }
    projectDock->SetExpanded(false);
    diagnosticsDock->SetExpanded(true);
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (projectDock->IsExpanded()
        || !diagnosticsDock->IsExpanded()
        || !diagnosticsAction->isChecked()
        || !inspector->isHidden()
        || window.m_sceneDocument.SceneRevision()
            != sceneRevision)
    {
        return fail(QStringLiteral(
            "13D-03 right-side task details did not replace "
            "the context inspector safely"));
    }
    diagnosticsDock->SetExpanded(false);
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (inspector->isHidden())
    {
        return fail(QStringLiteral(
            "13D-03 closing task details did not restore "
            "the context inspector"));
    }

    return pass(QStringLiteral(
        "workbench-project-diagnostics project=collapsed/"
        "advanced-actions diagnostics=right-side-alternate "
        "contexts=five"));
}

int UiSmokeTestRunner::WorkbenchLayoutRestore(
    const UiSmokeTestOptions& options)
{
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid())
    {
        return fail(QStringLiteral(
            "13D-04 temporary settings directory unavailable"));
    }
    const QString settingsPath =
        QDir(settingsDir.path()).filePath(
            QStringLiteral("layout.ini"));
    QSettings settings(
        settingsPath,
        QSettings::IniFormat);

    MainWindow source(options.repo_root);
    source.resize(1280, 720);
    source.show();
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    source.m_projectToolsDock->SetExpanded(true);
    source.m_diagnosticsDock->SetExpanded(true);
    source.m_contextInspector->SetCurrentPageIndex(3);
    source.m_contextInspectorToggleAction->setChecked(
        false);
    source.m_mainSplitter->setSizes(
        QList<int>{760, 300});
    WorkspaceLayoutState::Save(
        settings,
        &source,
        source.m_mainSplitter,
        source.m_contextInspector);

    MainWindow restored(options.repo_root);
    const bool restoredSavedState =
        WorkspaceLayoutState::Restore(
            settings,
            &restored,
            restored.m_mainSplitter,
            restored.m_contextInspector);
    restored.show();
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (!restoredSavedState
        || !restored.m_projectToolsDock->IsExpanded()
        || !restored.m_diagnosticsDock->IsExpanded()
        || !restored.m_contextInspector->isHidden()
        || restored.m_contextInspectorToggleAction
               ->isChecked()
        || restored.m_contextInspector
               ->CurrentPageIndex()
            != 3)
    {
        return fail(
            QStringLiteral(
                "13D-04 valid layout state was not restored: "
                "restored=%1 project=%2 diagnostics=%3 "
                "inspectorHidden=%4 action=%5 page=%6")
                .arg(restoredSavedState)
                .arg(
                    restored.m_projectToolsDock
                        ->IsExpanded())
                .arg(
                    restored.m_diagnosticsDock
                        ->IsExpanded())
                .arg(
                    restored.m_contextInspector
                        ->isHidden())
                .arg(
                    restored
                        .m_contextInspectorToggleAction
                        ->isChecked())
                .arg(
                    restored.m_contextInspector
                        ->CurrentPageIndex()));
    }

    settings.beginGroup(
        QStringLiteral("ui/layout"));
    settings.setValue(
        QStringLiteral("version"),
        0);
    settings.setValue(
        QStringLiteral("geometry"),
        QByteArray("corrupt"));
    settings.endGroup();
    settings.sync();

    MainWindow fallback(options.repo_root);
    const bool restoredCorruptState =
        WorkspaceLayoutState::Restore(
            settings,
            &fallback,
            fallback.m_mainSplitter,
            fallback.m_contextInspector);
    fallback.show();
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (restoredCorruptState
        || fallback.m_projectToolsDock->IsExpanded()
        || fallback.m_diagnosticsDock->IsExpanded()
        || fallback.m_contextInspector->isHidden()
        || !fallback.m_contextInspectorToggleAction
                ->isChecked()
        || fallback.m_contextInspector
               ->CurrentPageIndex()
            != 0)
    {
        return fail(QStringLiteral(
            "13D-04 invalid layout did not fall back safely"));
    }

    return pass(QStringLiteral(
        "workbench-layout-restore schema=2/"
        "valid-state/corrupt-state-safe-default"));
}

int UiSmokeTestRunner::Workbench1280x720(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    QWidget* actionBar =
        window.findChild<QWidget*>(
            QStringLiteral("sceneActionBar"));
    QWidget* workspace =
        window.findChild<QWidget*>(
            QStringLiteral("mainWorkspaceTabs"));
    ContextInspector* inspector =
        window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"));
    const QList<QPushButton*> jobButtons{
        window.findChild<QPushButton*>(
            QStringLiteral("jobImportModelsButton")),
        window.findChild<QPushButton*>(
            QStringLiteral("jobSaveSceneButton")),
        window.findChild<QPushButton*>(
            QStringLiteral("sliceCurrentSceneButton")),
        window.findChild<QPushButton*>(
            QStringLiteral(
                "cancelCurrentSceneSliceButton")),
    };
    if (actionBar == nullptr
        || workspace == nullptr
        || inspector == nullptr
        || jobButtons.contains(nullptr))
    {
        return fail(QStringLiteral(
            "13D-04 responsive workbench controls missing"));
    }
    for (QPushButton* button : jobButtons)
    {
        button->setEnabled(true);
    }
    const auto nextTabFocus =
        [](QWidget* current)
        {
            QWidget* candidate =
                current->nextInFocusChain();
            while (candidate != current)
            {
                if ((candidate->focusPolicy()
                        & Qt::TabFocus)
                    && candidate->isEnabled()
                    && !candidate->isHidden())
                {
                    return candidate;
                }
                candidate =
                    candidate->nextInFocusChain();
            }
            return current;
        };
    if (nextTabFocus(jobButtons.at(0))
            != jobButtons.at(1)
        || nextTabFocus(jobButtons.at(1))
            != jobButtons.at(2)
        || nextTabFocus(jobButtons.at(2))
            != jobButtons.at(3))
    {
        return fail(QStringLiteral(
            "13D-04 primary action keyboard order mismatch"));
    }

    const auto verifyLayout =
        [&window,
         actionBar,
         workspace,
         inspector,
         &jobButtons](const QString& scaleLabel)
        {
            window.resize(1280, 720);
            QApplication::processEvents(
                QEventLoop::AllEvents,
                50);
            if (window.width() > 1280
                || window.height() > 720
                || window.m_projectToolsDock->IsExpanded()
                || window.m_diagnosticsDock->IsExpanded())
            {
                return QStringLiteral(
                    "%1 default size or dock state mismatch")
                    .arg(scaleLabel);
            }

            const QRect actionRect =
                GlobalRect(actionBar);
            const QRect workspaceRect =
                GlobalRect(workspace);
            const QRect inspectorRect =
                GlobalRect(inspector);
            if (!actionBar->isVisibleTo(&window)
                || !workspace->isVisibleTo(&window)
                || !inspector->isVisibleTo(&window)
                || workspaceRect.intersects(inspectorRect)
                || actionRect.intersects(workspaceRect)
                || actionRect.intersects(inspectorRect)
                || workspaceRect.width() < 400
                || inspectorRect.width() < 240)
            {
                return QStringLiteral(
                    "%1 workbench regions overlap or collapse")
                    .arg(scaleLabel);
            }

            QList<QRect> buttonRects;
            for (QPushButton* button : jobButtons)
            {
                const QRect buttonRect =
                    GlobalRect(button);
                if (!button->isVisibleTo(&window)
                    || !actionRect.contains(buttonRect)
                    || (button->text().isEmpty()
                        && button->toolTip().isEmpty()))
                {
                    return QStringLiteral(
                        "%1 primary action is clipped")
                        .arg(scaleLabel);
                }
                for (const QRect& priorRect : buttonRects)
                {
                    if (priorRect.intersects(buttonRect))
                    {
                        return QStringLiteral(
                            "%1 primary actions overlap")
                            .arg(scaleLabel);
                    }
                }
                buttonRects.push_back(buttonRect);
            }
            return QString{};
        };

    window.show();
    QApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    QString layoutError =
        verifyLayout(QStringLiteral("100%"));
    if (!layoutError.isEmpty())
    {
        return fail(layoutError);
    }

    QFont highDpiFont = window.font();
    highDpiFont.setPointSizeF(
        highDpiFont.pointSizeF() * 1.5);
    window.setFont(highDpiFont);
    layoutError =
        verifyLayout(QStringLiteral("150%"));
    if (!layoutError.isEmpty())
    {
        return fail(layoutError);
    }

    return pass(QStringLiteral(
        "workbench-1280x720 100%-150%/"
        "actions-visible/no-overlap/docks-collapsed"));
}

int UiSmokeTestRunner::DiagnosticSettingsControls(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    ContextInspector* inspector =
        window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"));
    QDoubleSpinBox* widthSpin =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral(
                "diagnosticTextureSurfaceWidthSpin"));
    QSlider* widthSlider =
        window.findChild<QSlider*>(
            QStringLiteral(
                "diagnosticTextureSurfaceWidthSlider"));
    QComboBox* fillMaterial =
        window.findChild<QComboBox*>(
            QStringLiteral(
                "diagnosticModelFillMaterialCombo"));
    QLabel* subject =
        window.findChild<QLabel*>(
            QStringLiteral(
                "diagnosticSubjectSummaryLabel"));
    QLabel* bounds =
        window.findChild<QLabel*>(
            QStringLiteral(
                "diagnosticWidthBoundsLabel"));
    QLabel* backend =
        window.findChild<QLabel*>(
            QStringLiteral(
                "diagnosticBackendAvailabilityLabel"));
    QLabel* status =
        window.findChild<QLabel*>(
            QStringLiteral(
                "diagnosticStatusLabel"));
    QLabel* diagnosticOnlyNotice =
        window.findChild<QLabel*>(
            QStringLiteral(
                "diagnosticOnlyNoticeLabel"));
    QPushButton* startAnalysis =
        window.findChild<QPushButton*>(
            QStringLiteral(
                "diagnosticStartAnalysisButton"));
    QPushButton* cancelAnalysis =
        window.findChild<QPushButton*>(
            QStringLiteral(
                "diagnosticCancelAnalysisButton"));
    if (inspector == nullptr
        || widthSpin == nullptr
        || widthSlider == nullptr
        || fillMaterial == nullptr
        || subject == nullptr
        || bounds == nullptr
        || backend == nullptr
        || status == nullptr
        || diagnosticOnlyNotice == nullptr
        || startAnalysis == nullptr
        || cancelAnalysis == nullptr)
    {
        return fail(QStringLiteral(
            "12E-09A-03 diagnostic controls missing"));
    }
    if (widthSpin->decimals() != 2
        || std::abs(widthSpin->singleStep() - 0.01)
            > 1.0e-9
        || std::abs(widthSpin->minimum() - 0.10)
            > 1.0e-9
        || std::abs(widthSpin->maximum() - 6.00)
            > 1.0e-9
        || widthSlider->minimum() != 10
        || widthSlider->maximum() != 600)
    {
        return fail(QStringLiteral(
            "12E-09A-03 width range or precision mismatch"));
    }
    if (fillMaterial->findData(
            QStringLiteral("white"))
            < 0
        || fillMaterial->findData(
               QStringLiteral("varnish"))
            < 0
        || fillMaterial->findData(
               QStringLiteral("rgb"))
            < 0)
    {
        return fail(QStringLiteral(
            "12E-09A-03 model-fill material options missing"));
    }
    if (widthSpin->toolTip().isEmpty()
        || widthSlider->toolTip().isEmpty()
        || fillMaterial->toolTip().isEmpty()
        || startAnalysis->toolTip().isEmpty()
        || cancelAnalysis->toolTip().isEmpty()
        || subject->text().isEmpty()
        || bounds->text().isEmpty()
        || backend->text().isEmpty()
        || status->text().isEmpty()
        || !diagnosticOnlyNotice->text().contains(
            QStringLiteral("不会修改生产")))
    {
        return fail(QStringLiteral(
            "12E-09A-03 Chinese status or tooltip missing"));
    }
    widthSpin->setValue(0.23);
    if (widthSlider->value() != 23)
    {
        return fail(QStringLiteral(
            "12E-09A-03 spinbox did not update slider"));
    }
    widthSlider->setValue(41);
    if (std::abs(widthSpin->value() - 0.41)
        > 1.0e-9)
    {
        return fail(QStringLiteral(
            "12E-09A-03 slider did not update spinbox"));
    }
    if (widthSpin->isEnabled()
        || widthSlider->isEnabled()
        || fillMaterial->isEnabled())
    {
        return fail(QStringLiteral(
            "12E-09A-03 controls must be disabled without a model"));
    }
    if (startAnalysis->isEnabled()
        || cancelAnalysis->isEnabled())
    {
        return fail(QStringLiteral(
            "12E-09A-04 actions must be disabled without a model"));
    }

    DiagnosticSettingsPresentation presentation;
    presentation.subjectsummary =
        QStringLiteral(
            "场景 这是一个用于验证最长中文排版的诊断场景身份 / "
            "revision 123456 / 当前实例 "
            "instance-with-a-long-readable-identity");
    presentation.minimumwidthmm = 0.12;
    presentation.maximumwidthmm = 5.43;
    presentation.alltexturethresholdmm = 2.34;
    presentation.backendavailability =
        QStringLiteral(
            "Legacy CPU 可用；OpenVDB 候选后端可用，"
            "但诊断可用不等同于生产准入。");
    presentation.status =
        QStringLiteral(
            "诊断参数已经准备完成；当前仅验证中文参数、状态和 tooltip，"
            "尚未启动异步拓扑、距离场、纹理转移或栅格映射分析。");
    presentation.blockingreasons = QStringList{
        QStringLiteral(
            "这是最长中文阻断原因示例，用于确认窗口缩放时文本不会覆盖相邻控件。")};
    presentation.controlsenabled = true;
    inspector->SetDiagnosticPresentation(
        presentation);
    inspector->ShowTextureDiagnosticPage();
    widthSpin->setValue(0.37);
    const int varnishIndex =
        fillMaterial->findData(
            QStringLiteral("varnish"));
    fillMaterial->setCurrentIndex(varnishIndex);
    if (!widthSpin->isEnabled()
        || !widthSlider->isEnabled()
        || !fillMaterial->isEnabled()
        || !startAnalysis->isEnabled()
        || cancelAnalysis->isEnabled()
        || std::abs(
               window
                   .m_diagnosticTextureSurfaceWidthMm
               - 0.37)
            > 1.0e-9
        || window.m_diagnosticModelFillMaterial
            != QStringLiteral("varnish")
        || !bounds->text().contains(
            QStringLiteral("0.12 mm"))
        || !bounds->text().contains(
            QStringLiteral("5.43 mm"))
        || !bounds->text().contains(
            QStringLiteral("2.34 mm")))
    {
        return fail(QStringLiteral(
            "12E-09A-03 available presentation or edits mismatch"));
    }
    if (std::abs(widthSpin->minimum() - 0.12)
            > 1.0e-9
        || std::abs(widthSpin->maximum() - 5.43)
            > 1.0e-9
        || widthSlider->minimum() != 12
        || widthSlider->maximum() != 543)
    {
        return fail(QStringLiteral(
            "12E-09A-03 derived width bounds not applied"));
    }

    presentation.analysisrunning = true;
    presentation.status =
        QStringLiteral(
            "运行中（running）：后台正在执行拓扑、距离、"
            "纹理分区和栅格映射。");
    inspector->SetDiagnosticPresentation(presentation);
    if (widthSpin->isEnabled()
        || widthSlider->isEnabled()
        || fillMaterial->isEnabled()
        || startAnalysis->isEnabled()
        || !cancelAnalysis->isEnabled()
        || !status->text().contains(
            QStringLiteral("运行中")))
    {
        return fail(QStringLiteral(
            "12E-09A-04 running action state mismatch"));
    }
    presentation.analysisrunning = false;
    inspector->SetDiagnosticPresentation(presentation);

    presentation.maximumwidthmm.reset();
    presentation.alltexturethresholdmm.reset();
    presentation.status =
        QStringLiteral(
            "诊断失败：strict_closed rejected mesh with non-manifold edges");
    inspector->SetDiagnosticPresentation(presentation);
    if (!bounds->text().contains(
            QStringLiteral("最大 未评估"))
        || !bounds->text().contains(
            QStringLiteral("全纹理阈值 未评估"))
        || bounds->text().contains(
            QStringLiteral("最大 0.00 mm")))
    {
        return fail(QStringLiteral(
            "12E-09A diagnostic failure published a false zero width bound"));
    }

    window.show();
    const QList<QSize> sizes{
        QSize(1280, 720),
        QSize(1440, 900),
        QSize(1920, 1080),
    };
    const QList<QWidget*> statusWidgets{
        subject,
        bounds,
        backend,
        status,
    };
    for (const QSize& size : sizes)
    {
        window.resize(size);
        QApplication::processEvents(
            QEventLoop::AllEvents,
            50);
        const QRect inspectorRect =
            GlobalRect(inspector);
        for (QWidget* widget : statusWidgets)
        {
            if (!widget->isVisibleTo(&window)
                || !inspectorRect.contains(
                    GlobalRect(widget)))
            {
                return fail(
                    QStringLiteral(
                        "12E-09A-03 Chinese status clipped at %1x%2")
                        .arg(size.width())
                        .arg(size.height()));
            }
        }
    }

    return pass(QStringLiteral(
        "diagnostic-settings-controls Chinese/"
        "0.01mm/bidirectional/materials/"
        "unavailable/three-sizes"));
}

int UiSmokeTestRunner::ProductionTextureControls(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    ContextInspector* inspector =
        window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"));
    QWidget* sliceSettingsPage =
        window.findChild<QWidget*>(
            QStringLiteral("contextSliceSettingsPage"));
    QWidget* diagnosticPanel =
        window.findChild<QWidget*>(
            QStringLiteral("diagnosticSettingsPanel"));
    QSpinBox* legacyLayers =
        window.findChild<QSpinBox*>(
            QStringLiteral("productionLegacyTopLayersSpin"));
    QDoubleSpinBox* globalWidth =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("productionGlobalTextureWidthSpin"));
    QComboBox* globalMode =
        window.findChild<QComboBox*>(
            QStringLiteral("productionGlobalTextureModeCombo"));
    QComboBox* singleMaterial =
        window.findChild<QComboBox*>(
            QStringLiteral("productionSingleMaterialCombo"));
    QLabel* stateLabel =
        window.findChild<QLabel*>(
            QStringLiteral("productionSettingsStateLabel"));
    QLabel* noticeLabel =
        window.findChild<QLabel*>(
            QStringLiteral("productionSettingsNoticeLabel"));
    if (inspector == nullptr
        || sliceSettingsPage == nullptr
        || diagnosticPanel == nullptr
        || legacyLayers == nullptr
        || globalWidth == nullptr
        || globalMode == nullptr
        || singleMaterial == nullptr
        || stateLabel == nullptr
        || noticeLabel == nullptr)
    {
        return fail(QStringLiteral(
            "12E-09D-04 production controls missing"));
    }
    if (sliceSettingsPage->isAncestorOf(diagnosticPanel)
        || !noticeLabel->text().contains(QStringLiteral("诊断宽度")))
    {
        return fail(QStringLiteral(
            "12E-09D-04 diagnostic and production controls are not separated"));
    }

    const ScenarioEntry* legacyScenario =
        window.m_scenarioRegistry.FindById(
            QStringLiteral("textured_nail_rgb_white_lower_support"));
    if (legacyScenario == nullptr)
    {
        return fail(QStringLiteral(
            "12E-09D-04 legacy scenario missing"));
    }
    window.ApplyScenario(*legacyScenario);
    QApplication::processEvents();
    legacyLayers->setValue(3);
    QApplication::processEvents();
    const SliceSettingsState legacySettings =
        window.BuildCurrentSettings(
            {},
            {},
            SliceEngineRole::LegacyProduction);
    if (!legacyLayers->isEnabled()
        || window.config_document_.value(
               {"texture", "topSurfaceLayers"})
               .toInt()
            != 3
        || !legacySettings.productiontextureoverrideenabled
        || legacySettings.productiontexture.effectivetoplayers != 3
        || std::abs(
               legacySettings.productiontexture
                       .effectivetopthicknessmm
                   - 3.0 * legacySettings.layerthicknessmm)
            > 1.0e-9
        || !window.config_document_.isDirty()
        || !stateLabel->text().contains(QStringLiteral("stale")))
    {
        return fail(QStringLiteral(
            "12E-09D-04 legacy edit/effective/stale mismatch"));
    }
    const EffectiveConfigResult legacyEffective =
        window.GenerateEffectiveConfig(
            {},
            {},
            SliceEngineRole::LegacyProduction,
            QStringLiteral("ui_smoke_09d_legacy"));
    if (!legacyEffective.IsValid()
        || legacyEffective.document.object()
               .value(QStringLiteral("texture"))
               .toObject()
               .value(QStringLiteral("topSurfaceLayers"))
               .toInt()
            != 3
        || !legacyEffective.summary.contains(
            QStringLiteral("生产纹理：legacy_top_band / 3 层")))
    {
        return fail(QStringLiteral(
            "12E-09D-05 legacy one-click effective config mismatch: ")
            + legacyEffective.errors.join(QStringLiteral("；")));
    }

    QTemporaryDir saveDirectory;
    const QString savedPath = saveDirectory.filePath(
        QStringLiteral("production-settings.json"));
    SaveOptions saveOptions;
    saveOptions.allowOverwriteWithoutPrompt = true;
    if (!window.config_document_.saveAs(
            savedPath,
            nullptr,
            saveOptions))
    {
        return fail(QStringLiteral(
            "12E-09D-04 save failed: ")
            + window.config_document_.errorString());
    }
    ConfigDocument reloaded;
    if (!reloaded.load(savedPath)
        || reloaded.value(
               {"texture", "topSurfaceLayers"})
               .toInt()
            != 3)
    {
        return fail(QStringLiteral(
            "12E-09D-04 save/readback mismatch"));
    }

    const ScenarioEntry* singleScenario =
        window.m_scenarioRegistry.FindById(
            QStringLiteral("single_material_relief"));
    if (singleScenario == nullptr)
    {
        return fail(QStringLiteral(
            "12E-09D-04 single-material scenario missing"));
    }
    window.ApplyScenario(*singleScenario);
    QApplication::processEvents();
    const int varnishIndex = singleMaterial->findData(
        static_cast<int>(SingleMaterialReliefMaterial::Varnish));
    singleMaterial->setCurrentIndex(varnishIndex);
    QApplication::processEvents();
    const SliceSettingsState singleSettings =
        window.BuildCurrentSettings(
            {},
            {},
            SliceEngineRole::LegacyProduction);
    if (!singleMaterial->isEnabled()
        || window.config_document_.value(
               {"modelMaterial", "materialChannel"})
               .toString()
            != QStringLiteral("V")
        || window.config_document_.value(
               {"modelMaterial", "whiteValue"})
               .toInt()
            != 255
        || window.config_document_.value(
               {"modelMaterial", "varnishValue"})
               .toInt()
            != 0
        || !singleSettings.singlematerialreliefoverrideenabled
        || singleSettings.singlematerialrelief.effectivechannel
            != QStringLiteral("V"))
    {
        return fail(QStringLiteral(
            "12E-09D-04 single-material atomic edit mismatch"));
    }
    const EffectiveConfigResult singleEffective =
        window.GenerateEffectiveConfig(
            {},
            {},
            SliceEngineRole::LegacyProduction,
            QStringLiteral("ui_smoke_09d_single"));
    if (!singleEffective.IsValid()
        || singleEffective.document.object()
               .value(QStringLiteral("modelMaterial"))
               .toObject()
               .value(QStringLiteral("materialChannel"))
               .toString()
            != QStringLiteral("V")
        || !singleEffective.summary.contains(
            QStringLiteral("单材料浮雕：varnish / channel=V")))
    {
        return fail(QStringLiteral(
            "12E-09D-05 single-material one-click effective config mismatch: ")
            + singleEffective.errors.join(QStringLiteral("；")));
    }

    QComboBox* productionMode =
        window.findChild<QComboBox*>(
            QStringLiteral("productionModeCombo"));
    QComboBox* productionProfile =
        window.findChild<QComboBox*>(
            QStringLiteral("productionProfileCombo"));
    if (productionMode == nullptr
        || productionProfile == nullptr)
    {
        return fail(QStringLiteral(
            "12E-09D-04 production selector missing"));
    }
    const int globalModeIndex = productionMode->findData(
        QStringLiteral("global_surface_shell"));
    productionMode->setCurrentIndex(globalModeIndex);
    QApplication::processEvents();
    const int globalProfileIndex = productionProfile->findData(
        QStringLiteral(
            "global_surface_shell_material_parity_candidate"));
    productionProfile->setCurrentIndex(globalProfileIndex);
    QApplication::processEvents();
    globalWidth->setValue(0.37);
    QApplication::processEvents();
    const int allTextureIndex = globalMode->findData(
        static_cast<int>(ProductionTexturePartitionMode::AllTexture));
    globalMode->setCurrentIndex(allTextureIndex);
    QApplication::processEvents();
    const SliceSettingsState globalSettings =
        window.BuildCurrentSettings(
            {},
            {},
            SliceEngineRole::LegacyProduction);
    const QJsonObject storedGlobal =
        window.config_document_.document()
            .object()
            .value(QStringLiteral("uiProductionSettings"))
            .toObject()
            .value(QStringLiteral("globalSurfaceShellOverrides"))
            .toObject()
            .value(QStringLiteral(
                "global_surface_shell_material_parity_candidate"))
            .toObject();
    if (!globalMode->isEnabled()
        || globalWidth->isEnabled()
        || storedGlobal.value(QStringLiteral("mode")).toString()
            != QStringLiteral("all_texture")
        || std::abs(
               storedGlobal.value(QStringLiteral("widthMm")).toDouble()
                   - 0.37)
            > 1.0e-9
        || !globalSettings.productiontextureoverrideenabled
        || globalSettings.productiontexture.partitionmode
            != ProductionTexturePartitionMode::AllTexture)
    {
        return fail(QStringLiteral(
            "12E-09D-04 global edit/persistence mismatch"));
    }
    const EffectiveConfigResult globalEffective =
        window.GenerateEffectiveConfig(
            {},
            {},
            SliceEngineRole::LegacyProduction,
            QStringLiteral("ui_smoke_09d_global"));
    const QJsonObject globalSurfaceShell = globalEffective.document
        .object()
        .value(QStringLiteral("texture"))
        .toObject()
        .value(QStringLiteral("surfaceShell"))
        .toObject();
    if (!globalEffective.IsValid()
        || globalSurfaceShell.value(QStringLiteral("mode")).toString()
            != QStringLiteral("all_texture")
        || std::abs(
               globalSurfaceShell.value(QStringLiteral("widthMm")).toDouble()
                   - 0.37)
            > 1.0e-9
        || !globalEffective.summary.contains(
            QStringLiteral(
                "生产纹理：global_surface_shell / all_texture")))
    {
        return fail(QStringLiteral(
            "12E-09D-05 Global one-click effective config mismatch: ")
            + globalEffective.errors.join(QStringLiteral("；")));
    }

    return pass(QStringLiteral(
        "production-texture-controls legacy/global/single/"
        "diagnostic-separated/stale/save-readback/one-click-effective"));
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
    if (model.State().dpix != slicer_core::kDefaultOutputDpiX
        || model.State().dpiy != slicer_core::kDefaultOutputDpiY
        || std::abs(
               model.State().layerthicknessmm
               - slicer_core::kDefaultLayerThicknessMm)
            > 1.0e-9
        || model.State().modelfillmaterial != ModelFillMaterial::Rgb)
    {
        return fail(
            "slice-settings-model 系统默认值不是 "
            "635x600 DPI、0.038 mm 层高和全 RGB。");
    }
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
            || std::abs(
                   defaults.layerthicknessmm
                   - slicer_core::kDefaultLayerThicknessMm)
                > 1.0e-9
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
    const SliceSettingsValidationResult validValidation =
        model.Validate();
    if (!validValidation.IsValid())
    {
        return fail("slice-settings-model 安全 legacy 设置未通过校验。");
    }
    SliceSettingsState rgbOnlyState = validState;
    rgbOnlyState.modelfillmaterial = ModelFillMaterial::Rgb;
    model.SetState(rgbOnlyState);
    const SliceSettingsValidationResult rgbOnlyValidation =
        model.Validate();
    if (!rgbOnlyValidation.IsValid()
        || !rgbOnlyValidation.warnings.join(QStringLiteral(" ")).contains(
            QStringLiteral("纯白")))
    {
        return fail(
            "slice-settings-model 全实体 RGB 兼容模式未提示纯白像素的 RGBWSV 协议限制。");
    }
    model.SetState(validState);

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

    return pass(
        "slice-settings-model profiles=5 diagnostics-default=false openvdb=candidate-only");
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
        QStringLiteral("preview.outputPolicy"),
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
        {QStringLiteral("baseProjectionEnabledCheck"), QStringLiteral("support.baseProjection.enabled")},
        {QStringLiteral("baseProjectionLayerCountSpin"), QStringLiteral("support.baseProjection.layerCount")},
        {QStringLiteral("surfaceVarnishEnabledCheck"), QStringLiteral("surfaceVarnish.enabled")},
        {QStringLiteral("outerVarnishEnabledCheck"), QStringLiteral("outerVarnish.enabled")},
        {QStringLiteral("previewDiagnosticImagesCheck"), QStringLiteral("preview.enabled")},
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
    QDoubleSpinBox* layerHeightSpin =
        quickPanel.findChild<QDoubleSpinBox*>(
            QStringLiteral("layerHeightSpin"));
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
        || layerHeightSpin == nullptr
        || outputPixelSizeLabel == nullptr
        || outputDpiXSpin->minimum() != 72
        || outputDpiXSpin->maximum() != 2400
        || outputDpiYSpin->minimum() != 72
        || outputDpiYSpin->maximum() != 2400
        || outputDpiXSpin->value() != 635
        || outputDpiYSpin->value() != 600
        || std::abs(
               layerHeightSpin->value()
               - slicer_core::kDefaultLayerThicknessMm)
            > 1.0e-9
        || !ContainsAll(
            outputPixelSizeLabel->text(),
            {QStringLiteral("X 0.040000 mm/px"),
             QStringLiteral("Y 0.042333 mm/px")}))
    {
        return fail(QStringLiteral("setting-help-metadata X/Y DPI 默认值或物理像素提示错误。"));
    }
    const QList<QAbstractSpinBox*> numericInputs =
        quickPanel.findChildren<QAbstractSpinBox*>();
    if (numericInputs.isEmpty())
    {
        return fail(
            QStringLiteral(
                "setting-help-metadata 未找到可编辑数值输入框。"));
    }
    for (const QAbstractSpinBox* input : numericInputs)
    {
        if (input->keyboardTracking())
        {
            return fail(
                QStringLiteral(
                    "setting-help-metadata 数值输入仍会逐字符提交：")
                + input->objectName());
        }
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
    auto* semantic = workspace.findChild<DiagnosticSemanticPreviewPanel*>(
        QStringLiteral("diagnosticSemanticPreviewPanel"));
    auto* overlay = workspace.findChild<PreviewOverlayPanel*>(QStringLiteral("materialOverlayView"));
    auto* raw = workspace.findChild<PreviewPanel*>(QStringLiteral("rawPreviewView"));
    if (production == nullptr || semantic == nullptr || overlay == nullptr || raw == nullptr)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 缺少生产或诊断预览面板。"));
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

    workspace.SetMode(PreviewWorkspaceMode::Diagnostic);
    workspace.SetDiagnosticMode(
        DiagnosticPreviewMode::MaterialOverlay);
    workspace.SetDiagnosticMode(
        DiagnosticPreviewMode::RawPreview);
    workspace.SetMode(PreviewWorkspaceMode::Production);
    if (workspace.CurrentLayerIndex() != previewLayer
        || production->CurrentLayerIndex() != previewLayer
        || overlay->CurrentLayerIndex() != previewLayer
        || raw->CurrentLayerIndex() != previewLayer)
    {
        return fail(QStringLiteral("preview-workspace-shared-layer 模式切换改变了真实层号。"));
    }

    QComboBox* rawChannel = raw->findChild<QComboBox*>(QStringLiteral("rawPreviewChannelSelector"));
    QComboBox* overlayMode = overlay->findChild<QComboBox*>(QStringLiteral("overlayModeSelector"));
    QComboBox* workspaceMode =
        workspace.findChild<QComboBox*>(
            QStringLiteral(
                "previewWorkspaceModeSelector"));
    QComboBox* diagnosticMode =
        workspace.findChild<QComboBox*>(
            QStringLiteral(
                "diagnosticPreviewModeSelector"));
    if (rawChannel == nullptr
        || overlayMode == nullptr
        || workspaceMode == nullptr
        || diagnosticMode == nullptr
        || workspaceMode->count() != 2
        || diagnosticMode->count() != 3
        || diagnosticMode->findData(
               static_cast<int>(DiagnosticPreviewMode::TextureFillSemantics))
            < 0)
    {
        return fail(
            QStringLiteral(
                "preview-workspace-shared-layer 未收敛为生产/诊断两个一级入口，或缺少诊断子入口。"));
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
        QStringLiteral("preview-workspace-shared-layer layers=%1 rawSparse=%2 overlaySparse=%3 preview=%4 primaryModes=2 diagnosticModes=3")
            .arg(workspace.LayerIndices().size())
            .arg(sparseLayer)
            .arg(overlaySparseLayer)
            .arg(previewLayer));
}

int UiSmokeTestRunner::TiffNativePreviewAllMaterials(
    const UiSmokeTestOptions& options)
{
    const QString packagePath =
        absoluteFromRepo(
            options,
            options.package_path);
    PackageSummary package =
        PackageLoader().load(packagePath);
    if (package.manifest_path.isEmpty())
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 未找到 manifest：")
            + packagePath);
    }

    // Production preview must remain usable even when its package summary
    // does not expose any preview PNG paths.
    package.preview_paths.clear();

    PreviewWorkspace workspace;
    workspace.LoadPackage(package);
    auto* production =
        workspace.findChild<LayerPreviewPanel*>(
            QStringLiteral("productionLayerView"));
    auto* workspaceMode =
        workspace.findChild<QComboBox*>(
            QStringLiteral(
                "previewWorkspaceModeSelector"));
    auto* diagnosticMode =
        workspace.findChild<QComboBox*>(
            QStringLiteral(
                "diagnosticPreviewModeSelector"));
    auto* overlay =
        workspace.findChild<PreviewOverlayPanel*>(
            QStringLiteral("materialOverlayView"));
    auto* semantic =
        workspace.findChild<DiagnosticSemanticPreviewPanel*>(
            QStringLiteral("diagnosticSemanticPreviewPanel"));
    auto* raw =
        workspace.findChild<PreviewPanel*>(
            QStringLiteral("rawPreviewView"));
    if (production == nullptr
        || workspaceMode == nullptr
        || diagnosticMode == nullptr
        || overlay == nullptr
        || semantic == nullptr
        || raw == nullptr
        || workspaceMode->count() != 2
        || diagnosticMode->count() != 3)
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 统一生产/诊断入口不完整。"));
    }

    const QVector<int> layerIndices =
        production->LayerIndices();
    if (layerIndices.size() < 3
        || production->DataSourceForTest()
            != QStringLiteral(
                "manifest/layers RGBWSV TIFF"))
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 未使用 manifest 权威 TIFF 层源。"));
    }

    const QStringList requiredModes{
        QStringLiteral("rgb"),
        QStringLiteral("red"),
        QStringLiteral("green"),
        QStringLiteral("blue"),
        QStringLiteral("white"),
        QStringLiteral("support"),
        QStringLiteral("varnish"),
        QStringLiteral("rgb_white"),
        QStringLiteral("rgb_support"),
        QStringLiteral("rgb_varnish"),
        QStringLiteral(
            "rgb_support_white_varnish"),
        QStringLiteral("occupancy"),
        QStringLiteral("empty"),
    };
    for (const QString& mode : requiredModes)
    {
        if (!production->AvailableChannels().contains(
                mode))
        {
            return fail(
                QStringLiteral(
                    "tiff-native-preview-all-materials 缺少模式：")
                + mode);
        }
    }

    const QList<int> requestedLayers{
        layerIndices.first(),
        layerIndices.at(layerIndices.size() / 2),
        layerIndices.last(),
    };
    for (const int layerIndex : requestedLayers)
    {
        if (!workspace.SelectLayer(layerIndex)
            || !WaitForCondition(
                [production, layerIndex]()
                {
                    return production
                               ->IsLayerReadyForTest()
                        && production
                               ->LoadedLayerIndexForTest()
                            == layerIndex;
                }))
        {
            return fail(
                QStringLiteral(
                    "tiff-native-preview-all-materials 异步切层失败：layer=%1 status=%2")
                    .arg(layerIndex)
                    .arg(production->StatusForTest()));
        }
        if (!ContainsAll(
                production->StatusForTest(),
                {
                    QStringLiteral("layer=%1")
                        .arg(layerIndex),
                    QStringLiteral("z="),
                    QStringLiteral("DPI="),
                    QStringLiteral(
                        "层序=低Z->高Z"),
                    QStringLiteral(
                        "数据源=manifest/layers TIFF"),
                    QStringLiteral("cache="),
                }))
        {
            return fail(
                QStringLiteral(
                    "tiff-native-preview-all-materials 层元数据状态不完整：")
                + production->StatusForTest());
        }
    }

    const quint64 requestsBeforeModes =
        production->LayerRequestCountForTest();
    for (const QString& mode : requiredModes)
    {
        if (!production->SelectChannelForTest(mode)
            || production->CurrentImageForTest().isNull())
        {
            return fail(
                QStringLiteral(
                    "tiff-native-preview-all-materials 合成失败：mode=")
                + mode);
        }
    }
    if (production->LayerRequestCountForTest()
        != requestsBeforeModes)
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 模式切换重复读取了 TIFF。"));
    }

    const QImage image =
        production->CurrentImageForTest();
    const QString probe =
        production->ProbePixelForTest(
            image.width() / 2,
            image.height() / 2);
    if (!ContainsAll(
            probe,
            {
                QStringLiteral("display=("),
                QStringLiteral("raw=("),
                QStringLiteral("生产值 RGBWSV=("),
                QStringLiteral(
                    "black_is_print/0打印/255不打印"),
            }))
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 六通道探针不完整：")
            + probe);
    }

    const int rapidFirst = layerIndices.at(1);
    const int rapidMiddle =
        layerIndices.at(layerIndices.size() / 3);
    const int rapidLast =
        layerIndices.at(layerIndices.size() - 2);
    production->SelectLayer(rapidFirst);
    production->SelectLayer(rapidMiddle);
    production->SelectLayer(rapidLast);
    if (!WaitForCondition(
            [production, rapidLast]()
            {
                return production->IsLayerReadyForTest()
                    && production
                           ->LoadedLayerIndexForTest()
                        == rapidLast;
            })
        || production->CurrentLayerIndex()
            != rapidLast)
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials stale generation 覆盖了最新层：")
            + production->StatusForTest());
    }

    workspace.SetMode(
        PreviewWorkspaceMode::Diagnostic);
    workspace.SetDiagnosticMode(
        DiagnosticPreviewMode::MaterialOverlay);
    workspace.SetDiagnosticMode(
        DiagnosticPreviewMode::RawPreview);
    workspace.SetMode(
        PreviewWorkspaceMode::Production);
    if (workspace.CurrentLayerIndex() != rapidLast
        || workspace.CurrentMode()
            != PreviewWorkspaceMode::Production)
    {
        return fail(
            QStringLiteral(
                "tiff-native-preview-all-materials 模式切换改变了真实层。"));
    }

    return pass(
        QStringLiteral(
            "tiff-native-preview-all-materials layers=%1 modes=%2 requests=%3 source=TIFF primaryModes=2 diagnosticModes=3")
            .arg(layerIndices.size())
            .arg(requiredModes.size())
            .arg(
                production
                    ->LayerRequestCountForTest()));
}

int UiSmokeTestRunner::TiffNativePreviewNoPng(
    const UiSmokeTestOptions& options)
{
    const QString packagePath =
        absoluteFromRepo(options, options.package_path);
    const QDir packageDirectory(packagePath);
    QFile manifestFile(packageDirectory.filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return fail(
            QStringLiteral("tiff-native-preview-no-png 未找到 manifest：")
            + packagePath);
    }

    QJsonParseError parseError;
    const QJsonDocument manifestDocument =
        QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    const QJsonObject preview =
        manifestDocument.object().value(QStringLiteral("preview")).toObject();
    if (parseError.error != QJsonParseError::NoError
        || preview.value(QStringLiteral("outputPolicy")).toString()
            != QStringLiteral("tiff_native")
        || preview.value(QStringLiteral("productionSource")).toString()
            != QStringLiteral("rgbwsv_tiff")
        || preview.value(QStringLiteral("automaticDiagnosticImages")).toBool(true)
        || !preview.value(QStringLiteral("files")).toArray().isEmpty())
    {
        return fail(
            QStringLiteral("tiff-native-preview-no-png manifest 预览合同不完整。"));
    }
    if (packageDirectory.exists(QStringLiteral("preview")))
    {
        return fail(
            QStringLiteral("tiff-native-preview-no-png 不应生成 preview 目录。"));
    }

    const int previewResult = TiffNativePreviewAllMaterials(options);
    if (previewResult != 0)
    {
        return previewResult;
    }
    return pass(
        QStringLiteral(
            "tiff-native-preview-no-png policy=tiff_native previewDir=absent source=RGBWSV_TIFF"));
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

int UiSmokeTestRunner::DiagnosticSemanticPreview(
    const UiSmokeTestOptions& options)
{
    Q_UNUSED(options);

    constexpr std::size_t channelCount{6U};
    auto evidence = std::make_shared<
        slicer_core::
            TextureFillPartitionReleaseBenchmarkResult>();
    auto& partition = evidence->partition;
    partition.available = true;
    partition.partitionPass = true;
    partition.status = "diagnostic";
    partition.grid.width = 4;
    partition.grid.height = 3;
    partition.grid.depth = 1;
    partition.grid.originXMm = 1.0;
    partition.grid.originYMm = 2.0;
    partition.grid.originZMm = 3.0;
    partition.grid.spacingXMm = 0.04;
    partition.grid.spacingYMm = 0.05;
    partition.grid.spacingZMm = 0.10;
    partition.widthMetrics.effectiveWidthMm = 0.20;
    partition.widthMetrics.allTexture = false;
    partition.modelMask.grid = partition.grid;
    partition.textureSurfaceMask.grid = partition.grid;
    partition.modelFillMask.grid = partition.grid;
    partition.modelMask.values.assign(12U, 0U);
    partition.textureSurfaceMask.values.assign(12U, 0U);
    partition.modelFillMask.values.assign(12U, 0U);
    partition.modelMask.values.at(5U) = 1U;
    partition.textureSurfaceMask.values.at(5U) = 1U;
    partition.modelMask.values.at(6U) = 1U;
    partition.modelFillMask.values.at(6U) = 1U;

    DiagnosticAnalysisResult analysis;
    analysis.state = DiagnosticAnalysisState::Succeeded;
    analysis.identity.sessionid =
        QStringLiteral("diagnostic-session");
    analysis.identity.sceneid =
        QStringLiteral("scene-semantic-smoke");
    analysis.identity.modelid =
        QStringLiteral("model-a");
    analysis.identity.instanceid =
        QStringLiteral("instance-a");
    analysis.identity.confighash =
        QStringLiteral("0123456789abcdef");
    analysis.identity.scenerevision = 7U;
    analysis.identity.transformrevision = 2U;
    analysis.evidence = evidence;

    auto layer =
        std::make_shared<slicer_core::RgbwsvLayerBuffer>();
    layer->sourceIdentity = "semantic-smoke-layer";
    layer->layerIndex = 37;
    layer->zMm = 3.05;
    layer->width = 4U;
    layer->height = 3U;
    layer->dpiX = 635;
    layer->dpiY = 508;
    layer->originxmm = 1.0;
    layer->originymm = 2.0;
    layer->originzmm = 3.0;
    layer->pixelsizexmm = 0.04;
    layer->pixelsizeymm = 0.05;
    layer->layerthicknessmm = 0.10;
    layer->sceneidentityavailable = true;
    layer->sceneid = "scene-semantic-smoke";
    layer->scenerevision = 7U;
    layer->pixels.assign(
        4U * 3U * channelCount,
        255U);
    layer->pixels.at(10U * channelCount + 3U) = 0U;
    layer->pixels.at(4U) = 0U;
    layer->pixels.at(11U * channelCount + 5U) =
        127U;

    MaterialClosureDiagnosticsSummary closureSummary;
    closureSummary.reportavailable = true;
    closureSummary.schemavalid = true;
    closureSummary.confidence = QStringLiteral("exact");
    closureSummary.closurestatus = QStringLiteral("pass");
    closureSummary.productionacceptance = QStringLiteral("passed");
    MaterialClosureLayerUi closureLayer;
    closureLayer.layerindex = 37;
    closureLayer.zmm = 3.05;
    closureLayer.closurestatus = QStringLiteral("pass");
    closureSummary.layers.push_back(closureLayer);

    DiagnosticSemanticPreviewPanel panel;
    panel.resize(640, 480);
    panel.SetMaterialClosureSummary(closureSummary);
    panel.SetDiagnosticAnalysis(analysis);
    panel.SetProductionLayer(layer);
    QApplication::processEvents();

    if (panel.CurrentImageForTest().isNull()
        || panel.LayerIndexForTest() != 37
        || !ContainsAll(
            panel.StatusForTest(),
            {QStringLiteral("同层 layer=37"),
             QStringLiteral("z=3.050 mm"),
             QStringLiteral("Texture=1"),
             QStringLiteral("Fill=1"),
             QStringLiteral("W=1"),
             QStringLiteral("S=1"),
             QStringLiteral("V=1"),
             QStringLiteral("width=0.20 mm"),
             QStringLiteral("scene 身份已匹配"),
             QStringLiteral("材料闭环=通过（gap=0）")}))
    {
        return fail(
            QStringLiteral(
                "diagnostic-semantic-preview 同层状态或图像不完整：")
            + panel.StatusForTest());
    }

    if (!panel.SetDisplayModeForTest(
            DiagnosticSemanticDisplayMode::
                TextureSurface)
        || panel.CurrentImageForTest().pixelColor(1, 1)
            != QColor(0, 151, 167)
        || !panel.SetDisplayModeForTest(
            DiagnosticSemanticDisplayMode::ModelFill)
        || panel.CurrentImageForTest().pixelColor(2, 1)
            != QColor(230, 126, 34))
    {
        return fail(
            QStringLiteral(
                "diagnostic-semantic-preview Texture/Fill 伪彩分区不正确。"));
    }

    auto staleLayer =
        std::make_shared<slicer_core::RgbwsvLayerBuffer>(
            *layer);
    staleLayer->scenerevision = 8U;
    panel.SetProductionLayer(staleLayer);
    if (!panel.CurrentImageForTest().isNull()
        || !panel.StatusForTest().contains(
            QStringLiteral("sceneId/revision 与当前诊断身份不一致")))
    {
        return fail(
            QStringLiteral(
                "diagnostic-semantic-preview 未拒绝 stale scene 身份。"));
    }

    DiagnosticAnalysisResult missing;
    missing.state = DiagnosticAnalysisState::Failed;
    panel.SetDiagnosticAnalysis(missing);
    if (!panel.CurrentImageForTest().isNull()
        || !panel.StatusForTest().contains(
            QStringLiteral("未评估")))
    {
        return fail(
            QStringLiteral(
                "diagnostic-semantic-preview 缺证据状态不明确。"));
    }

    return pass(
        QStringLiteral(
            "diagnostic-semantic-preview layer=37 Texture=1 Fill=1 W=1 S=1 V=1 stale=blocked"));
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
            if (!WaitForCondition(
                    [production, layerIndex]()
                    {
                        return production->IsLayerReadyForTest()
                            && production
                                   ->LoadedLayerIndexForTest()
                                == layerIndex;
                    }))
            {
                return {};
            }
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
    if (!WaitForCondition(
            [production]()
            {
                return production->IsLayerReadyForTest()
                    && production->LoadedLayerIndexForTest()
                        == production->LayerIndices().first();
            }))
    {
        return fail(
            QStringLiteral(
                "preview-legend-probe-context 首层 TIFF 异步读取超时。"));
    }
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
    if (workspaceTitles
        != QStringList{
            QStringLiteral("模型"),
            QStringLiteral("预览"),
            QStringLiteral("配置")})
    {
        return fail(QStringLiteral("diagnostics-collapse 中央页签仍包含历史报告/曲线入口：")
                    + workspaceTitles.join(QStringLiteral(",")));
    }
    if (dock->TabTitles()
        != QStringList{
            QStringLiteral("报告"),
            QStringLiteral("材料闭环"),
            QStringLiteral("曲线"),
            QStringLiteral("材料参数"),
            QStringLiteral("工艺对比"),
            QStringLiteral("切片耗时"),
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
    auto* projectDock = window.findChild<ProjectToolsDock*>(
        QStringLiteral("projectToolsDock"));
    auto* projectPanel = window.findChild<QWidget*>(QStringLiteral("projectPanel"));
    auto* workspaceTabs = window.findChild<QTabWidget*>(QStringLiteral("mainWorkspaceTabs"));
    auto* rightPanel = window.findChild<ContextInspector*>(
        QStringLiteral("contextInspector"));
    auto* preview = window.findChild<PreviewWorkspace*>(QStringLiteral("previewWorkspace"));
    auto* configPanel = window.findChild<ConfigEditorPanel*>();
    auto* dock = window.findChild<DiagnosticsDock*>(QStringLiteral("diagnosticsDock"));
    auto* projectAction = window.findChild<QAction*>(
        QStringLiteral("projectToolsToggleAction"));
    auto* diagnosticsAction = window.findChild<QAction*>(QStringLiteral("diagnosticsToggleAction"));
    if (splitter == nullptr || projectDock == nullptr || projectPanel == nullptr
        || workspaceTabs == nullptr || rightPanel == nullptr
        || preview == nullptr || configPanel == nullptr || dock == nullptr
        || projectAction == nullptr || diagnosticsAction == nullptr)
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
                    "windowHint=%5x%6 projectDockHint=%7x%8 workspaceHint=%9x%10 rightHint=%11x%12 "
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
        if (projectDock->IsExpanded()
            || projectAction->isChecked()
            || !workspaceTabs->isVisible()
            || !rightPanel->isVisible())
        {
            return fail(QStringLiteral(
                "workspace-layout-sizes 默认工作区可见性不正确。"));
        }

        const QRect splitterRect = GlobalRect(splitter);
        const QRect workspaceRect = GlobalRect(workspaceTabs);
        const QRect rightRect = GlobalRect(rightPanel);
        if (workspaceRect.width() < 400
            || rightRect.width() < 240)
        {
            return fail(
                QStringLiteral(
                    "workspace-layout-sizes 主工作区宽度低于冻结边界：%1/%2")
                    .arg(workspaceRect.width())
                    .arg(rightRect.width()));
        }
        if (workspaceRect.intersects(rightRect))
        {
            return fail(QStringLiteral(
                "workspace-layout-sizes 主工作区发生重叠。"));
        }
        if (!splitterRect.contains(workspaceRect)
            || !splitterRect.contains(rightRect))
        {
            return fail(QStringLiteral(
                "workspace-layout-sizes 主工作区超出 mainSplitter。"));
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
            QStringLiteral("%1x%2=%3/%4")
                .arg(targetSize.width())
                .arg(targetSize.height())
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
    auto* configWorkspace =
        window.findChild<QWidget*>(QStringLiteral("configEditorScrollArea"));
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
    if (workspaceTabs == nullptr || configWorkspace == nullptr
        || modePanel == nullptr || modeCombo == nullptr
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
    workspaceTabs->setCurrentWidget(configWorkspace);
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
                    "production-mode-selector 中文文本在 %1x%2 被截断或模式面板不可见："
                    "window=%3x%4 panelVisible=%5 mode=%6/%7 profile=%8/%9。")
                    .arg(targetSize.width())
                    .arg(targetSize.height())
                    .arg(window.width())
                    .arg(window.height())
                    .arg(modePanel->isVisible())
                    .arg(modeCombo->width())
                    .arg(modeCombo->minimumSizeHint().width())
                    .arg(profileCombo->width())
                    .arg(profileCombo->minimumSizeHint().width()));
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
    const QJsonObject baseProjection =
        support.value("baseProjection").toObject();
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
        || generated.value("modelFill").toObject().value("scope").toString() != "all_model"
        || support.value("placement").toString() != "both"
        || support.value("internalVoid").toObject().value("minAreaPx").toInt() != 24
        || !baseProjection.value("enabled").toBool()
        || baseProjection.value("layerCount").toInt() != 30
        || baseProjection.value("layerPlacement").toString()
            != "prepend_below_model"
        || baseProjection.value("source").toString()
            != "max_support_footprint"
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
    if (!configpanel.loadConfig(templatepath))
    {
        return fail("generated-effective-config UI 无法加载压缩设置 fixture。");
    }
    auto* compressionCombo =
        configpanel.findChild<QComboBox*>("tiffCompressionCombo");
    if (compressionCombo == nullptr
        || compressionCombo->findData(QStringLiteral("none")) < 0
        || compressionCombo->findData(QStringLiteral("packbits")) < 0)
    {
        return fail("generated-effective-config UI 未提供 none/PackBits TIFF 压缩选项。");
    }
    compressionCombo->setCurrentIndex(
        compressionCombo->findData(QStringLiteral("packbits")));
    if (viewdocument
            .value({"output", "tiffCompression", "algorithm"})
            .toString()
        != QStringLiteral("packbits"))
    {
        return fail("generated-effective-config UI 未写回 output.tiffCompression.algorithm。");
    }
    EffectiveConfigRequest uiCompressionRequest = request;
    uiCompressionRequest.generatedconfigpath =
        tempdir.filePath("ui_compression/slice_config.effective.json");
    uiCompressionRequest.overridedocument = viewdocument.document();
    const EffectiveConfigResult uiCompressionResult =
        EffectiveConfigGenerator().Generate(uiCompressionRequest);
    if (!uiCompressionResult.IsValid()
        || uiCompressionResult.document.object()
                   .value("output")
                   .toObject()
                   .value("tiffCompression")
                   .toObject()
                   .value("algorithm")
                   .toString()
            != QStringLiteral("packbits"))
    {
        return fail("generated-effective-config 未把 UI PackBits 选择传入生效配置。");
    }
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
        || rgbOnlyRoot.value("modelFill").toObject().value("scope").toString()
            != "below_texture_surface"
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

    EffectiveConfigRequest compressionRequest = request;
    compressionRequest.generatedconfigpath =
        tempdir.filePath("bad_compression/slice_config.effective.json");
    QJsonObject badCompressionRoot =
        compressionRequest.overridedocument.object();
    QJsonObject badCompressionOutput =
        badCompressionRoot.value("output").toObject();
    badCompressionOutput.insert(
        "tiffCompression",
        QJsonObject{{"algorithm", "deflate"}});
    badCompressionRoot.insert("output", badCompressionOutput);
    compressionRequest.overridedocument =
        QJsonDocument(badCompressionRoot);
    const EffectiveConfigResult compressionResult =
        EffectiveConfigGenerator().Generate(compressionRequest);
    if (compressionResult.IsValid()
        || QFileInfo::exists(compressionRequest.generatedconfigpath))
    {
        return fail("generated-effective-config 未阻断不支持的 TIFF 压缩算法。");
    }

    return pass(QString("generated-effective-config differences=%1 template-readonly=true compression=packbits")
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
        "gridSetupMs=90.000 sliceProcessingMs=800.000 layerComputeMs=500.000 layerComposeMs=75.000 "
        "tiffWriteMs=200.000 previewWriteMs=100.000 "
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
        || qAbs(timing.gridsetupms - 90.0) > 0.001
        || qAbs(timing.sliceprocessingms - 800.0) > 0.001
        || qAbs(timing.layercomposems - 75.0) > 0.001
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
        || !summary.contains(QStringLiteral("340.0 ms"))
        || !summary.contains(QStringLiteral("准入=90.0 ms"))
        || !summary.contains(QStringLiteral("合成=75.0 ms")))
    {
        return fail(QStringLiteral("切片耗时面板未显示解析后的数据：") + summary);
    }

    SliceProgressEvent sceneProgress;
    sceneProgress.phase =
        QStringLiteral("scene_package_write");
    sceneProgress.current = 12;
    sceneProgress.total = 50;
    sceneProgress.percent = 84;
    sceneProgress.elapsedms = 2100.0;
    SliceTimingEvent sceneTiming = timing;
    sceneTiming.engine = QStringLiteral("legacy-scene");
    panel.Reset(QStringLiteral("切片当前场景"));
    panel.UpdateProgress(sceneProgress);
    panel.ShowTiming(sceneTiming);
    const QString sceneSummary = panel.SummaryText();
    if (!sceneSummary.contains(
            QStringLiteral("正在保存场景图层 12 / 50"))
        || !sceneSummary.contains(
            QStringLiteral("传统场景切片引擎")))
    {
        return fail(
            QStringLiteral("场景切片阶段与引擎中文显示错误：")
            + sceneSummary);
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
    auto* cancelImportButton = window.findChild<QPushButton*>(
        QStringLiteral("cancelModelImportButton"));
    auto* sliceCurrentSceneButton =
        window.findChild<QPushButton*>(
            QStringLiteral("sliceCurrentSceneButton"));
    auto* workspace = window.findChild<QWidget*>(
        QStringLiteral("modelTopViewWorkspace"));
    auto* canvas = window.findChild<ModelTopViewWidget*>(
        QStringLiteral("modelTopViewWidget"));
    if (importButton == nullptr
        || cancelImportButton == nullptr
        || sliceCurrentSceneButton == nullptr
        || workspace == nullptr
        || canvas == nullptr
        || importButton->text()
            != QStringLiteral("导入模型（可多选）")
        || sliceCurrentSceneButton->text()
            != QStringLiteral("切片当前场景")
        || sliceCurrentSceneButton->isEnabled())
    {
        return fail(QStringLiteral(
            "model top view batch import/current-scene action integration missing"));
    }

    return pass(QStringLiteral(
        "model-top-view async/+Z/grid/identity/selection/blocked/"
        "cancel/batch-action"));
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
    auto* axisPolicy = panel.findChild<QLabel*>(
        QStringLiteral("modelTransformAxisPolicy"));
    if (translateX == nullptr
        || translateY == nullptr
        || rotateZ == nullptr
        || uniformScale == nullptr
        || applyButton == nullptr
        || centerButton == nullptr
        || resetButton == nullptr
        || saveButton == nullptr
        || axisPolicy == nullptr
        || !axisPolicy->text().contains(
            QStringLiteral("Z 高度由自动定向")))
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
        || window.findChild<ContextInspector*>(
            QStringLiteral("contextInspector"))
            == nullptr)
    {
        return fail(QStringLiteral(
            "model transform context-inspector integration missing"));
    }

    return pass(QStringLiteral(
        "model-top-view-transform x/y/rotate/scale/center/reset/"
        "locked/dirty/latest-generation"));
}

int UiSmokeTestRunner::SceneBatchImportThree(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    SceneDocument& document = window.m_sceneDocument;
    SceneBatchImportController& controller =
        window.m_sceneBatchImportController;
    ModelTopViewWidget* topView = window.m_modelTopViewWidget;

    const QString configPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    if (topView == nullptr
        || !window.config_editor_panel_->loadConfig(configPath))
    {
        return fail(QStringLiteral(
            "scene batch three-item MainWindow fixture unavailable"));
    }

    bool secondItemWasPresented{false};
    QObject::connect(
        &controller,
        &SceneBatchImportController::SigStateChanged,
        &window,
        [&controller,
         &document,
         topView,
         &secondItemWasPresented]()
         {
             if (controller.IsRunning()
                 && controller.Summary().imported == 2)
             {
                 secondItemWasPresented =
                     document.InstanceCount() == 2U
                     && topView->PresentationItemCount() == 2U;
             }
         });

    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    SceneBatchImportRequest request;
    request.batchid = QStringLiteral("smoke-three");
    request.configpath = configPath;
    request.files = QStringList{
        modelPath,
        modelPath,
        modelPath,
    };
    request.autolayout = true;
    if (!controller.Start(request).IsValid()
        || !WaitForCondition(
            [&controller]()
            {
                return !controller.IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "scene batch three-item import did not complete"));
    }

    const SceneBatchImportSummary& summary =
        controller.Summary();
    if (summary.selected != 3
        || summary.imported != 3
        || summary.failed != 0
        || summary.cancelled != 0
        || !summary.autolayoutapplied
        || document.InstanceCount() != 3U
        || !secondItemWasPresented
        || topView->PresentationItemCount() != 3U
        || document.Items().at(0U).layoutcolumn != 0
        || document.Items().at(1U).layoutcolumn != 1
        || document.Items().at(2U).layoutcolumn != 2)
    {
        return fail(QStringLiteral(
            "scene batch three-item summary/layout mismatch"));
    }

    int configTabIndex{-1};
    for (int index = 0;
         index < window.m_mainWorkspaceTabs->count();
         ++index)
    {
        if (window.m_mainWorkspaceTabs->tabText(index)
            == QStringLiteral("配置"))
        {
            configTabIndex = index;
            break;
        }
    }
    if (configTabIndex < 0)
    {
        return fail(QStringLiteral(
            "scene batch configuration workspace missing"));
    }

    window.m_mainWorkspaceTabs->setCurrentIndex(configTabIndex);
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    if (document.InstanceCount() != 3U
        || !topView->HasRenderableGeometry())
    {
        return fail(QStringLiteral(
            "scene disappeared when configuration workspace opened"));
    }
    const int previewInterval =
        window.config_document_
            .value({QStringLiteral("preview"),
                    QStringLiteral("interval")})
            .toInt(1);
    window.config_document_.setValue(
        {QStringLiteral("preview"), QStringLiteral("interval")},
        previewInterval + 1);
    window.m_mainWorkspaceTabs->setCurrentWidget(
        window.m_modelTopViewWorkspace);
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    if (document.InstanceCount() != 3U
        || !document.Geometry().has_value()
        || !topView->HasRenderableGeometry())
    {
        return fail(QStringLiteral(
            "scene disappeared after configuration workspace round-trip"));
    }

    return pass(QStringLiteral(
        "scene-batch-import-three ordered/one-layout/"
        "incremental-presentation/config-round-trip"));
}

int UiSmokeTestRunner::SceneBatchImportRealMeigui(
    const UiSmokeTestOptions& options)
{
    SceneDocument document;
    SceneModelRepository repository;
    ModelTopViewLoader loader(&document, &repository);
    SceneBatchImportController controller(&document);
    controller.SetLoadHandlers(
        [&loader](const ModelTopViewLoadRequest& request)
        {
            loader.RequestLoad(request);
            return loader.Generation();
        },
        [&loader]()
        {
            loader.Cancel();
        });
    QObject::connect(
        &loader,
        &ModelTopViewLoader::SigLoadingFinished,
        &controller,
        [&controller, &loader]()
        {
            controller.OnLoadFinished(loader.Generation());
        });

    const QDir root(options.repo_root);
    const QString modelRoot =
        root.filePath(QStringLiteral("model/obj/meigui_fudiao"));
    SceneBatchImportRequest request;
    request.batchid = QStringLiteral("smoke-real-meigui");
    request.configpath = root.filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    request.files = QStringList{
        QDir(modelRoot).filePath(QStringLiteral("02.obj")),
        QDir(modelRoot).filePath(QStringLiteral("03.obj")),
        QDir(modelRoot).filePath(QStringLiteral("04.obj")),
        QDir(modelRoot).filePath(
            QStringLiteral("MF_Mei_gui_wumingzhi_fx04.obj")),
    };
    request.autolayout = true;
    for (const QString& modelPath : request.files)
    {
        if (!QFileInfo::exists(modelPath))
        {
            return fail(
                QStringLiteral(
                    "scene batch real meigui asset missing: ")
                + modelPath);
        }
    }
    if (!controller.Start(request).IsValid()
        || !WaitForCondition(
            [&controller]()
            {
                return !controller.IsRunning();
            },
            120000))
    {
        return fail(QStringLiteral(
            "scene batch real meigui import did not complete"));
    }

    const SceneBatchImportSummary& summary =
        controller.Summary();
    if (summary.selected != 4
        || summary.imported != 4
        || summary.failed != 0
        || summary.cancelled != 0
        || !summary.autolayoutapplied
        || document.InstanceCount() != 4U)
    {
        return fail(
            QStringLiteral(
                "scene batch real meigui summary mismatch: "
                "selected=%1 imported=%2 failed=%3 cancelled=%4 "
                "instances=%5 layout=%6")
                .arg(summary.selected)
                .arg(summary.imported)
                .arg(summary.failed)
                .arg(summary.cancelled)
                .arg(document.InstanceCount())
                .arg(summary.autolayoutapplied));
    }

    const auto TipFacesPositiveY =
        [](const slicer_core::SceneModel& model)
    {
        constexpr double kEndBandFraction{0.12};
        constexpr double kMinimumDifferenceMm{0.05};
        const double width =
            model.bbox_mm.max.x - model.bbox_mm.min.x;
        const double depth =
            model.bbox_mm.max.y - model.bbox_mm.min.y;
        if (depth <= width)
        {
            return false;
        }
        const double lowBoundary =
            model.bbox_mm.min.y
            + depth * kEndBandFraction;
        const double highBoundary =
            model.bbox_mm.max.y
            - depth * kEndBandFraction;
        double lowMinimumX{
            std::numeric_limits<double>::max()};
        double lowMaximumX{
            std::numeric_limits<double>::lowest()};
        double highMinimumX{
            std::numeric_limits<double>::max()};
        double highMaximumX{
            std::numeric_limits<double>::lowest()};
        for (const slicer_core::Triangle& triangle : model.triangles)
        {
            for (const slicer_core::Vec3* point :
                 {&triangle.a, &triangle.b, &triangle.c})
            {
                if (point->y <= lowBoundary)
                {
                    lowMinimumX = std::min(
                        lowMinimumX,
                        point->x);
                    lowMaximumX = std::max(
                        lowMaximumX,
                        point->x);
                }
                if (point->y >= highBoundary)
                {
                    highMinimumX = std::min(
                        highMinimumX,
                        point->x);
                    highMaximumX = std::max(
                        highMaximumX,
                        point->x);
                }
            }
        }
        const double lowSpan = lowMaximumX - lowMinimumX;
        const double highSpan = highMaximumX - highMinimumX;
        return std::isfinite(lowSpan)
            && std::isfinite(highSpan)
            && highSpan + kMinimumDifferenceMm < lowSpan;
    };
    for (const SceneDocumentItem& item : document.Items())
    {
        const auto source = repository.Find(item.sourcecachekey);
        if (!source.has_value()
            || source->model == nullptr
            || !TipFacesPositiveY(*source->model))
        {
            return fail(
                QStringLiteral(
                    "scene batch real meigui tip is not facing +Y: ")
                + item.modelpath);
        }
    }
    return pass(QStringLiteral(
        "scene-batch-import-real-meigui 02/03/04/MF +Y-tip"));
}

int UiSmokeTestRunner::SceneBatchImportPartialFailure(
    const UiSmokeTestOptions& options)
{
    SceneDocument document;
    SceneModelRepository repository;
    ModelTopViewLoader loader(&document, &repository);
    SceneBatchImportController controller(&document);
    controller.SetLoadHandlers(
        [&loader](const ModelTopViewLoadRequest& request)
        {
            loader.RequestLoad(request);
            return loader.Generation();
        },
        [&loader]()
        {
            loader.Cancel();
        });
    QObject::connect(
        &loader,
        &ModelTopViewLoader::SigLoadingFinished,
        &controller,
        [&controller, &loader]()
        {
            controller.OnLoadFinished(loader.Generation());
        });

    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    SceneBatchImportRequest request;
    request.batchid = QStringLiteral("smoke-partial");
    request.configpath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    request.files = QStringList{
        modelPath,
        QDir(options.repo_root).filePath(
            QStringLiteral(
                "samples/models/missing-model.obj")),
        modelPath,
    };
    request.autolayout = true;
    if (!controller.Start(request).IsValid()
        || !WaitForCondition(
            [&controller]()
            {
                return !controller.IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "scene batch partial-failure import did not complete"));
    }

    const SceneBatchImportSummary& summary =
        controller.Summary();
    if (summary.imported != 2
        || summary.failed != 1
        || summary.items.size() != 3U
        || summary.items.at(1U).errorcode
            != QStringLiteral(
                "SCENE_BATCH_IMPORT_ITEM_FAILED")
        || document.InstanceCount() != 2U
        || !summary.autolayoutapplied)
    {
        return fail(QStringLiteral(
            "scene batch partial-failure contract mismatch"));
    }
    return pass(QStringLiteral(
        "scene-batch-import-partial-failure continue/summary"));
}

int UiSmokeTestRunner::SceneSliceCurrent(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    const QString configPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    window.m_currentProfileId.clear();
    window.config_edit_->setText(configPath);
    if (!window.config_editor_panel_->loadConfig(configPath))
    {
        return fail(QStringLiteral(
            "scene slice Profile fixture did not load"));
    }

    SceneBatchImportRequest importRequest;
    importRequest.batchid =
        QStringLiteral("scene-slice-current-smoke");
    importRequest.configpath = configPath;
    importRequest.files =
        QStringList{modelPath, modelPath, modelPath};
    importRequest.autolayout = true;
    if (!window.m_sceneBatchImportController
             .Start(importRequest)
             .IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.m_sceneBatchImportController
                            .IsRunning();
            })
        || window.m_sceneDocument.InstanceCount() != 3U)
    {
        return fail(QStringLiteral(
            "scene slice smoke batch import failed"));
    }

    window.UpdateActionAvailability();
    auto* sliceButton = window.findChild<QPushButton*>(
        QStringLiteral("sliceCurrentSceneButton"));
    auto* cancelButton = window.findChild<QPushButton*>(
        QStringLiteral("cancelCurrentSceneSliceButton"));
    if (sliceButton == nullptr
        || cancelButton == nullptr
        || !sliceButton->isEnabled()
        || cancelButton->isEnabled())
    {
        return fail(QStringLiteral(
            "scene slice action availability mismatch"));
    }

    sliceButton->click();
    if (!WaitForCondition(
            [&window]()
            {
                return !window.m_sceneSliceActionController
                            .IsRunning()
                    && !window.runner_.IsRunning();
            },
            30000)
        || window.m_sceneSliceActionController.State()
            != SceneSliceActionState::Completed
        || !QFileInfo::exists(
            QDir(window.package_edit_->text())
                .filePath(QStringLiteral("manifest.json")))
        || window.m_previewWorkspace->LayerIndices().isEmpty()
        || window.m_mainWorkspaceTabs->currentWidget()
            != window.m_previewWorkspace
        || !window.m_lastSliceTimingEvent.has_value()
        || window.m_lastSliceTimingEvent->engine
            != QStringLiteral("legacy-scene")
        || !window.m_sliceTimingPanel->SummaryText().contains(
            QStringLiteral("传统场景切片引擎"))
        || !window.m_sliceTimingPanel->SummaryText().contains(
            QStringLiteral("准入=")))
    {
        return fail(
            QStringLiteral(
                "scene slice did not publish/load one TIFF package: ")
            + window.m_sceneSliceActionController.Message());
    }
    return pass(QStringLiteral(
        "scene-slice-current three-model/one-package/"
        "tiff-load/progress-timing"));
}

int UiSmokeTestRunner::SceneSliceSingleMaterialProfile(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    const ScenarioEntry* scenario =
        window.m_scenarioRegistry.FindById(
            QStringLiteral("single_material_relief"));
    if (scenario == nullptr)
    {
        return fail(
            QStringLiteral(
                "single-material scene Profile is missing"));
    }
    window.ApplyScenario(*scenario);
    window.config_document_.setValue(
        {QStringLiteral("output"), QStringLiteral("dpiX")},
        127);
    window.config_document_.setValue(
        {QStringLiteral("output"), QStringLiteral("dpiY")},
        127);
    window.config_document_.setValue(
        {QStringLiteral("output"),
         QStringLiteral("layerThicknessMm")},
        0.50);

    const QString configPath =
        QDir(options.repo_root).filePath(
            scenario->configpath);
    const QString modelPath = QDir(
        QFileInfo(configPath).absolutePath())
                                  .filePath(
                                      QStringLiteral(
                                          "../../models/relief/"
                                          "relief_nail_arched.obj"));
    SceneBatchImportRequest importRequest;
    importRequest.batchid =
        QStringLiteral(
            "scene-slice-single-material-profile-smoke");
    importRequest.configpath = configPath;
    importRequest.files = QStringList{modelPath};
    importRequest.autolayout = true;
    if (!window.m_sceneBatchImportController
             .Start(importRequest)
             .IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.m_sceneBatchImportController
                            .IsRunning();
            })
        || window.m_sceneDocument.InstanceCount() != 1U)
    {
        return fail(
            QStringLiteral(
                "single-material scene import failed"));
    }

    SceneSliceActionRequest request;
    request.mode =
        slicer_core::SlicePipelineMode::Legacy;
    const SceneSliceSnapshotResult snapshot =
        window.WriteCurrentSceneSnapshot(request);
    if (!snapshot.IsValid()
        || snapshot.snapshot->profileid
            != QStringLiteral("single_material_relief"))
    {
        return fail(
            QStringLiteral(
                "single-material scene snapshot rejected: ")
            + snapshot.message);
    }

    return pass(
        QStringLiteral(
            "scene-slice-single-material-profile "
            "identity/snapshot"));
}

int UiSmokeTestRunner::SceneSliceRealAssets(
    const UiSmokeTestOptions& options)
{
    const QString evidencePath = absoluteFromRepo(
        options,
        options.output_path.isEmpty()
            ? QStringLiteral(
                  "output/benchmarks/13b_08/"
                  "qt_real_assets_workflow.json")
            : options.output_path);
    const QString evidenceDirectory =
        QFileInfo(evidencePath).absolutePath();
    if (!QDir().mkpath(evidenceDirectory))
    {
        return fail(QStringLiteral(
            "scene real-assets evidence directory creation failed"));
    }

    const QString templatePath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    QJsonObject profileRoot;
    if (!ReadJsonObject(templatePath, &profileRoot))
    {
        return fail(QStringLiteral(
            "scene real-assets Profile fixture did not load"));
    }
    QJsonObject output =
        profileRoot.value(QStringLiteral("output")).toObject();
    output[QStringLiteral("dpiX")] = 127;
    output[QStringLiteral("dpiY")] = 127;
    output[QStringLiteral("layerThicknessMm")] = 0.20;
    output[QStringLiteral("packageDir")] =
        QDir::fromNativeSeparators(
            QDir(evidenceDirectory).filePath(
                QStringLiteral("qt_real_assets_package")));
    profileRoot[QStringLiteral("output")] = output;
    QJsonObject preview =
        profileRoot.value(QStringLiteral("preview")).toObject();
    preview[QStringLiteral("enabled")] = false;
    profileRoot[QStringLiteral("preview")] = preview;

    const QString profilePath =
        QDir(evidenceDirectory).filePath(
            QStringLiteral("qt_real_assets_profile.json"));
    if (!WriteJsonFixture(profilePath, profileRoot))
    {
        return fail(QStringLiteral(
            "scene real-assets Profile fixture write failed"));
    }

    const QStringList modelPaths{
        QDir(options.repo_root).filePath(
            QStringLiteral(
                "model/obj/xiao_ma_wu_yu_new/"
                "MF_Xiao_ma_Damuzhi_ty02.obj")),
        QDir(options.repo_root).filePath(
            QStringLiteral("model/obj/yecan/3.obj")),
        QDir(options.repo_root).filePath(
            QStringLiteral(
                "samples/models/3mf/"
                "texture2d_checker_cube.3mf")),
    };
    for (const QString& modelPath : modelPaths)
    {
        if (!QFileInfo::exists(modelPath))
        {
            return fail(
                QStringLiteral(
                    "scene real-assets model is missing: ")
                + modelPath);
        }
    }

    MainWindow window(options.repo_root);
    window.m_currentProfileId.clear();
    window.config_edit_->setText(profilePath);
    if (!window.config_editor_panel_->loadConfig(profilePath))
    {
        return fail(QStringLiteral(
            "scene real-assets effective Profile did not load"));
    }

    SceneBatchImportRequest importRequest;
    importRequest.batchid =
        QStringLiteral("scene-slice-real-assets-smoke");
    importRequest.configpath = profilePath;
    importRequest.files = modelPaths;
    importRequest.autolayout = true;
    if (!window.m_sceneBatchImportController
             .Start(importRequest)
             .IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.m_sceneBatchImportController
                            .IsRunning();
            },
            120000)
        || window.m_sceneDocument.InstanceCount() != 3U)
    {
        return fail(QStringLiteral(
            "scene real-assets batch import failed"));
    }

    window.UpdateActionAvailability();
    auto* sliceButton = window.findChild<QPushButton*>(
        QStringLiteral("sliceCurrentSceneButton"));
    if (sliceButton == nullptr || !sliceButton->isEnabled())
    {
        return fail(QStringLiteral(
            "scene real-assets slice action unavailable"));
    }
    sliceButton->click();
    if (!WaitForCondition(
            [&window]()
            {
                return !window.m_sceneSliceActionController
                            .IsRunning()
                    && !window.runner_.IsRunning();
            },
            180000)
        || window.m_sceneSliceActionController.State()
            != SceneSliceActionState::Completed
        || window.m_previewWorkspace->LayerIndices().isEmpty())
    {
        return fail(
            QStringLiteral(
                "scene real-assets workflow did not complete: ")
            + window.m_sceneSliceActionController.Message());
    }

    const QString packageDir = window.package_edit_->text();
    const QString manifestPath =
        QDir(packageDir).filePath(QStringLiteral("manifest.json"));
    const QString sceneReportPath =
        QDir(packageDir).filePath(
            QStringLiteral(
                "reports/multimodel_scene_report.json"));
    QJsonObject manifest;
    QJsonObject sceneReport;
    if (!ReadJsonObject(manifestPath, &manifest)
        || !ReadJsonObject(sceneReportPath, &sceneReport)
        || sceneReport.value(QStringLiteral("instanceCount"))
               .toInt()
            != 3
        || sceneReport.value(QStringLiteral("package"))
               .toObject()
               .value(QStringLiteral("path"))
               .toString()
            != QDir::fromNativeSeparators(packageDir))
    {
        return fail(QStringLiteral(
            "scene real-assets package identity mismatch"));
    }

    QJsonArray assets;
    for (const QString& modelPath : modelPaths)
    {
        assets.append(
            QJsonObject{
                {QStringLiteral("path"),
                 QDir::fromNativeSeparators(modelPath)},
                {QStringLiteral("format"),
                 QFileInfo(modelPath).suffix().toLower()}});
    }
    const QJsonObject tiff =
        manifest.value(QStringLiteral("tiff")).toObject();
    const QJsonObject grid =
        manifest.value(QStringLiteral("grid")).toObject();
    const QJsonObject protocol{
        {QStringLiteral("schema"),
         manifest.value(QStringLiteral("schema"))},
        {QStringLiteral("channelOrder"),
         tiff.value(QStringLiteral("channelOrder"))},
        {QStringLiteral("bitDepth"),
         tiff.value(QStringLiteral("bitDepth"))},
        {QStringLiteral("polarity"),
         tiff.value(QStringLiteral("polarity"))},
        {QStringLiteral("printValue"),
         tiff.value(QStringLiteral("printValue"))},
        {QStringLiteral("emptyValue"),
         tiff.value(QStringLiteral("emptyValue"))},
    };
    const QJsonObject evidence{
        {QStringLiteral("schema"),
         QStringLiteral(
             "slicesoft.scene_workflow_ui_smoke.13b08.1")},
        {QStringLiteral("caseId"),
         QStringLiteral("13B-08-UI-REAL-3")},
        {QStringLiteral("status"), QStringLiteral("passed")},
        {QStringLiteral("route"),
         QStringLiteral("slicer_cli --scene-config")},
        {QStringLiteral("profilePath"),
         QDir::fromNativeSeparators(profilePath)},
        {QStringLiteral("packageDir"),
         QDir::fromNativeSeparators(packageDir)},
        {QStringLiteral("manifestPath"),
         QDir::fromNativeSeparators(manifestPath)},
        {QStringLiteral("sceneReportPath"),
         QDir::fromNativeSeparators(sceneReportPath)},
        {QStringLiteral("sceneId"),
         sceneReport.value(QStringLiteral("sceneId"))},
        {QStringLiteral("sceneRevision"),
         sceneReport.value(QStringLiteral("sceneRevision"))},
        {QStringLiteral("sceneHash"),
         sceneReport.value(QStringLiteral("sceneHash"))},
        {QStringLiteral("instanceCount"), 3},
        {QStringLiteral("loadedLayerCount"),
         static_cast<int>(
             window.m_previewWorkspace->LayerIndices().size())},
        {QStringLiteral("assets"), assets},
        {QStringLiteral("protocol"), protocol},
        {QStringLiteral("grid"), grid},
        {QStringLiteral("singlePackage"), true},
        {QStringLiteral("productionGo"), false},
        {QStringLiteral("productionStatus"),
         QStringLiteral("INPUT_OPEN")},
    };
    if (!WriteJsonFixture(evidencePath, evidence))
    {
        return fail(QStringLiteral(
            "scene real-assets evidence write failed"));
    }

    return pass(QStringLiteral(
        "scene-slice-real-assets OBJ/texture/3MF/"
        "one-package/tiff-load"));
}

int UiSmokeTestRunner::SceneSliceStale(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    const QString configPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    window.m_currentProfileId.clear();
    window.config_edit_->setText(configPath);
    if (!window.config_editor_panel_->loadConfig(configPath))
    {
        return fail(QStringLiteral(
            "stale scene slice Profile fixture did not load"));
    }

    SceneBatchImportRequest importRequest;
    importRequest.batchid =
        QStringLiteral("scene-slice-stale-smoke");
    importRequest.configpath = configPath;
    importRequest.files = QStringList{modelPath};
    importRequest.autolayout = true;
    if (!window.m_sceneBatchImportController
             .Start(importRequest)
             .IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.m_sceneBatchImportController
                            .IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "stale scene slice import failed"));
    }

    window.UpdateActionAvailability();
    auto* sliceButton = window.findChild<QPushButton*>(
        QStringLiteral("sliceCurrentSceneButton"));
    if (sliceButton == nullptr || !sliceButton->isEnabled())
    {
        return fail(QStringLiteral(
            "stale scene slice action unavailable"));
    }
    sliceButton->click();
    if (!WaitForCondition(
            [&window]()
            {
                return window.m_sceneSliceActionController.State()
                    == SceneSliceActionState::Slicing;
            }))
    {
        return fail(QStringLiteral(
            "stale scene slice process did not start"));
    }

    const QString sourceInstance =
        window.m_sceneDocument.CurrentInstanceId();
    const SceneDocumentOperationResult changed =
        window.m_sceneDocument.DuplicateInstance(
            sourceInstance,
            QStringLiteral("stale-extra-instance"),
            window.m_sceneDocument.SceneRevision());
    if (!changed.IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.runner_.IsRunning();
            },
            30000)
        || window.m_sceneSliceActionController.State()
            != SceneSliceActionState::Blocked
        || window.m_sceneSliceActionController.ErrorCode()
            != SceneSliceActionErrorCode::SceneStale)
    {
        return fail(QStringLiteral(
            "stale scene output was not rejected"));
    }
    return pass(QStringLiteral(
        "scene-slice-stale revision/no-old-package-load"));
}

int UiSmokeTestRunner::SceneSliceCancel(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    const QString configPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    window.m_currentProfileId.clear();
    window.config_edit_->setText(configPath);
    if (!window.config_editor_panel_->loadConfig(configPath))
    {
        return fail(QStringLiteral(
            "cancel scene slice Profile fixture did not load"));
    }

    SceneBatchImportRequest importRequest;
    importRequest.batchid =
        QStringLiteral("scene-slice-cancel-smoke");
    importRequest.configpath = configPath;
    importRequest.files = QStringList{modelPath};
    importRequest.autolayout = true;
    if (!window.m_sceneBatchImportController
             .Start(importRequest)
             .IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.m_sceneBatchImportController
                            .IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "cancel scene slice import failed"));
    }

    window.UpdateActionAvailability();
    auto* sliceButton = window.findChild<QPushButton*>(
        QStringLiteral("sliceCurrentSceneButton"));
    auto* cancelButton = window.findChild<QPushButton*>(
        QStringLiteral("cancelCurrentSceneSliceButton"));
    const QString packageMarker =
        QStringLiteral("scene-slice-cancel-marker");
    window.package_edit_->setText(packageMarker);
    if (sliceButton == nullptr
        || cancelButton == nullptr
        || !sliceButton->isEnabled())
    {
        return fail(QStringLiteral(
            "cancel scene slice action unavailable"));
    }
    sliceButton->click();
    if (!WaitForCondition(
            [&window, cancelButton]()
            {
                return window.m_sceneSliceActionController.State()
                        == SceneSliceActionState::Slicing
                    && cancelButton->isEnabled();
            }))
    {
        return fail(QStringLiteral(
            "cancel scene slice did not enter slicing"));
    }
    cancelButton->click();
    if (!WaitForCondition(
            [&window]()
            {
                return !window.runner_.IsRunning();
            },
            30000)
        || window.m_sceneSliceActionController.State()
            != SceneSliceActionState::Cancelled
        || window.m_sceneSliceActionController.ErrorCode()
            != SceneSliceActionErrorCode::Cancelled
        || window.package_edit_->text() != packageMarker)
    {
        return fail(QStringLiteral(
            "cancelled scene slice loaded or accepted output"));
    }
    return pass(QStringLiteral(
        "scene-slice-cancel terminate/no-package-load"));
}

int UiSmokeTestRunner::SceneSliceNoFallback(
    const UiSmokeTestOptions& options)
{
    MainWindow window(options.repo_root);
    const QString configPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/configs/golden/"
            "material_process_top2_fixture.json"));
    const QString modelPath = QDir(options.repo_root).filePath(
        QStringLiteral(
            "samples/models/openvdb/surface_shell_cube.obj"));
    window.m_currentProfileId.clear();
    window.config_edit_->setText(configPath);
    if (!window.config_editor_panel_->loadConfig(configPath))
    {
        return fail(QStringLiteral(
            "no-fallback scene slice Profile fixture did not load"));
    }

    SceneBatchImportRequest importRequest;
    importRequest.batchid =
        QStringLiteral("scene-slice-no-fallback-smoke");
    importRequest.configpath = configPath;
    importRequest.files = QStringList{modelPath};
    importRequest.autolayout = true;
    if (!window.m_sceneBatchImportController
             .Start(importRequest)
             .IsValid()
        || !WaitForCondition(
            [&window]()
            {
                return !window.m_sceneBatchImportController
                            .IsRunning();
            }))
    {
        return fail(QStringLiteral(
            "no-fallback scene slice import failed"));
    }

    auto* modeCombo = window.findChild<QComboBox*>(
        QStringLiteral("productionModeCombo"));
    if (modeCombo == nullptr)
    {
        return fail(QStringLiteral(
            "no-fallback production mode selector missing"));
    }
    const int globalIndex = modeCombo->findData(
        QStringLiteral("global_surface_shell"));
    if (globalIndex < 0)
    {
        return fail(QStringLiteral(
            "no-fallback Global mode option missing"));
    }
    modeCombo->setCurrentIndex(globalIndex);
    window.UpdateActionAvailability();

    auto* sliceButton = window.findChild<QPushButton*>(
        QStringLiteral("sliceCurrentSceneButton"));
    if (sliceButton == nullptr || sliceButton->isEnabled())
    {
        return fail(QStringLiteral(
            "Global scene slice was not disabled"));
    }
    window.OnSliceCurrentScene();
    if (window.runner_.IsRunning()
        || window.m_sceneSliceActionController.State()
            != SceneSliceActionState::Blocked
        || window.m_sceneSliceActionController.ErrorCode()
            != SceneSliceActionErrorCode::
                PipelineModeNotAdmitted)
    {
        return fail(QStringLiteral(
            "Global scene slice launched or fell back to Legacy"));
    }
    return pass(QStringLiteral(
        "scene-slice-no-fallback Global/Legacy-isolation"));
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
    firstGeometry.surfacepreview.width = 8;
    firstGeometry.surfacepreview.height = 4;
    firstGeometry.surfacepreview.rgba.assign(
        8U * 4U * 4U,
        255U);
    for (std::size_t index = 0U;
         index < firstGeometry.surfacepreview.rgba.size();
         index += 4U)
    {
        firstGeometry.surfacepreview.rgba.at(index + 0U) = 24U;
        firstGeometry.surfacepreview.rgba.at(index + 1U) = 86U;
        firstGeometry.surfacepreview.rgba.at(index + 2U) = 214U;
    }
    firstGeometry.surfacepreview.texturedpixelcount = 8U * 4U;
    firstGeometry.surfacepreview.contenthash =
        "multi-model-list-blue-surface";
    firstGeometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;
    if (!document.SetGeometry(1U, firstGeometry))
    {
        return fail(QStringLiteral("multi-model-list first geometry failed"));
    }
    selection.SetSelectedInstance(QStringLiteral("multi-first"));
    workspace.resize(700, 420);
    canvas.setGeometry(0, 0, 700, 420);
    workspace.show();
    QApplication::processEvents(QEventLoop::AllEvents, 50);
    const QImage appearanceImage = canvas.grab().toImage();
    bool foundAppearanceColor{false};
    QPoint appearancePoint;
    for (int y = 0;
         y < appearanceImage.height() && !foundAppearanceColor;
         ++y)
    {
        for (int x = 0; x < appearanceImage.width(); ++x)
        {
            const QColor pixel = appearanceImage.pixelColor(x, y);
            if (pixel.red() == 24
                && pixel.green() == 86
                && pixel.blue() == 214)
            {
                foundAppearanceColor = true;
                appearancePoint = QPoint(x, y);
                break;
            }
        }
    }
    if (!foundAppearanceColor)
    {
        return fail(QStringLiteral(
            "multi-model-list top view ignores material appearance"));
    }
    selection.Clear();
    QMouseEvent selectSurface(
        QEvent::MouseButtonPress,
        QPointF(appearancePoint),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(&canvas, &selectSurface);
    if (selection.SelectedInstance()
        != QStringLiteral("multi-first"))
    {
        return fail(QStringLiteral(
            "multi-model-list surface preview hit-test failed"));
    }

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
            "samples/models/textured/fixtures/"
            "policy_textured_small.obj"));
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
    QSet<QRgb> importedSurfaceColors;
    if (!WaitForCondition(
            [&importedDocument]()
            {
                return importedDocument.State()
                    != SceneDocumentState::Loading;
            })
        || importedDocument.State() != SceneDocumentState::Ready
        || importedDocument.InstanceCount() != 2U
        || repository.Size() != 1U
        || importedDocument.Items().at(0U).layoutcolumn != 0
        || importedDocument.Items().at(1U).layoutcolumn != 1
        || !importedDocument.Items().at(0U)
                .geometry->surfacepreview.IsValid()
        || !importedDocument.Items().at(1U)
                .geometry->surfacepreview.IsValid()
        || importedDocument.Items().at(0U)
                .geometry->surfacepreview.texturedpixelcount
            == 0U
        || importedDocument.Items().at(1U)
                .geometry->surfacepreview.texturedpixelcount
            == 0U
        || importedDocument.Items().at(0U)
                .geometry->worldboundsmm.max.xmm
            >= importedDocument.Items().at(1U)
                .geometry->worldboundsmm.min.xmm)
    {
        return fail(
            QStringLiteral(
                "multi-model-list async append/source sharing/"
                "auto-layout failed"));
    }
    const auto& importedSurface =
        importedDocument.Items().at(0U).geometry->surfacepreview;
    for (std::size_t index = 0U;
         index + 3U < importedSurface.rgba.size();
         index += 4U)
    {
        if (importedSurface.rgba.at(index + 3U) == 0U)
        {
            continue;
        }
        importedSurfaceColors.insert(
            qRgb(
                importedSurface.rgba.at(index + 0U),
                importedSurface.rgba.at(index + 1U),
                importedSurface.rgba.at(index + 2U)));
        if (importedSurfaceColors.size() > 1)
        {
            break;
        }
    }
    if (importedSurfaceColors.size() <= 1)
    {
        return fail(
            QStringLiteral(
                "multi-model-list real UV texture sampling failed"));
    }

    MainWindow window(options.repo_root);
    auto* integratedPanel = window.findChild<ModelListPanel*>(
        QStringLiteral("modelListPanel"));
    auto* integratedCanvas = window.findChild<ModelTopViewWidget*>(
        QStringLiteral("modelTopViewWidget"));
    auto* transformPanel = window.findChild<ModelTransformPanel*>(
        QStringLiteral("modelTransformPanel"));
    auto* inspector = window.findChild<ContextInspector*>(
        QStringLiteral("contextInspector"));
    if (integratedPanel == nullptr
        || integratedCanvas == nullptr
        || transformPanel == nullptr
        || inspector == nullptr)
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
            GlobalRect(inspector).adjusted(1, 1, -1, -1);
        if (canvasRect.intersects(sideRect)
            || inspector->width() < inspector->minimumWidth()
            || integratedCanvas->width() < integratedCanvas->minimumWidth()
            || !inspector->PageTitles().contains(
                QStringLiteral("场景")))
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
        "selection/material-appearance/textured-import/auto-layout/"
        "three-window-sizes"));
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
