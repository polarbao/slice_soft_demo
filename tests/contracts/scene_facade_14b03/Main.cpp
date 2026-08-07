#include "slicer_core/api/scene/SceneFacadeService.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{

class TestCancelToken final : public slicer_core::api::ICancelToken
{
public:
    explicit TestCancelToken(const bool cancelled = false)
        : m_cancelled(cancelled)
    {
    }

    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return m_cancelled;
    }

private:
    bool m_cancelled{false};
};

class TestViewDataProvider final
    : public slicer_core::api::ITexturedSceneViewDataProvider
{
public:
    [[nodiscard]] slicer_core::api::ApiResult<
        slicer_core::api::SceneViewData> GetViewData(
        const slicer_core::api::SceneViewDataRequest& request,
        const slicer_core::api::SceneSnapshot& snapshot,
        const slicer_core::api::ICancelToken&) const noexcept override
    {
        ++m_callCount;
        slicer_core::api::SceneViewData result;
        result.view_mode = request.view_mode;
        result.scene_revision = snapshot.scene_revision;
        result.viewdata_identity = "provider-viewdata";

        slicer_core::api::ViewAppearance appearance;
        appearance.appearance_identity = "appearance-untextured";
        slicer_core::api::ViewMaterial material;
        material.material_id = "material-base";
        appearance.materials.push_back(material);
        result.appearances.push_back(appearance);

        for (const slicer_core::api::SceneInstanceState& state :
             snapshot.instances)
        {
            slicer_core::api::ViewInstance instance;
            instance.instance_id = state.instance.instance_id;
            instance.model_id = state.instance.model_id;
            instance.world_matrix = state.instance.world_matrix;
            instance.local_bounds_mm = state.effective_bounds_mm;
            instance.texture_status =
                slicer_core::api::TextureStatus::NotProvided;
            instance.appearance_identity = "appearance-untextured";
            if (request.view_mode == slicer_core::api::ViewMode::Top)
            {
                slicer_core::api::SurfacePreview preview;
                preview.width_px = 1;
                preview.height_px = 1;
                preview.rgba8 = {255U, 255U, 255U, 255U};
                preview.local_bounds_mm = state.effective_bounds_mm;
                preview.preview_identity = "preview-untextured";
                preview.appearance_identity = "appearance-untextured";
                instance.preview_identity = preview.preview_identity;
                instance.surface_preview = std::move(preview);
            }
            result.instances.push_back(std::move(instance));
        }
        return slicer_core::api::ApiResult<
            slicer_core::api::SceneViewData>::Success(std::move(result));
    }

    [[nodiscard]] int CallCount() const noexcept
    {
        return m_callCount;
    }

private:
    mutable int m_callCount{0};
};

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "SceneFacade 14B-03: " << message << '\n';
        std::exit(1);
    }
}

void RequireError(
    const slicer_core::api::ApiError* error,
    const std::string& code,
    const std::string& message)
{
    Require(error != nullptr, message + " should return an error");
    Require(error->code == code, message + " returned " + error->code);
}

slicer_core::SceneModel MakeModel(const std::string& name)
{
    slicer_core::SceneModel model;
    model.model_path = "models/" + name + ".obj";
    model.format = "obj";
    model.vertex_count = 8U;
    model.face_count = 4U;
    model.triangle_count = 4U;
    model.bbox_mm = {{0.0, 0.0, 0.0}, {10.0, 10.0, 1.0}};
    model.triangles = {
        {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, {10.0, 10.0, 0.0}},
        {{0.0, 0.0, 0.0}, {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}},
        {{0.0, 0.0, 1.0}, {10.0, 10.0, 1.0}, {10.0, 0.0, 1.0}},
        {{0.0, 0.0, 1.0}, {0.0, 10.0, 1.0}, {10.0, 10.0, 1.0}},
    };
    return model;
}

slicer_core::ModelSource MakeModelSource(const std::string& modelId)
{
    slicer_core::ModelSource source;
    source.modelid = modelId;
    source.sourcepath = "models/" + modelId + ".obj";
    source.format = "obj";
    source.resourcescopeid = "scope-models";
    source.sourcehash = "source-" + modelId;
    source.resourcehash = "resource-" + modelId;
    source.displayname = modelId;
    return source;
}

slicer_core::SceneModelInstance MakeInstance(
    const std::string& instanceId,
    const std::string& modelId,
    const double translateX)
{
    slicer_core::SceneModelInstance sceneInstance;
    sceneInstance.instance.instanceid = instanceId;
    sceneInstance.instance.modelid = modelId;
    sceneInstance.instance.sourcetransformidentity = "source-transform";
    sceneInstance.instance.transformrevision = 1U;
    sceneInstance.instance.sourcebboxmm = {
        {0.0, 0.0, 0.0},
        {10.0, 10.0, 1.0}};
    sceneInstance.derivedlayouttransform.translatexmm = translateX;
    sceneInstance.derivedlayouttransform.translateymm = 10.0;
    sceneInstance.effectivetransform =
        sceneInstance.derivedlayouttransform;
    sceneInstance.instance.transform = sceneInstance.effectivetransform;
    sceneInstance.instance.effectivebboxmm = {
        {translateX, 10.0, 0.0},
        {translateX + 10.0, 20.0, 1.0}};
    sceneInstance.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;
    sceneInstance.resolvedprofileid = "profile-test";
    return sceneInstance;
}

slicer_core::api::SceneFacadeSeed MakeSeed()
{
    slicer_core::api::SceneFacadeSeed seed;
    seed.scene_id = 42U;
    seed.validation_purpose =
        slicer_core::SceneValidationPurpose::FunctionalFixture;
    seed.scene.sceneid = "scene-14b03";
    seed.scene.scenerevision = 7U;
    seed.scene.resolvedprofileid = "profile-test";
    seed.scene.buildvolume.source = slicer_core::BuildVolumeSource::Fixture;
    seed.scene.buildvolume.widthmm = 100.0;
    seed.scene.buildvolume.heightmm = 80.0;
    seed.scene.buildvolume.zlimitmm = 60.0;
    seed.scene.buildvolume.origin = slicer_core::BuildVolumeOrigin::LowerLeft;
    seed.scene.buildvolume.xdirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    seed.scene.buildvolume.ydirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    seed.scene.buildvolume.isfixture = true;

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-models";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = "models";
    seed.scene.resourcescopes.push_back(scope);
    seed.scene.models.push_back(MakeModelSource("model-a"));
    seed.scene.models.push_back(MakeModelSource("model-b"));
    seed.scene.instances.push_back(
        MakeInstance("instance-a", "model-a", 10.0));
    seed.scene.instances.push_back(
        MakeInstance("instance-b", "model-b", 30.0));
    seed.models_by_id.emplace(
        "model-a",
        std::make_shared<const slicer_core::SceneModel>(MakeModel("model-a")));
    seed.models_by_id.emplace(
        "model-b",
        std::make_shared<const slicer_core::SceneModel>(MakeModel("model-b")));
    seed.api_model_ids = {{"model-a", 101U}, {"model-b", 102U}};
    slicer_core::api::SceneFacadeModelRegistration registration;
    registration.api_model_id = 101U;
    registration.scene_model_id = "model-a";
    registration.source = seed.scene.models.front();
    registration.scope = seed.scene.resourcescopes.front();
    registration.model = seed.models_by_id.at("model-a");
    seed.registered_models.emplace(101U, std::move(registration));
    return seed;
}

std::shared_ptr<slicer_core::api::SceneFacadeService> CreateFacade(
    std::shared_ptr<const slicer_core::api::ITexturedSceneViewDataProvider>
        provider = {})
{
    auto created = slicer_core::api::SceneFacadeService::Create(
        MakeSeed(),
        std::move(provider));
    Require(created.IsOk(), "valid seed should create a facade");
    return *created.Value();
}

void VerifiesCanonicalSignedZeroHash()
{
    slicer_core::MultiModelScene negativeZeroScene = MakeSeed().scene;
    negativeZeroScene.instances.front().instance.sourcebboxmm.min.x = -0.0;
    negativeZeroScene.instances.front().requestedtransform.translatexmm = -0.0;
    negativeZeroScene.layout.columngapmm = -0.0;

    slicer_core::MultiModelScene positiveZeroScene = negativeZeroScene;
    positiveZeroScene.instances.front().instance.sourcebboxmm.min.x = 0.0;
    positiveZeroScene.instances.front().requestedtransform.translatexmm = 0.0;
    positiveZeroScene.layout.columngapmm = 0.0;

    Require(
        slicer_core::ComputeMultiModelSceneHash(negativeZeroScene)
            == slicer_core::ComputeMultiModelSceneHash(positiveZeroScene),
        "scene hash should canonicalize signed zero values");
}

slicer_core::api::SceneOperationRequest Translate(
    const std::string& operationId,
    const std::uint64_t revision,
    const std::string& instanceId,
    const double x)
{
    slicer_core::api::SceneOperation operation;
    operation.type = slicer_core::api::SceneOperationType::Translate;
    operation.instance_id = instanceId;
    operation.value_x = x;

    slicer_core::api::SceneOperationRequest request;
    request.scene_id = 42U;
    request.operation_id = operationId;
    request.current_scene_revision = revision;
    request.expected_scene_revision = revision;
    request.operations.push_back(operation);
    return request;
}

void VerifiesRevisionReplayAndAtomicity()
{
    const TestCancelToken active;
    const auto facade = CreateFacade();
    const auto initial = facade->GetSnapshot(42U);
    Require(initial.IsOk(), "initial snapshot should be available");
    Require(initial.Value()->scene_revision == 7U, "initial revision should be 7");

    const auto request = Translate("operation-1", 7U, "instance-a", 5.0);
    const auto committed = facade->ApplyOperation(request, active);
    Require(committed.IsOk(), "valid operation should commit");
    Require(committed.Value()->snapshot.scene_revision == 8U,
            "commit should increment revision once");
    Require(committed.Value()->snapshot.scene_hash != initial.Value()->scene_hash,
            "commit should update scene hash");
    Require(committed.Value()->snapshot.build_volume.width_mm == 100.0
                && committed.Value()->snapshot.build_volume.height_mm == 80.0,
            "normal Commit response should carry the resolved build volume");
    Require(committed.Value()->viewdata_identity.starts_with("vd:8:scene:auto:"),
            "normal Commit response should carry a ViewData snapshot identity");
    Require(committed.Value()->preflight_delta.size() == 1U,
            "normal Commit response should carry touched-instance preflight delta");

    const auto replay = facade->ApplyOperation(request, active);
    Require(replay.IsOk(), "same operation replay should succeed");
    Require(replay.Value()->snapshot.scene_revision == 8U,
            "same operation replay must not increment revision");
    Require(replay.Value()->snapshot.scene_hash
                == committed.Value()->snapshot.scene_hash,
            "same operation replay should return the exact snapshot");

    auto conflictingRequest = request;
    conflictingRequest.operations.front().value_x = 6.0;
    const auto conflict = facade->ApplyOperation(conflictingRequest, active);
    RequireError(conflict.Error(), "PM-SLICER-PROFILE-0031",
                 "operationId payload conflict");
    Require(facade->GetSnapshot(42U).Value()->scene_revision == 8U,
            "operationId conflict must not mutate the scene");

    auto revisionMismatch = Translate(
        "operation-revision-mismatch", 8U, "instance-a", 1.0);
    revisionMismatch.current_scene_revision = 7U;
    const auto mismatched = facade->ApplyOperation(revisionMismatch, active);
    RequireError(mismatched.Error(), "PM-SLICER-PROFILE-0031",
                 "current and expected revision mismatch");
    Require(facade->GetSnapshot(42U).Value()->scene_revision == 8U,
            "revision field mismatch must not mutate the scene");

    const auto stale = facade->ApplyOperation(
        Translate("operation-stale", 7U, "instance-a", 1.0), active);
    RequireError(stale.Error(), "PM-SLICER-LAYOUT-0022", "stale operation");
    Require(facade->GetSnapshot(42U).Value()->scene_revision == 8U,
            "stale operation must not mutate the scene");

    auto atomicRequest = Translate("operation-atomic", 8U, "instance-a", 2.0);
    slicer_core::api::SceneOperation invalid;
    invalid.type = slicer_core::api::SceneOperationType::MirrorX;
    invalid.instance_id = "instance-missing";
    atomicRequest.operations.push_back(invalid);
    const std::string hashBefore = facade->GetSnapshot(42U).Value()->scene_hash;
    const auto atomic = facade->ApplyOperation(atomicRequest, active);
    RequireError(atomic.Error(), "PM-SLICER-PROFILE-0031",
                 "invalid atomic batch");
    const auto afterAtomic = facade->GetSnapshot(42U);
    Require(afterAtomic.Value()->scene_revision == 8U
                && afterAtomic.Value()->scene_hash == hashBefore,
            "failed batch must leave revision and hash unchanged");
}

void VerifiesAuthoritativeCollisionAndBounds()
{
    const TestCancelToken active;
    const auto facade = CreateFacade();
    const auto overlap = facade->ApplyOperation(
        Translate("operation-overlap", 7U, "instance-a", 15.0), active);
    Require(overlap.IsOk(), "collision state should commit atomically");
    Require(overlap.Value()->collision_report.collisions.size() == 1U,
            "normal Commit response should carry authoritative collisions");
    const auto collision = facade->CheckCollision(
        overlap.Value()->snapshot, active);
    Require(collision.IsOk(), "current snapshot collision query should pass");
    Require(collision.Value()->collisions.size() == 1U,
            "positive-area overlap should produce one authoritative pair");
    Require(collision.Value()->collisions.front().instance_a == "instance-a"
                && collision.Value()->collisions.front().instance_b == "instance-b",
            "collision pair identity should match existing layout service order");

    const auto outOfBounds = facade->ApplyOperation(
        Translate("operation-oob", 8U, "instance-a", -30.0), active);
    Require(outOfBounds.IsOk(), "out-of-bounds state should commit with evidence");
    Require(outOfBounds.Value()->collision_report.out_of_bounds_instances.size()
                == 1U,
            "normal Commit response should carry out-of-bounds identities");
    const auto boundsReport = facade->CheckCollision(
        outOfBounds.Value()->snapshot, active);
    Require(boundsReport.IsOk(), "current out-of-bounds report should be readable");
    Require(boundsReport.Value()->out_of_bounds_instances.size() == 1U
                && boundsReport.Value()->out_of_bounds_instances.front()
                    == "instance-a",
            "existing build-volume evaluator should identify instance-a");
    Require(outOfBounds.Value()->snapshot.instances.front().out_of_bounds,
            "snapshot instance should carry authoritative out-of-bounds state");

    const auto staleCollision = facade->CheckCollision(
        overlap.Value()->snapshot, active);
    RequireError(staleCollision.Error(), "PM-SLICER-LAYOUT-0022",
                 "stale collision snapshot");
}

void VerifiesAddRemoveReplayAndAtomicity()
{
    const TestCancelToken active;
    const auto facade = CreateFacade();

    slicer_core::api::SceneOperation add;
    add.type = slicer_core::api::SceneOperationType::AddInstance;
    add.instance_id = "instance-added";
    add.model_id = 101U;
    add.initial_transform.translatexmm = 55.0;
    add.initial_transform.translateymm = 10.0;

    slicer_core::api::SceneOperationRequest addRequest;
    addRequest.scene_id = 42U;
    addRequest.operation_id = "operation-add";
    addRequest.current_scene_revision = 7U;
    addRequest.expected_scene_revision = 7U;
    addRequest.scene_context_identity = "context-a";
    addRequest.operations.push_back(add);

    const auto added = facade->ApplyOperation(addRequest, active);
    Require(added.IsOk(), "registered addInstance should commit");
    Require(added.Value()->snapshot.scene_revision == 8U
                && added.Value()->snapshot.instances.size() == 3U,
            "addInstance should append one authoritative instance");
    Require(added.Value()->snapshot.instances.back().instance.instance_id
                == "instance-added"
                && added.Value()->snapshot.instances.back().instance.model_id
                    == 101U,
            "addInstance should preserve assigned and imported identities");
    Require(
        added.Value()->snapshot.scene.instances.back()
                .instance.sourcetransformidentity
            == "models/model-a.obj",
        "addInstance should preserve the registered source path identity");

    const auto replay = facade->ApplyOperation(addRequest, active);
    Require(replay.IsOk()
                && replay.Value()->snapshot.scene_revision == 8U
                && replay.Value()->snapshot.instances.size() == 3U,
            "addInstance exact replay must not duplicate the instance");

    auto changedContext = addRequest;
    changedContext.scene_context_identity = "context-b";
    const auto contextConflict = facade->ApplyOperation(
        changedContext,
        active);
    RequireError(
        contextConflict.Error(),
        "PM-SLICER-PROFILE-0031",
        "addInstance replay with changed sceneContext");

    auto duplicateRequest = addRequest;
    duplicateRequest.operation_id = "operation-add-duplicate";
    duplicateRequest.current_scene_revision = 8U;
    duplicateRequest.expected_scene_revision = 8U;
    duplicateRequest.operations.front().instance_id = "instance-a";
    const auto duplicate = facade->ApplyOperation(duplicateRequest, active);
    RequireError(
        duplicate.Error(),
        "PM-SLICER-PROFILE-0031",
        "duplicate addInstance identity");
    Require(facade->GetSnapshot(42U).Value()->scene_revision == 8U,
            "duplicate addInstance must not mutate the scene");

    auto unknownModelRequest = duplicateRequest;
    unknownModelRequest.operation_id = "operation-add-unknown-model";
    unknownModelRequest.operations.front().instance_id = "instance-unknown";
    unknownModelRequest.operations.front().model_id = 999U;
    const auto unknownModel = facade->ApplyOperation(
        unknownModelRequest,
        active);
    RequireError(
        unknownModel.Error(),
        "PM-SLICER-INPUT-0001",
        "addInstance unknown modelId");

    slicer_core::api::SceneOperationRequest addTransformRequest;
    addTransformRequest.scene_id = 42U;
    addTransformRequest.operation_id = "operation-add-transform";
    addTransformRequest.current_scene_revision = 8U;
    addTransformRequest.expected_scene_revision = 8U;
    addTransformRequest.operations.push_back(add);
    addTransformRequest.operations.front().instance_id = "instance-batch";
    addTransformRequest.operations.front().initial_transform.translatexmm =
        65.0;
    slicer_core::api::SceneOperation translate;
    translate.type = slicer_core::api::SceneOperationType::Translate;
    translate.instance_id = "instance-batch";
    translate.value_x = 2.0;
    addTransformRequest.operations.push_back(translate);
    const auto addTransform = facade->ApplyOperation(
        addTransformRequest,
        active);
    Require(addTransform.IsOk()
                && addTransform.Value()->snapshot.scene_revision == 9U
                && addTransform.Value()->snapshot.instances.size() == 4U,
            "add then transform should commit in request order");
    Require(
        addTransform.Value()->snapshot.instances.back()
                .instance.world_matrix.values[3]
            == 67.0,
        "add initial transform and same-batch translate should compose");

    slicer_core::api::SceneOperation remove;
    remove.type = slicer_core::api::SceneOperationType::RemoveInstance;
    remove.instance_id = "instance-batch";
    slicer_core::api::SceneOperationRequest removeRequest;
    removeRequest.scene_id = 42U;
    removeRequest.operation_id = "operation-remove";
    removeRequest.current_scene_revision = 9U;
    removeRequest.expected_scene_revision = 9U;
    removeRequest.operations.push_back(remove);
    const auto removed = facade->ApplyOperation(removeRequest, active);
    Require(removed.IsOk()
                && removed.Value()->snapshot.scene_revision == 10U
                && removed.Value()->snapshot.instances.size() == 3U,
            "removeInstance should remove one authoritative instance");

    auto removeMissingRequest = removeRequest;
    removeMissingRequest.operation_id = "operation-remove-missing";
    removeMissingRequest.current_scene_revision = 10U;
    removeMissingRequest.expected_scene_revision = 10U;
    removeMissingRequest.operations.front().instance_id = "missing";
    const auto removeMissing = facade->ApplyOperation(
        removeMissingRequest,
        active);
    RequireError(
        removeMissing.Error(),
        "PM-SLICER-PROFILE-0031",
        "removeInstance unknown instance");

    slicer_core::api::SceneOperationRequest addRemoveRequest;
    addRemoveRequest.scene_id = 42U;
    addRemoveRequest.operation_id = "operation-add-remove";
    addRemoveRequest.current_scene_revision = 10U;
    addRemoveRequest.expected_scene_revision = 10U;
    addRemoveRequest.operations.push_back(add);
    addRemoveRequest.operations.front().instance_id = "instance-transient";
    remove.instance_id = "instance-transient";
    addRemoveRequest.operations.push_back(remove);
    const auto addRemove = facade->ApplyOperation(addRemoveRequest, active);
    Require(addRemove.IsOk()
                && addRemove.Value()->snapshot.scene_revision == 11U
                && addRemove.Value()->snapshot.instances.size() == 3U,
            "same-batch add then remove should commit with no residual instance");
}

void VerifiesGridLayoutCommitAndAtomicity()
{
    const TestCancelToken active;
    const auto facade = CreateFacade();
    slicer_core::api::SceneOperation layout;
    layout.type = slicer_core::api::SceneOperationType::ApplyGridLayout;
    layout.layout.maxcolumns = 2;
    layout.layout.maxrows = 1;
    layout.layout.columngapmm = 10.0;
    layout.layout.rowgapmm = 10.0;

    slicer_core::api::SceneOperationRequest request;
    request.scene_id = 42U;
    request.operation_id = "operation-grid-layout";
    request.current_scene_revision = 7U;
    request.expected_scene_revision = 7U;
    request.operations.push_back(layout);
    const auto committed = facade->ApplyOperation(request, active);
    Require(
        committed.IsOk()
            && committed.Value()->snapshot.scene_revision == 8U
            && committed.Value()->snapshot.instances.size() == 2U,
        "grid layout should commit all instances in one revision");
    Require(
        committed.Value()->snapshot.instances.at(0U)
                .effective_bounds_mm.min_mm[0]
            == 0.0
            && committed.Value()->snapshot.instances.at(1U)
                    .effective_bounds_mm.min_mm[0]
                == 20.0
            && committed.Value()->collision_report.collisions.empty(),
        "grid layout should reuse row-major edge-clearance semantics");

    const auto replay = facade->ApplyOperation(request, active);
    Require(
        replay.IsOk()
            && replay.Value()->snapshot.scene_revision == 8U
            && replay.Value()->snapshot.scene_hash
                == committed.Value()->snapshot.scene_hash,
        "grid layout exact replay should return the original commit");

    auto changedReplay = request;
    changedReplay.operations.front().layout.columngapmm = 9.0;
    RequireError(
        facade->ApplyOperation(changedReplay, active).Error(),
        "PM-SLICER-PROFILE-0031",
        "grid layout replay with changed parameters");

    auto mixed = request;
    mixed.operation_id = "operation-grid-layout-mixed";
    mixed.current_scene_revision = 8U;
    mixed.expected_scene_revision = 8U;
    slicer_core::api::SceneOperation translate;
    translate.type = slicer_core::api::SceneOperationType::Translate;
    translate.instance_id = "instance-a";
    translate.value_x = 1.0;
    mixed.operations.push_back(translate);
    RequireError(
        facade->ApplyOperation(mixed, active).Error(),
        "PM-SLICER-PROFILE-0031",
        "grid layout mixed operation batch");
    Require(
        facade->GetSnapshot(42U).Value()->scene_revision == 8U,
        "rejected grid layout requests must not mutate authority state");
}

void VerifiesCancellationAndProviderBoundary()
{
    const TestCancelToken active;
    const TestCancelToken cancelled(true);
    const auto facade = CreateFacade();
    const auto cancellation = facade->ApplyOperation(
        Translate("operation-cancel", 7U, "instance-a", 1.0), cancelled);
    RequireError(cancellation.Error(), "PM-SLICER-CANCELLED-0070",
                 "cancelled operation");
    Require(facade->GetSnapshot(42U).Value()->scene_revision == 7U,
            "cancelled operation must not mutate the scene");

    slicer_core::api::SceneViewDataRequest request;
    request.scene_id = 42U;
    request.expected_scene_revision = 7U;
    request.view_mode = slicer_core::api::ViewMode::Top;
    request.lod = slicer_core::api::ViewLod::OutlineOnly;
    request.max_bytes = 1024U;
    const auto missingProvider = facade->GetViewData(request, active);
    RequireError(missingProvider.Error(), "PM-SLICER-INTERNAL-0099",
                 "missing 14B-03A provider");
    Require(missingProvider.Error()->detail.find("14B-03A")
                != std::string::npos,
            "missing provider error should name the explicit prerequisite");

    const auto provider = std::make_shared<TestViewDataProvider>();
    const auto providerFacade = CreateFacade(provider);
    const auto view = providerFacade->GetViewData(request, active);
    Require(view.IsOk(), "installed provider should receive ViewData request");
    Require(provider->CallCount() == 1, "provider should be called exactly once");
    Require(view.Value()->instances.size() == 2U
                && view.Value()->instances.front().surface_preview.has_value(),
            "provider result should preserve a top surface preview");

    request.view_mode = slicer_core::api::ViewMode::ThreeD;
    const auto invalidThreeD = providerFacade->GetViewData(request, active);
    RequireError(invalidThreeD.Error(), "PM-SLICER-PROFILE-0031",
                 "three_d outline_only request");
    Require(provider->CallCount() == 1,
            "invalid three_d request must not reach the provider");
}

}  // namespace

int main()
{
    VerifiesCanonicalSignedZeroHash();
    VerifiesRevisionReplayAndAtomicity();
    VerifiesAuthoritativeCollisionAndBounds();
    VerifiesAddRemoveReplayAndAtomicity();
    VerifiesGridLayoutCommitAndAtomicity();
    VerifiesCancellationAndProviderBoundary();
    std::cout << "SceneFacade Stage 14B-03 independent tests: PASS\n";
    return 0;
}
