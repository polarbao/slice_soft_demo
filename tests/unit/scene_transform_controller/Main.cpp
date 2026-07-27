#include "SceneDocument.h"
#include "SceneModelRepository.h"
#include "SceneSelectionModel.h"
#include "SceneTransformController.h"

#include "slicer_core/scene/SceneEffectiveConfig.h"

#include <QCoreApplication>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace
{

constexpr double kTolerance{1.0e-9};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool NearlyEqual(const double left, const double right)
{
    return std::abs(left - right) <= kTolerance;
}

std::shared_ptr<const slicer_core::SceneModel> MakeSource()
{
    auto source = std::make_shared<slicer_core::SceneModel>();
    source->model_path =
        std::filesystem::current_path()
        / "scene-transform-fixture.obj";
    source->format = "obj";
    source->bbox_mm.min = {0.0, 0.0, 0.0};
    source->bbox_mm.max = {2.0, 4.0, 1.0};
    source->triangles = {
        {
            {0.0, 0.0, 0.0},
            {2.0, 0.0, 0.0},
            {1.0, 4.0, 1.0},
        },
    };
    source->triangle_count = source->triangles.size();
    return source;
}

slicer_core::ModelInstance MakeInstance()
{
    slicer_core::ModelInstance instance;
    instance.instanceid = "instance-1";
    instance.modelid = "model-1";
    instance.sourcetransformidentity = "source-transform-1";
    instance.sourcebboxmm = MakeSource()->bbox_mm;
    instance.effectivebboxmm = instance.sourcebboxmm;
    return instance;
}

slicer_core::SceneViewGeometry MakeGeometry(
    const std::uint64_t sceneRevision = 1U,
    const std::uint64_t transformRevision = 0U)
{
    slicer_core::SceneViewGeometry geometry;
    geometry.sceneid = "scene-1";
    geometry.modelid = "model-1";
    geometry.instanceid = "instance-1";
    geometry.scenerevision = sceneRevision;
    geometry.transformrevision = transformRevision;
    geometry.worldboundsmm.min = {0.0, 0.0};
    geometry.worldboundsmm.max = {2.0, 4.0};
    geometry.sourcebboxmm = MakeSource()->bbox_mm;
    geometry.effectivebboxmm = geometry.sourcebboxmm;
    geometry.triangles.push_back({
        {0.0, 0.0},
        {2.0, 0.0},
        {1.0, 4.0},
    });
    geometry.geometryhash = "geometry-hash";
    geometry.transformhash = "transform-hash";
    return geometry;
}

SceneModelRepositoryEntry MakeEntry()
{
    SceneModelRepositoryEntry entry;
    entry.cachekey = QStringLiteral("cache-1");
    entry.modelpath = QString::fromStdWString(
        MakeSource()->model_path.wstring());
    entry.sourcetransformidentity =
        QStringLiteral("source-transform-1");
    entry.sourcehash = QStringLiteral("source-hash");
    entry.resourcehash = QStringLiteral("resource-hash");
    entry.model = MakeSource();
    return entry;
}

void InitializeDocument(
    SceneDocument& document,
    SceneModelRepository& repository,
    const bool locked = false)
{
    const SceneModelRepositoryEntry entry = MakeEntry();
    repository.Store(entry);
    slicer_core::ModelInstance instance = MakeInstance();
    instance.locked = locked;
    document.SetLoading(1U, entry.modelpath);
    document.SetSceneContext(
        1U,
        QStringLiteral("scene-1"),
        1U,
        entry.cachekey,
        entry.sourcehash,
        entry.resourcehash,
        std::move(instance));
    document.SetGeometry(1U, MakeGeometry());
}

bool RepositoryRetainsImmutableSource()
{
    SceneModelRepository repository;
    const SceneModelRepositoryEntry entry = MakeEntry();
    const bool stored = repository.Store(entry);
    const auto found = repository.Find(entry.cachekey);
    return ExpectTrue(stored, "repository accepts immutable source")
        && ExpectTrue(found.has_value(), "repository finds source by key")
        && ExpectTrue(
            found->model.get() == entry.model.get(),
            "repository retains shared immutable source")
        && ExpectTrue(repository.Size() == 1U, "repository size is stable");
}

bool TransformCommandsUpdateRevisionAndRequestProjection()
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    InitializeDocument(document, repository);
    selection.SetSelectedInstance(QStringLiteral("instance-1"));

    SceneProjectionRequest projected;
    int projectionCount{0};
    SceneTransformController controller(
        &document,
        &selection,
        &repository);
    controller.SetProjectionRequester(
        [&](const SceneProjectionRequest& request)
        {
            projected = request;
            ++projectionCount;
        });

    slicer_core::ModelTransform transform;
    transform.translatexmm = 3.5;
    transform.translateymm = -2.25;
    transform.rotatezdeg = 450.0;
    transform.uniformscale = 1.5;
    const SceneTransformCommandResult result = controller.SetTransform(
        transform,
        1U,
        0U);
    const SceneTransformCommandResult equivalent = controller.SetTransform(
        document.Instance().value().transform,
        2U,
        1U);

    return ExpectTrue(result.IsValid() && result.changed, "transform changes")
        && ExpectTrue(
            document.SceneRevision() == 2U
                && document.Instance()->transformrevision == 1U,
            "scene and transform revisions increment")
        && ExpectTrue(
            NearlyEqual(document.Instance()->transform.rotatezdeg, 90.0),
            "rotation is normalized")
        && ExpectTrue(
            document.IsDirty() && document.IsGeometryStale(),
            "changed transform marks document dirty and geometry stale")
        && ExpectTrue(
            projectionCount == 1
                && projected.scenerevision == 2U
                && projected.instance.transformrevision == 1U
                && projected.cachekey == QStringLiteral("cache-1"),
            "latest transform requests identity-bound projection")
        && ExpectTrue(
            equivalent.IsValid() && !equivalent.changed,
            "equivalent transform is a no-op")
        && ExpectTrue(
            projectionCount == 1 && document.SceneRevision() == 2U,
            "no-op does not increment revision or reproject");
}

bool InvalidLockedAndStaleCommandsFailClosed()
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    InitializeDocument(document, repository);
    SceneTransformController controller(
        &document,
        &selection,
        &repository);

    slicer_core::ModelTransform transform;
    const auto noSelection = controller.SetTransform(transform, 1U, 0U);
    selection.SetSelectedInstance(QStringLiteral("instance-1"));
    const auto stale = controller.SetTransform(transform, 0U, 0U);

    transform.uniformscale = 0.0;
    const auto invalid = controller.SetTransform(transform, 1U, 0U);

    document.Reset();
    InitializeDocument(document, repository, true);
    selection.SetSelectedInstance(QStringLiteral("instance-1"));
    transform.uniformscale = 1.0;
    transform.translatexmm = 1.0;
    const auto lockedResult = controller.SetTransform(transform, 1U, 0U);

    return ExpectTrue(
               noSelection.error->code
                   == SceneTransformErrorCode::NoSelection,
               "missing selection is rejected")
        && ExpectTrue(
            stale.error->code
                == SceneTransformErrorCode::SceneRevisionStale,
            "stale scene revision is rejected")
        && ExpectTrue(
            invalid.error->code
                == SceneTransformErrorCode::ScaleNonPositive,
            "non-positive scale is rejected")
        && ExpectTrue(
            lockedResult.error->code
                == SceneTransformErrorCode::InstanceLocked,
            "locked instance is rejected")
        && ExpectTrue(
            document.SceneRevision() == 1U
                && document.Instance()->transformrevision == 0U,
            "rejected commands do not mutate revisions");
}

bool CenterAndResetUseEffectiveGeometry()
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    InitializeDocument(document, repository);
    selection.SetSelectedInstance(QStringLiteral("instance-1"));
    SceneTransformController controller(
        &document,
        &selection,
        &repository);
    controller.SetProjectionRequester(
        [](const SceneProjectionRequest&)
        {
        });

    const auto centered = controller.CenterAtSceneOrigin(1U, 0U);
    const slicer_core::ModelTransform centeredTransform =
        document.Instance()->transform;
    const auto reset = controller.ResetTransform(2U, 1U);

    return ExpectTrue(centered.IsValid() && centered.changed, "center succeeds")
        && ExpectTrue(
            NearlyEqual(centeredTransform.translatexmm, -1.0)
                && NearlyEqual(centeredTransform.translateymm, -2.0),
            "center uses effective XY bounds")
        && ExpectTrue(reset.IsValid() && reset.changed, "reset succeeds")
        && ExpectTrue(
            slicer_core::ModelTransformsEquivalent(
                document.Instance()->transform,
                slicer_core::ModelTransform{}),
            "reset restores identity instance transform");
}

bool EffectiveConfigSavesReadsBackAndRejectsStale()
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    InitializeDocument(document, repository);
    selection.SetSelectedInstance(QStringLiteral("instance-1"));
    SceneTransformController controller(
        &document,
        &selection,
        &repository);

    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_13a03_transform_session";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    SceneTransformSaveRequest request;
    request.sessiondirectory = root;
    request.sourceprofileid = "profile-a";
    request.generatedatutc = "2026-07-27T12:00:00.000Z";
    request.expectedscenerevision = 1U;
    request.expectedtransformrevision = 0U;
    const SceneTransformSaveResult saved =
        controller.SaveSceneEffectiveConfig(request);
    if (!saved.IsValid())
    {
        std::cerr << "SAVE ERROR "
                  << SceneTransformErrorCodeName(saved.error->code)
                  << ' '
                  << saved.error->message.toStdString()
                  << '\n';
    }
    const auto readback = slicer_core::ReadSceneEffectiveConfig(
        root / "scene_config.effective.json");

    request.expectedscenerevision = 0U;
    const SceneTransformSaveResult stale =
        controller.SaveSceneEffectiveConfig(request);

    request.expectedscenerevision = 1U;
    request.cancelled = true;
    const SceneTransformSaveResult cancelled =
        controller.SaveSceneEffectiveConfig(request);

    const bool result =
        ExpectTrue(saved.IsValid(), "scene effective config saves")
        && ExpectTrue(readback.IsValid(), "saved config reads back")
        && ExpectTrue(
            !slicer_core::IsSceneEffectiveConfigStale(
                readback.document,
                saved.scene),
            "saved identity and revisions match readback")
        && ExpectTrue(
            !document.IsDirty()
                && document.EffectiveConfigPath()
                    == QString::fromStdWString(
                        (root / "scene_config.effective.json").wstring()),
            "successful readback marks document saved")
        && ExpectTrue(
            stale.error->code
                == SceneTransformErrorCode::SceneRevisionStale,
            "stale save is rejected")
        && ExpectTrue(
            cancelled.error->code
                == SceneTransformErrorCode::SaveCancelled,
            "cancelled save is rejected");
    std::filesystem::remove_all(root, cleanupError);
    return result;
}

bool FailedSaveRestoresPreviousDraft()
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    InitializeDocument(document, repository);
    selection.SetSelectedInstance(QStringLiteral("instance-1"));
    SceneTransformController controller(
        &document,
        &selection,
        &repository);

    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_13a03_transform_rollback";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);
    const std::filesystem::path scenePath =
        root / "scene_config.draft.json";
    {
        std::ofstream output(scenePath, std::ios::binary);
        output << "previous-scene-draft";
    }

    SceneTransformSaveRequest request;
    request.sessiondirectory = root;
    request.sourceprofileid = "profile-a";
    request.generatedatutc = "2026-07-27T12:00:00.000Z";
    request.dpix = 0;
    request.expectedscenerevision = 1U;
    request.expectedtransformrevision = 0U;
    const SceneTransformSaveResult failed =
        controller.SaveSceneEffectiveConfig(request);

    std::ifstream input(scenePath, std::ios::binary);
    const std::string restored{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    const bool result =
        ExpectTrue(!failed.IsValid(), "invalid save request fails")
        && ExpectTrue(
            failed.error->code == SceneTransformErrorCode::SaveFailed,
            "failed effective config reports stable save error")
        && ExpectTrue(
            restored == "previous-scene-draft",
            "failed save restores previous scene draft")
        && ExpectTrue(
            !std::filesystem::exists(
                root / "scene_config.effective.json"),
            "failed save does not leave an effective config");
    std::filesystem::remove_all(root, cleanupError);
    return result;
}

bool MirrorTransformPersistsThroughSceneReadback()
{
    SceneDocument document;
    SceneSelectionModel selection;
    SceneModelRepository repository;
    InitializeDocument(document, repository);
    selection.SetSelectedInstance(QStringLiteral("instance-1"));
    SceneTransformController controller(
        &document,
        &selection,
        &repository);
    controller.SetProjectionRequester(
        [](const SceneProjectionRequest&)
        {
        });

    slicer_core::ModelTransform transform;
    transform.mirrorx = true;
    const SceneTransformCommandResult changed =
        controller.SetTransform(transform, 1U, 0U);
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_13a04_mirror_scene";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);

    SceneTransformSaveRequest request;
    request.sessiondirectory = root;
    request.sourceprofileid = "profile-a";
    request.generatedatutc = "2026-07-27T12:00:00.000Z";
    request.expectedscenerevision = 2U;
    request.expectedtransformrevision = 1U;
    const SceneTransformSaveResult saved =
        controller.SaveSceneEffectiveConfig(request);
    const auto readback = slicer_core::ReadSceneEffectiveConfig(
        root / "scene_config.effective.json");

    const bool result =
        ExpectTrue(
            changed.IsValid() && changed.changed,
            "mirror transform changes instance")
        && ExpectTrue(
            saved.IsValid() && readback.IsValid(),
            "mirrored scene saves and reads back")
        && ExpectTrue(
            saved.scene.instances.size() == 1U
                && saved.scene.instances.front()
                       .requestedtransform.mirrorx
                && saved.scene.instances.front()
                       .effectivetransform.mirrorx,
            "scene snapshot retains requested and effective mirror")
        && ExpectTrue(
            !slicer_core::IsSceneEffectiveConfigStale(
                readback.document,
                saved.scene),
            "mirrored effective config identity remains current");
    std::filesystem::remove_all(root, cleanupError);
    return result;
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application);

    bool ok = true;
    ok = RepositoryRetainsImmutableSource() && ok;
    ok = TransformCommandsUpdateRevisionAndRequestProjection() && ok;
    ok = InvalidLockedAndStaleCommandsFailClosed() && ok;
    ok = CenterAndResetUseEffectiveGeometry() && ok;
    ok = EffectiveConfigSavesReadsBackAndRejectsStale() && ok;
    ok = FailedSaveRestoresPreviousDraft() && ok;
    ok = MirrorTransformPersistsThroughSceneReadback() && ok;
    if (!ok)
    {
        return 1;
    }
    std::cout << "scene_transform_controller_unit_tests: PASS\n";
    return 0;
}
