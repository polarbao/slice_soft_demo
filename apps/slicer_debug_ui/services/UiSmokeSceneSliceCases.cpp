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
