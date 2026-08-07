#include "UiSmokeTestInternal.h"

namespace ui_smoke_test_support
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
    const int timeoutMs)
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

}  // namespace ui_smoke_test_support

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
