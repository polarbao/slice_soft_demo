#include "slicer_core/config.h"
#include "slicer_core/model.h"
#include "slicer_core/preflight/SceneFullPreflightService.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

namespace
{

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        std::exit(1);
    }
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Require(static_cast<bool>(input), "fixture source must be readable");
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

std::filesystem::path FixturePath()
{
    return std::filesystem::absolute(
        std::filesystem::path(SLICESOFT_SOURCE_DIR)
        / "samples/models/openvdb/surface_shell_cube.obj")
        .lexically_normal();
}

slicer_core::SceneModel LoadClosedCube()
{
    slicer_core::SliceConfig config;
    config.input.model_path = FixturePath();
    config.input.format = "obj";
    config.auto_orient.enabled = false;
    return slicer_core::load_model_report(
        config, std::filesystem::path(SLICESOFT_SOURCE_DIR));
}

slicer_core::SceneModel MakeOpenTriangle()
{
    slicer_core::SceneModel model;
    model.model_path = FixturePath();
    model.format = "obj";
    model.triangles = {{
        {0.0, 0.0, 0.0},
        {2.0, 0.0, 0.0},
        {0.0, 2.0, 0.0},
    }};
    model.triangle_count = model.triangles.size();
    model.bbox_mm = {{0.0, 0.0, 0.0}, {2.0, 2.0, 0.0}};
    return model;
}

slicer_core::SceneModel MakeBudgetBlockedModel()
{
    slicer_core::SceneModel model;
    model.model_path = FixturePath();
    model.format = "obj";
    model.triangles = {
        {{0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {0.0, 2.0, 0.0}},
        {{0.5, 0.5, 0.0}, {1.5, 0.5, 0.0}, {0.5, 1.5, 0.0}},
    };
    model.triangle_count = model.triangles.size();
    model.bbox_mm = {{0.0, 0.0, 0.0}, {2.0, 2.0, 0.1}};
    return model;
}

slicer_core::BoundingBox TranslateBounds(
    slicer_core::BoundingBox bounds,
    const slicer_core::ModelTransform& transform)
{
    bounds.min.x += transform.translatexmm;
    bounds.max.x += transform.translatexmm;
    bounds.min.y += transform.translateymm;
    bounds.max.y += transform.translateymm;
    return bounds;
}

slicer_core::MultiModelScene MakeScene(
    const slicer_core::SceneModel& model,
    const int instanceCount = 1,
    const bool hideLast = false)
{
    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-authoritative-preflight";
    scene.scenerevision = 7U;
    scene.buildvolume = slicer_core::MakeDefaultDeviceBuildVolume();
    scene.resolvedprofileid = "profile-stage14d-r3";

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-model-1";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = FixturePath().parent_path();
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource source;
    source.modelid = "model-1";
    source.sourcepath = FixturePath();
    source.format = "obj";
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash = slicer_core::ComputeSha256(ReadFile(source.sourcepath));
    source.resourcehash = slicer_core::ComputeSceneResourceHash(model);
    source.displayname = "stage14d-r3-model";
    scene.models.push_back(source);

    const double width = model.bbox_mm.max.x - model.bbox_mm.min.x;
    for (int index{0}; index < instanceCount; ++index)
    {
        slicer_core::SceneModelInstance item;
        item.instance.instanceid = "instance-" + std::to_string(index + 1);
        item.instance.modelid = source.modelid;
        item.instance.sourcetransformidentity = source.resourcehash;
        item.instance.sourcebboxmm = model.bbox_mm;
        item.instance.transform.translatexmm =
            10.0 - model.bbox_mm.min.x
            + static_cast<double>(index) * (width + 5.0);
        item.instance.transform.translateymm = 10.0 - model.bbox_mm.min.y;
        item.instance.transformrevision = 1U;
        item.instance.visible = !(hideLast && index == instanceCount - 1);
        item.instance.effectivebboxmm = TranslateBounds(
            model.bbox_mm, item.instance.transform);
        item.requestedtransform = item.instance.transform;
        item.effectivetransform = item.instance.transform;
        item.resolvedprofileid = scene.resolvedprofileid;
        scene.instances.push_back(std::move(item));
    }
    return scene;
}

slicer_core::SceneFullPreflightRequest MakeRequest(
    const slicer_core::MultiModelScene& scene,
    const std::shared_ptr<const slicer_core::SceneModel>& model,
    int* resolveCount = nullptr)
{
    slicer_core::SceneFullPreflightRequest request;
    request.scene = &scene;
    request.scenehash = slicer_core::ComputeMultiModelSceneHash(scene);
    request.expectedscenerevision = scene.scenerevision;
    request.admissioncontext.global_backend_available = true;
    request.modelresolver = [model, resolveCount](
        const slicer_core::ModelSource&)
    {
        if (resolveCount != nullptr)
        {
            ++(*resolveCount);
        }
        return slicer_core::SceneFullPreflightResolvedModel{model, {}, {}};
    };
    return request;
}

bool HasIssue(
    const slicer_core::SceneFullPreflightResult& result,
    const std::string& code)
{
    for (const slicer_core::SceneFullPreflightIssue& issue : result.sceneissues)
    {
        if (issue.code == code)
        {
            return true;
        }
    }
    for (const slicer_core::SceneFullPreflightInstanceResult& item :
         result.instances)
    {
        for (const slicer_core::SceneFullPreflightIssue& issue : item.issues)
        {
            if (issue.code == code)
            {
                return true;
            }
        }
    }
    return false;
}

void PassesAndIsDeterministic()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        LoadClosedCube());
    const slicer_core::MultiModelScene scene = MakeScene(*model, 2);
    int resolveCount{0};
    slicer_core::SceneFullPreflightService service;
    const slicer_core::SceneFullPreflightResult first = service.Run(
        MakeRequest(scene, model, &resolveCount));
    const slicer_core::SceneFullPreflightResult second = service.Run(
        MakeRequest(scene, model, &resolveCount));

    Require(first.authoritative && first.productionadmitted,
        "closed non-overlapping scene must pass");
    Require(first.complete && first.checkedmodelcount == 1U
            && first.checkedinstancecount == 2U,
        "all required checks must complete");
    Require(resolveCount == 2,
        "one resolver call per model per run is required");
    Require(first.instances.size() == second.instances.size()
            && first.instances.front().instanceid
                == second.instances.front().instanceid
            && first.instances.front().transformhash
                == second.instances.front().transformhash,
        "repeated runs must preserve stable ordering and identity");
}

void SkipsHiddenAndReportsSpatialBlockers()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        LoadClosedCube());
    slicer_core::SceneFullPreflightService service;

    const slicer_core::MultiModelScene hiddenScene = MakeScene(*model, 2, true);
    const auto hidden = service.Run(MakeRequest(hiddenScene, model));
    Require(hidden.authoritative && hidden.productionadmitted
            && hidden.skippedinstancecount == 1U,
        "hidden instances must be skipped without blocking production");

    slicer_core::MultiModelScene collisionScene = MakeScene(*model, 2);
    collisionScene.instances[1].instance.transform =
        collisionScene.instances[0].instance.transform;
    collisionScene.instances[1].requestedtransform =
        collisionScene.instances[0].requestedtransform;
    collisionScene.instances[1].effectivetransform =
        collisionScene.instances[0].effectivetransform;
    collisionScene.instances[1].instance.effectivebboxmm =
        collisionScene.instances[0].instance.effectivebboxmm;
    const auto collision = service.Run(MakeRequest(collisionScene, model));
    Require(collision.authoritative && !collision.productionadmitted
            && !collision.collisions.empty(),
        "positive-area overlap must be an authoritative blocker");

    slicer_core::MultiModelScene outsideScene = MakeScene(*model);
    auto& outside = outsideScene.instances.front();
    outside.instance.transform.translatexmm += 300.0;
    outside.requestedtransform = outside.instance.transform;
    outside.effectivetransform = outside.instance.transform;
    outside.instance.effectivebboxmm = TranslateBounds(
        model->bbox_mm, outside.instance.transform);
    const auto outOfBounds = service.Run(MakeRequest(outsideScene, model));
    Require(outOfBounds.authoritative && !outOfBounds.productionadmitted
            && outOfBounds.outofboundsinstances.size() == 1U,
        "out-of-volume geometry must be an authoritative blocker");
}

void FailsClosedForIdentityErrors()
{
    const auto model = std::make_shared<const slicer_core::SceneModel>(
        LoadClosedCube());
    slicer_core::SceneFullPreflightService service;

    const slicer_core::MultiModelScene goodScene = MakeScene(*model);
    auto staleRequest = MakeRequest(goodScene, model);
    staleRequest.scenehash = "stale";
    const auto stale = service.Run(staleRequest);
    Require(!stale.authoritative && HasIssue(stale, "PM-SLICER-LAYOUT-0022"),
        "stale scene identity must fail closed");

    slicer_core::MultiModelScene missingScene = MakeScene(*model);
    missingScene.models.front().sourcepath =
        FixturePath().parent_path() / "missing.obj";
    const auto missing = service.Run(MakeRequest(missingScene, model));
    Require(!missing.authoritative
            && HasIssue(missing, "PM-SLICER-INPUT-0001"),
        "missing source must fail closed");

    slicer_core::MultiModelScene hashScene = MakeScene(*model);
    hashScene.models.front().sourcehash = "changed";
    const auto changed = service.Run(MakeRequest(hashScene, model));
    Require(!changed.authoritative
            && HasIssue(changed, "PM-SLICER-INPUT-0001"),
        "source hash mismatch must fail closed");

    slicer_core::MultiModelScene scopeScene = MakeScene(*model);
    scopeScene.resourcescopes.front().rootpath =
        FixturePath().parent_path().parent_path() / "unrelated";
    const auto escaped = service.Run(MakeRequest(scopeScene, model));
    Require(!escaped.authoritative
            && HasIssue(escaped, "PM-SLICER-INPUT-0001"),
        "resource scope escape must fail closed");
}

void AppliesModeBudgetAndCancellationRules()
{
    const auto openModel = std::make_shared<const slicer_core::SceneModel>(
        MakeOpenTriangle());
    const slicer_core::MultiModelScene openScene = MakeScene(*openModel);
    slicer_core::SceneFullPreflightService service;
    auto legacyRequest = MakeRequest(openScene, openModel);
    const auto legacy = service.Run(legacyRequest);
    Require(legacy.authoritative && legacy.productionadmitted,
        "legacy mode must admit completed topology warnings");

    auto globalRequest = MakeRequest(openScene, openModel);
    globalRequest.targetmode =
        slicer_core::ModelPreflightPipelineMode::GlobalSurfaceShell;
    const auto global = service.Run(globalRequest);
    Require(global.authoritative && !global.productionadmitted,
        "global mode must block open topology");

    const auto budgetModel = std::make_shared<const slicer_core::SceneModel>(
        MakeBudgetBlockedModel());
    const slicer_core::MultiModelScene budgetScene = MakeScene(*budgetModel);
    auto budgetRequest = MakeRequest(budgetScene, budgetModel);
    budgetRequest.options.maxCompleteSelfIntersectionCandidatePairs = 0U;
    const auto budget = service.Run(budgetRequest);
    Require(!budget.authoritative && !budget.complete,
        "incomplete self-intersection audit must fail closed");

    auto cancelledRequest = MakeRequest(openScene, openModel);
    cancelledRequest.cancellationrequested = []() { return true; };
    const auto cancelled = service.Run(cancelledRequest);
    Require(cancelled.cancelled && !cancelled.authoritative,
        "cooperative cancellation must never become authoritative");
}

}  // namespace

int main()
{
    PassesAndIsDeterministic();
    SkipsHiddenAndReportsSpatialBlockers();
    FailsClosedForIdentityErrors();
    AppliesModeBudgetAndCancellationRules();
    std::cout << "stage14d08_r3_scene_preflight_tests: PASS\n";
    return 0;
}
