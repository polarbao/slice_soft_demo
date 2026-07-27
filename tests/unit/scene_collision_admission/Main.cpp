#include "slicer_core/layout/SceneCollisionService.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

slicer_core::SceneBuildVolume MakeFixtureVolume(
    const slicer_core::BuildVolumeOrigin origin)
{
    slicer_core::SceneBuildVolume volume;
    volume.source = slicer_core::BuildVolumeSource::Fixture;
    volume.widthmm = 100.0;
    volume.heightmm = 80.0;
    volume.origin = origin;
    volume.xdirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    volume.ydirection =
        slicer_core::BuildVolumeAxisDirection::Positive;
    volume.isfixture = true;
    return volume;
}

slicer_core::SceneCollisionItem MakeItem(
    const int index,
    const slicer_core::SceneViewBounds& bounds,
    const std::vector<slicer_core::SceneViewTriangle>& triangles)
{
    slicer_core::SceneCollisionItem item;
    item.instance.instanceid = "instance-" + std::to_string(index);
    item.instance.modelid = "model-" + std::to_string(index);
    item.instance.sourcetransformidentity =
        "source-" + std::to_string(index);
    item.instance.transformrevision =
        static_cast<std::uint64_t>(index + 3);
    item.instance.sourcebboxmm = {
        {bounds.min.xmm, bounds.min.ymm, 0.0},
        {bounds.max.xmm, bounds.max.ymm, 1.0}};
    item.instance.effectivebboxmm = item.instance.sourcebboxmm;
    item.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;

    slicer_core::SceneViewGeometry geometry;
    geometry.sceneid = "scene-test";
    geometry.modelid = item.instance.modelid;
    geometry.instanceid = item.instance.instanceid;
    geometry.scenerevision = 9U;
    geometry.transformrevision = item.instance.transformrevision;
    geometry.triangles = triangles;
    geometry.worldboundsmm = bounds;
    geometry.effectivebboxmm = item.instance.effectivebboxmm;
    geometry.visible = true;
    geometry.admissionstatus =
        slicer_core::SceneViewAdmissionStatus::Admitted;
    const slicer_core::ModelTransformHashResult transformHash =
        slicer_core::ComputeModelTransformHash(
            item.instance.transform,
            item.instance.sourcetransformidentity,
            item.instance.instanceid,
            item.instance.modelid);
    Require(transformHash.IsValid(), "fixture transform hash should be valid");
    geometry.transformhash = transformHash.hash;
    item.geometry = std::move(geometry);
    return item;
}

slicer_core::SceneCollisionRequest MakeRequest()
{
    slicer_core::SceneCollisionRequest request;
    request.sceneid = "scene-test";
    request.currentscenerevision = 9U;
    request.expectedscenerevision = 9U;
    request.purpose =
        slicer_core::SceneValidationPurpose::FunctionalFixture;
    request.buildvolume = MakeFixtureVolume(
        slicer_core::BuildVolumeOrigin::LowerLeft);
    return request;
}

std::vector<slicer_core::SceneViewTriangle> MakeRectangle(
    const double minX,
    const double minY,
    const double maxX,
    const double maxY)
{
    return {
        {{minX, minY}, {maxX, minY}, {maxX, maxY}},
        {{minX, minY}, {maxX, maxY}, {minX, maxY}},
    };
}

bool HasError(
    const slicer_core::SceneCollisionResult& result,
    const slicer_core::SceneCollisionErrorCode code,
    const std::string& instanceId = {})
{
    for (const slicer_core::SceneCollisionError& error : result.errors)
    {
        if (error.code == code
            && (instanceId.empty() || error.instanceid == instanceId))
        {
            return true;
        }
    }
    return false;
}

void AcceptsExplicitLowerLeftAndCenterFixtures()
{
    slicer_core::SceneCollisionRequest lowerLeft = MakeRequest();
    lowerLeft.items.push_back(
        MakeItem(
            0,
            {{10.0, 10.0}, {20.0, 20.0}},
            MakeRectangle(10.0, 10.0, 20.0, 20.0)));
    const slicer_core::SceneCollisionResult lowerLeftResult =
        slicer_core::EvaluateSceneCollisionAdmission(lowerLeft);
    Require(lowerLeftResult.IsValid(), "lower-left fixture should pass");
    Require(
        lowerLeftResult.functionalallowed
            && !lowerLeftResult.productionallowed,
        "fixture pass must not become a production pass");
    Require(
        lowerLeftResult.instances.size() == 1U
            && lowerLeftResult.instances.front().inbounds
            && lowerLeftResult.instances.front().transformrevision
                == lowerLeft.items.front().instance.transformrevision
            && lowerLeftResult.instances.front().transformhash
                == lowerLeft.items.front().geometry->transformhash,
        "in-volume instance should be reported in bounds");

    slicer_core::SceneCollisionRequest center = MakeRequest();
    center.buildvolume = MakeFixtureVolume(
        slicer_core::BuildVolumeOrigin::Center);
    center.items.push_back(
        MakeItem(
            0,
            {{-40.0, -30.0}, {-30.0, -20.0}},
            MakeRectangle(-40.0, -30.0, -30.0, -20.0)));
    const slicer_core::SceneCollisionResult centerResult =
        slicer_core::EvaluateSceneCollisionAdmission(center);
    Require(centerResult.IsValid(), "center fixture should pass");
}

void RejectsUndefinedInvalidAndFixtureProductionVolumes()
{
    slicer_core::SceneCollisionRequest undefined = MakeRequest();
    undefined.buildvolume = {};
    const slicer_core::SceneCollisionResult undefinedResult =
        slicer_core::EvaluateSceneCollisionAdmission(undefined);
    Require(
        HasError(
            undefinedResult,
            slicer_core::SceneCollisionErrorCode::
                BuildVolumeUndefined),
        "unresolved volume must fail closed");

    slicer_core::SceneCollisionRequest invalid = MakeRequest();
    invalid.buildvolume.widthmm = -1.0;
    const slicer_core::SceneCollisionResult invalidResult =
        slicer_core::EvaluateSceneCollisionAdmission(invalid);
    Require(
        HasError(
            invalidResult,
            slicer_core::SceneCollisionErrorCode::
                BuildVolumeInvalid),
        "non-positive volume must be invalid");

    slicer_core::SceneCollisionRequest production = MakeRequest();
    production.purpose =
        slicer_core::SceneValidationPurpose::Production;
    const slicer_core::SceneCollisionResult productionResult =
        slicer_core::EvaluateSceneCollisionAdmission(production);
    Require(
        HasError(
            productionResult,
            slicer_core::SceneCollisionErrorCode::
                BuildVolumeFixtureNotProduction),
        "fixture volume must not pass production");
}

void ReportsEveryBoundaryFailureWithInstanceIdentity()
{
    const std::vector<slicer_core::SceneViewBounds> outsideBounds{
        {{-0.1, 10.0}, {5.0, 15.0}},
        {{95.0, 10.0}, {100.1, 15.0}},
        {{10.0, -0.1}, {15.0, 5.0}},
        {{10.0, 75.0}, {15.0, 80.1}},
    };
    for (std::size_t index = 0U;
         index < outsideBounds.size();
         ++index)
    {
        slicer_core::SceneCollisionRequest request = MakeRequest();
        const slicer_core::SceneViewBounds& bounds =
            outsideBounds[index];
        request.items.push_back(
            MakeItem(
                static_cast<int>(index),
                bounds,
                MakeRectangle(
                    bounds.min.xmm,
                    bounds.min.ymm,
                    bounds.max.xmm,
                    bounds.max.ymm)));
        const slicer_core::SceneCollisionResult result =
            slicer_core::EvaluateSceneCollisionAdmission(request);
        Require(
            HasError(
                result,
                slicer_core::SceneCollisionErrorCode::
                    InstanceOutOfRange,
                "instance-" + std::to_string(index)),
            "each boundary failure must identify its instance");
    }
}

void UsesAabbFilteringAndAllowsBoundaryContact()
{
    slicer_core::SceneCollisionRequest separated = MakeRequest();
    separated.items.push_back(
        MakeItem(
            0,
            {{0.0, 0.0}, {1.0, 1.0}},
            MakeRectangle(0.0, 0.0, 1.0, 1.0)));
    separated.items.push_back(
        MakeItem(
            1,
            {{2.0, 0.0}, {3.0, 1.0}},
            MakeRectangle(2.0, 0.0, 3.0, 1.0)));
    const slicer_core::SceneCollisionResult separatedResult =
        slicer_core::EvaluateSceneCollisionAdmission(separated);
    Require(separatedResult.IsValid(), "separated items should pass");
    Require(
        separatedResult.statistics.aabbcandidatepaircount == 0U
            && separatedResult.statistics.exacttestedpaircount == 0U,
        "separated AABBs must skip exact testing");

    slicer_core::SceneCollisionRequest touching = MakeRequest();
    touching.items.push_back(
        MakeItem(
            0,
            {{0.0, 0.0}, {1.0, 1.0}},
            MakeRectangle(0.0, 0.0, 1.0, 1.0)));
    touching.items.push_back(
        MakeItem(
            1,
            {{1.0, 0.0}, {2.0, 1.0}},
            MakeRectangle(1.0, 0.0, 2.0, 1.0)));
    const slicer_core::SceneCollisionResult touchingResult =
        slicer_core::EvaluateSceneCollisionAdmission(touching);
    Require(touchingResult.IsValid(), "boundary contact should pass");
    Require(
        touchingResult.statistics.aabbcandidatepaircount == 0U,
        "boundary contact should not become a collision candidate");

    slicer_core::SceneCollisionRequest precise = MakeRequest();
    precise.items.push_back(
        MakeItem(
            0,
            {{0.0, 0.0}, {2.0, 2.0}},
            {{{0.0, 0.0}, {2.0, 0.0}, {0.0, 2.0}}}));
    precise.items.push_back(
        MakeItem(
            1,
            {{0.0, 0.0}, {2.0, 2.0}},
            {{{2.0, 2.0}, {2.0, 1.2}, {1.2, 2.0}}}));
    const slicer_core::SceneCollisionResult preciseResult =
        slicer_core::EvaluateSceneCollisionAdmission(precise);
    Require(
        preciseResult.IsValid(),
        "overlapping AABBs with separated triangles should pass");
    Require(
        preciseResult.statistics.aabbcandidatepaircount == 1U
            && preciseResult.statistics.exacttestedpaircount == 1U
            && preciseResult.statistics.collisionpaircount == 0U,
        "precise phase should reject an AABB false positive");
}

void RejectsCrossingAndContainedProjectedTriangles()
{
    slicer_core::SceneCollisionRequest crossing = MakeRequest();
    crossing.items.push_back(
        MakeItem(
            0,
            {{0.0, 0.0}, {3.0, 3.0}},
            {{{0.0, 0.0}, {3.0, 0.0}, {1.5, 3.0}}}));
    crossing.items.push_back(
        MakeItem(
            1,
            {{0.0, 0.0}, {3.0, 3.0}},
            {{{0.0, 2.0}, {3.0, 2.0}, {1.5, -1.0}}}));
    const slicer_core::SceneCollisionResult crossingResult =
        slicer_core::EvaluateSceneCollisionAdmission(crossing);
    Require(
        HasError(
            crossingResult,
            slicer_core::SceneCollisionErrorCode::
                InstanceOverlapBlocked),
        "crossing projected triangles must fail");

    slicer_core::SceneCollisionRequest contained = MakeRequest();
    contained.items.push_back(
        MakeItem(
            0,
            {{0.0, 0.0}, {10.0, 10.0}},
            {{{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}}}));
    contained.items.push_back(
        MakeItem(
            1,
            {{1.0, 1.0}, {2.0, 2.0}},
            {{{1.0, 1.0}, {2.0, 1.0}, {1.0, 2.0}}}));
    const slicer_core::SceneCollisionResult containedResult =
        slicer_core::EvaluateSceneCollisionAdmission(contained);
    Require(
        HasError(
            containedResult,
            slicer_core::SceneCollisionErrorCode::
                InstanceOverlapBlocked),
        "contained projected triangle must fail");
}

void RejectsAdmissionGeometryAndRevisionFailures()
{
    slicer_core::SceneCollisionRequest blocked = MakeRequest();
    slicer_core::SceneCollisionItem blockedItem =
        MakeItem(
            0,
            {{1.0, 1.0}, {2.0, 2.0}},
            MakeRectangle(1.0, 1.0, 2.0, 2.0));
    blockedItem.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Blocked;
    blocked.items.push_back(blockedItem);
    Require(
        HasError(
            slicer_core::EvaluateSceneCollisionAdmission(blocked),
            slicer_core::SceneCollisionErrorCode::
                InstanceAdmissionBlocked,
            "instance-0"),
        "blocked admission must fail closed");

    slicer_core::SceneCollisionRequest unknown = MakeRequest();
    slicer_core::SceneCollisionItem unknownItem = blockedItem;
    unknownItem.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Unknown;
    unknown.items.push_back(unknownItem);
    Require(
        HasError(
            slicer_core::EvaluateSceneCollisionAdmission(unknown),
            slicer_core::SceneCollisionErrorCode::
                InstanceAdmissionBlocked),
        "unknown admission must fail closed");

    slicer_core::SceneCollisionRequest missing = MakeRequest();
    slicer_core::SceneCollisionItem missingItem = blockedItem;
    missingItem.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;
    missingItem.geometry.reset();
    missing.items.push_back(missingItem);
    Require(
        HasError(
            slicer_core::EvaluateSceneCollisionAdmission(missing),
            slicer_core::SceneCollisionErrorCode::
                ProjectionGeometryInvalid),
        "missing geometry must fail closed");

    slicer_core::SceneCollisionRequest stale = MakeRequest();
    slicer_core::SceneCollisionItem staleItem = missingItem;
    staleItem.geometry = blockedItem.geometry;
    staleItem.geometry->scenerevision = 8U;
    stale.items.push_back(staleItem);
    Require(
        HasError(
            slicer_core::EvaluateSceneCollisionAdmission(stale),
            slicer_core::SceneCollisionErrorCode::SceneRevisionStale),
        "stale geometry must fail closed");

    slicer_core::SceneCollisionRequest staleRequest = MakeRequest();
    staleRequest.expectedscenerevision = 8U;
    Require(
        HasError(
            slicer_core::EvaluateSceneCollisionAdmission(staleRequest),
            slicer_core::SceneCollisionErrorCode::SceneRevisionStale),
        "stale request revision must fail closed");

    slicer_core::SceneCollisionRequest staleTransform = MakeRequest();
    slicer_core::SceneCollisionItem staleTransformItem = blockedItem;
    staleTransformItem.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;
    staleTransformItem.geometry->transformrevision += 1U;
    staleTransform.items.push_back(staleTransformItem);
    Require(
        HasError(
            slicer_core::EvaluateSceneCollisionAdmission(
                staleTransform),
            slicer_core::SceneCollisionErrorCode::SceneRevisionStale,
            "instance-0"),
        "stale transform revision must fail closed");

    slicer_core::SceneCollisionRequest staleTransformHash = MakeRequest();
    slicer_core::SceneCollisionItem staleTransformHashItem = blockedItem;
    staleTransformHashItem.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;
    staleTransformHashItem.geometry->transformhash = "stale-transform";
    staleTransformHash.items.push_back(staleTransformHashItem);
    Require(
        HasError(
            slicer_core::EvaluateSceneCollisionAdmission(
                staleTransformHash),
            slicer_core::SceneCollisionErrorCode::SceneRevisionStale,
            "instance-0"),
        "stale transform hash must fail closed");

    slicer_core::SceneCollisionRequest wrongIdentity = MakeRequest();
    slicer_core::SceneCollisionItem wrongIdentityItem = blockedItem;
    wrongIdentityItem.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;
    wrongIdentityItem.geometry->instanceid = "different-instance";
    wrongIdentity.items.push_back(wrongIdentityItem);
    Require(
        HasError(
            slicer_core::EvaluateSceneCollisionAdmission(
                wrongIdentity),
            slicer_core::SceneCollisionErrorCode::
                ProjectionGeometryInvalid,
            "instance-0"),
        "geometry identity mismatch must fail closed");

    slicer_core::SceneCollisionRequest invalidBounds = MakeRequest();
    slicer_core::SceneCollisionItem invalidBoundsItem = blockedItem;
    invalidBoundsItem.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Admitted;
    invalidBoundsItem.instance.effectivebboxmm.max.x =
        invalidBoundsItem.instance.effectivebboxmm.min.x;
    invalidBounds.items.push_back(invalidBoundsItem);
    Require(
        HasError(
            slicer_core::EvaluateSceneCollisionAdmission(
                invalidBounds),
            slicer_core::SceneCollisionErrorCode::
                InstanceBoundsInvalid,
            "instance-0"),
        "invalid effective bounds must fail closed");
}

void SkipsHiddenOverlapAndProducesDeterministicResults()
{
    slicer_core::SceneCollisionRequest request = MakeRequest();
    request.items.push_back(
        MakeItem(
            0,
            {{5.0, 5.0}, {10.0, 10.0}},
            MakeRectangle(5.0, 5.0, 10.0, 10.0)));
    slicer_core::SceneCollisionItem hidden =
        MakeItem(
            1,
            {{5.0, 5.0}, {10.0, 10.0}},
            MakeRectangle(5.0, 5.0, 10.0, 10.0));
    hidden.instance.visible = false;
    hidden.admissionstatus =
        slicer_core::SceneInstanceAdmissionStatus::Blocked;
    hidden.geometry.reset();
    request.items.push_back(hidden);

    const slicer_core::SceneCollisionResult first =
        slicer_core::EvaluateSceneCollisionAdmission(request);
    const slicer_core::SceneCollisionResult second =
        slicer_core::EvaluateSceneCollisionAdmission(request);
    Require(first.IsValid() && second.IsValid(), "hidden overlap should pass");
    Require(
        first.instances.at(1U).skippedhidden
            && first.statistics.visibleinstancecount == 1U,
        "hidden instance should be retained but skipped");
    Require(
        first.statistics.aabbcandidatepaircount
            == second.statistics.aabbcandidatepaircount
            && first.errors.size() == second.errors.size()
            && first.instances.size() == second.instances.size(),
        "identical input should produce deterministic results");
    Require(
        request.items.at(0U).instance.effectivebboxmm.min.x == 5.0
            && !request.items.at(1U).instance.visible,
        "evaluation must not mutate its input");
}

void ExposesStableErrorNames()
{
    Require(
        slicer_core::SceneCollisionErrorCodeName(
            slicer_core::SceneCollisionErrorCode::
                BuildVolumeUndefined)
            == "SCENE_BUILD_VOLUME_UNDEFINED",
        "build-volume error name must remain stable");
    Require(
        slicer_core::SceneCollisionErrorCodeName(
            slicer_core::SceneCollisionErrorCode::
                InstanceOverlapBlocked)
            == "SCENE_INSTANCE_OVERLAP_BLOCKED",
        "overlap error name must remain stable");
}

}  // namespace

int main()
{
    AcceptsExplicitLowerLeftAndCenterFixtures();
    RejectsUndefinedInvalidAndFixtureProductionVolumes();
    ReportsEveryBoundaryFailureWithInstanceIdentity();
    UsesAabbFilteringAndAllowsBoundaryContact();
    RejectsCrossingAndContainedProjectedTriangles();
    RejectsAdmissionGeometryAndRevisionFailures();
    SkipsHiddenOverlapAndProducesDeterministicResults();
    ExposesStableErrorNames();
    std::cout << "scene_collision_admission_unit_tests passed\n";
    return 0;
}
