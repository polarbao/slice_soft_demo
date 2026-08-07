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
