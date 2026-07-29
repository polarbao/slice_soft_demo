#pragma once

#include <QString>

struct UiSmokeTestOptions {
    QString repo_root;
    QString case_name;
    QString config_path;
    QString package_path;
    QString package_a_path;
    QString package_b_path;
    QString output_path;
    bool yes{false};
};

class UiSmokeTestRunner {
public:
    int run(const UiSmokeTestOptions& options);

private:
    QString absoluteFromRepo(const UiSmokeTestOptions& options, const QString& path) const;
    int startup(const UiSmokeTestOptions& options);
    int loadPackage(const UiSmokeTestOptions& options);
    int saveAsConfig(const UiSmokeTestOptions& options);
    int chartLoad(const UiSmokeTestOptions& options);
    int overlayLoad(const UiSmokeTestOptions& options);
    int overlayLoadReal(const UiSmokeTestOptions& options);
    int layerPreviewLoad(const UiSmokeTestOptions& options);
    int compareProfiles(const UiSmokeTestOptions& options);
    int scenarioRegistry(const UiSmokeTestOptions& options);
    int sliceSettingsModel(const UiSmokeTestOptions& options);
    int SettingHelpMetadataCase(const UiSmokeTestOptions& options);
    int PreviewWorkspaceSharedLayer(const UiSmokeTestOptions& options);
    int TiffNativePreviewAllMaterials(const UiSmokeTestOptions& options);
    int TiffNativePreviewNoPng(const UiSmokeTestOptions& options);
    int PreviewLegendProbeContext(const UiSmokeTestOptions& options);
    int PreviewPhysicalAspect(const UiSmokeTestOptions& options);
    int DiagnosticsCollapse(const UiSmokeTestOptions& options);
    int MaterialClosureDiagnostics(const UiSmokeTestOptions& options);
    int OpenVdbUtilitySummary(const UiSmokeTestOptions& options);
    int WorkspaceLayoutSizes(const UiSmokeTestOptions& options);
    int WorkbenchJobActionBar(
        const UiSmokeTestOptions& options);
    int WorkbenchContextInspector(
        const UiSmokeTestOptions& options);
    int WorkbenchProjectDiagnostics(
        const UiSmokeTestOptions& options);
    int ProductionModeSelector(const UiSmokeTestOptions& options);
    int GeneratedEffectiveConfig(const UiSmokeTestOptions& options);
    int SliceProgressTiming(const UiSmokeTestOptions& options);
    int ModelPreflightStates(const UiSmokeTestOptions& options);
    int ModelPreflightOneClickGate(const UiSmokeTestOptions& options);
    int ModelPreflightLifecycle(const UiSmokeTestOptions& options);
    int ModelTopView(const UiSmokeTestOptions& options);
    int ModelTopViewTransform(const UiSmokeTestOptions& options);
    int ModelTransformPreflight(const UiSmokeTestOptions& options);
    int MultiModelList(const UiSmokeTestOptions& options);
    int SceneGridLayout(const UiSmokeTestOptions& options);
    int SceneBatchImportThree(
        const UiSmokeTestOptions& options);
    int SceneBatchImportRealMeigui(
        const UiSmokeTestOptions& options);
    int SceneBatchImportPartialFailure(
        const UiSmokeTestOptions& options);
    int SceneSliceCurrent(
        const UiSmokeTestOptions& options);
    int SceneSliceRealAssets(
        const UiSmokeTestOptions& options);
    int SceneSliceStale(
        const UiSmokeTestOptions& options);
    int SceneSliceCancel(
        const UiSmokeTestOptions& options);
    int SceneSliceNoFallback(
        const UiSmokeTestOptions& options);
    int experimentalReportSummary(const UiSmokeTestOptions& options);
    int fail(const QString& message) const;
    int pass(const QString& message) const;
};
