#include "slicer_core/layout/SceneCollisionService.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr double kGeometryTolerance{1.0e-9};

struct EvaluationState
{
    bool boundsvalid{false};
    bool geometryvalid{false};
};

bool IsFinite(const SceneViewPoint& point)
{
    return std::isfinite(point.xmm) && std::isfinite(point.ymm);
}

bool IsFiniteBounds(const BoundingBox& bounds)
{
    return std::isfinite(bounds.min.x)
        && std::isfinite(bounds.min.y)
        && std::isfinite(bounds.max.x)
        && std::isfinite(bounds.max.y)
        && bounds.max.x > bounds.min.x
        && bounds.max.y > bounds.min.y;
}

bool IsFiniteBounds(const SceneViewBounds& bounds)
{
    return IsFinite(bounds.min)
        && IsFinite(bounds.max)
        && bounds.max.xmm > bounds.min.xmm
        && bounds.max.ymm > bounds.min.ymm;
}

bool NearlyEqual(const double left, const double right)
{
    const double scale = std::max(
        {1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right)
        <= kGeometryTolerance * scale;
}

SceneCollisionError MakeError(
    const SceneCollisionErrorCode code,
    const SceneCollisionRequest& request,
    const std::string& modelId,
    const std::string& instanceId,
    const std::string& otherInstanceId,
    const std::string_view field,
    const std::string_view message)
{
    SceneCollisionError error;
    error.code = code;
    error.sceneid = request.sceneid;
    error.modelid = modelId;
    error.instanceid = instanceId;
    error.otherinstanceid = otherInstanceId;
    error.field = field;
    error.message = message;
    return error;
}

void AppendGlobalError(
    SceneCollisionResult& result,
    SceneCollisionError error)
{
    result.errors.push_back(std::move(error));
}

void AppendInstanceError(
    SceneCollisionResult& result,
    const std::size_t index,
    SceneCollisionError error)
{
    result.instances.at(index).errors.push_back(error);
    result.errors.push_back(std::move(error));
}

std::optional<SceneCollisionError> ValidateBuildVolume(
    const SceneCollisionRequest& request)
{
    const SceneBuildVolume& volume = request.buildvolume;
    if (volume.source == BuildVolumeSource::Unresolved
        || !volume.widthmm.has_value()
        || !volume.heightmm.has_value()
        || volume.origin == BuildVolumeOrigin::Unresolved
        || volume.xdirection
            == BuildVolumeAxisDirection::Unresolved
        || volume.ydirection
            == BuildVolumeAxisDirection::Unresolved)
    {
        return MakeError(
            SceneCollisionErrorCode::BuildVolumeUndefined,
            request,
            {},
            {},
            {},
            "buildvolume",
            "scene admission requires an explicit build volume");
    }
    if (!std::isfinite(*volume.widthmm)
        || !std::isfinite(*volume.heightmm)
        || *volume.widthmm <= 0.0
        || *volume.heightmm <= 0.0
        || !std::isfinite(request.contactepsilonmm)
        || request.contactepsilonmm < 0.0)
    {
        return MakeError(
            SceneCollisionErrorCode::BuildVolumeInvalid,
            request,
            {},
            {},
            {},
            "buildvolume",
            "build volume dimensions and contact epsilon must be finite and non-negative");
    }
    if ((volume.source == BuildVolumeSource::Fixture
         && !volume.isfixture)
        || (volume.source == BuildVolumeSource::DeviceProfile
            && volume.isfixture))
    {
        return MakeError(
            SceneCollisionErrorCode::BuildVolumeInvalid,
            request,
            {},
            {},
            {},
            "buildvolume.source",
            "build volume provenance and fixture flag disagree");
    }
    if (request.purpose == SceneValidationPurpose::FunctionalFixture
        && (volume.source != BuildVolumeSource::Fixture
            || !volume.isfixture))
    {
        return MakeError(
            SceneCollisionErrorCode::BuildVolumeInvalid,
            request,
            {},
            {},
            {},
            "buildvolume.source",
            "functional admission requires explicit fixture provenance");
    }
    if (request.purpose == SceneValidationPurpose::Production
        && (volume.source != BuildVolumeSource::DeviceProfile
            || volume.isfixture))
    {
        return MakeError(
            SceneCollisionErrorCode::
                BuildVolumeFixtureNotProduction,
            request,
            {},
            {},
            {},
            "buildvolume.source",
            "fixture build volume cannot claim production admission");
    }
    return std::nullopt;
}

SceneViewBounds ResolveBuildVolumeBounds(
    const SceneBuildVolume& volume)
{
    if (volume.origin == BuildVolumeOrigin::Center)
    {
        return {
            {-*volume.widthmm / 2.0, -*volume.heightmm / 2.0},
            {*volume.widthmm / 2.0, *volume.heightmm / 2.0},
        };
    }
    return {
        {0.0, 0.0},
        {*volume.widthmm, *volume.heightmm},
    };
}

SceneViewBounds ToSceneViewBounds(const BoundingBox& bounds)
{
    return {
        {bounds.min.x, bounds.min.y},
        {bounds.max.x, bounds.max.y},
    };
}

bool BoundsMatch(
    const SceneViewBounds& left,
    const BoundingBox& right)
{
    return NearlyEqual(left.min.xmm, right.min.x)
        && NearlyEqual(left.min.ymm, right.min.y)
        && NearlyEqual(left.max.xmm, right.max.x)
        && NearlyEqual(left.max.ymm, right.max.y);
}

bool IsInBounds(
    const BoundingBox& instanceBounds,
    const SceneViewBounds& volumeBounds,
    const double epsilon)
{
    return instanceBounds.min.x
            >= volumeBounds.min.xmm - epsilon
        && instanceBounds.min.y
            >= volumeBounds.min.ymm - epsilon
        && instanceBounds.max.x
            <= volumeBounds.max.xmm + epsilon
        && instanceBounds.max.y
            <= volumeBounds.max.ymm + epsilon;
}

double Cross(
    const SceneViewPoint& first,
    const SceneViewPoint& second,
    const SceneViewPoint& third)
{
    return (second.xmm - first.xmm)
            * (third.ymm - first.ymm)
        - (second.ymm - first.ymm)
            * (third.xmm - first.xmm);
}

double TriangleSignedArea(const SceneViewTriangle& triangle)
{
    return Cross(triangle.a, triangle.b, triangle.c) / 2.0;
}

bool ValidateGeometry(
    const SceneCollisionRequest& request,
    const SceneCollisionItem& item,
    SceneCollisionError& error)
{
    if (!item.geometry.has_value())
    {
        error = MakeError(
            SceneCollisionErrorCode::ProjectionGeometryInvalid,
            request,
            item.instance.modelid,
            item.instance.instanceid,
            {},
            "geometry",
            "visible instance requires projected geometry");
        return false;
    }

    const SceneViewGeometry& geometry = *item.geometry;
    if (geometry.sceneid != request.sceneid
        || geometry.modelid != item.instance.modelid
        || geometry.instanceid != item.instance.instanceid
        || !geometry.visible)
    {
        error = MakeError(
            SceneCollisionErrorCode::ProjectionGeometryInvalid,
            request,
            item.instance.modelid,
            item.instance.instanceid,
            {},
            "geometry.identity",
            "projected geometry identity does not match the scene item");
        return false;
    }
    if (geometry.scenerevision != request.currentscenerevision
        || geometry.transformrevision
            != item.instance.transformrevision)
    {
        error = MakeError(
            SceneCollisionErrorCode::SceneRevisionStale,
            request,
            item.instance.modelid,
            item.instance.instanceid,
            {},
            "geometry.revision",
            "projected geometry revision is stale");
        return false;
    }
    const ModelTransformHashResult transformHash =
        ComputeModelTransformHash(
            item.instance.transform,
            item.instance.sourcetransformidentity,
            item.instance.instanceid,
            item.instance.modelid);
    if (!transformHash.IsValid()
        || geometry.transformhash.empty()
        || geometry.transformhash != transformHash.hash)
    {
        error = MakeError(
            SceneCollisionErrorCode::SceneRevisionStale,
            request,
            item.instance.modelid,
            item.instance.instanceid,
            {},
            "geometry.transformhash",
            "projected geometry transform hash is stale");
        return false;
    }
    if (!IsFiniteBounds(geometry.worldboundsmm)
        || !BoundsMatch(
            geometry.worldboundsmm,
            item.instance.effectivebboxmm)
        || geometry.triangles.empty())
    {
        error = MakeError(
            SceneCollisionErrorCode::ProjectionGeometryInvalid,
            request,
            item.instance.modelid,
            item.instance.instanceid,
            {},
            "geometry.bounds",
            "projected geometry bounds are invalid or stale");
        return false;
    }

    bool hasPositiveArea{false};
    for (const SceneViewTriangle& triangle : geometry.triangles)
    {
        if (!IsFinite(triangle.a)
            || !IsFinite(triangle.b)
            || !IsFinite(triangle.c))
        {
            error = MakeError(
                SceneCollisionErrorCode::
                    ProjectionGeometryInvalid,
                request,
                item.instance.modelid,
                item.instance.instanceid,
                {},
                "geometry.triangles",
                "projected geometry contains non-finite coordinates");
            return false;
        }
        hasPositiveArea = hasPositiveArea
            || std::abs(TriangleSignedArea(triangle))
                > kGeometryTolerance;
    }
    if (!hasPositiveArea)
    {
        error = MakeError(
            SceneCollisionErrorCode::ProjectionGeometryInvalid,
            request,
            item.instance.modelid,
            item.instance.instanceid,
            {},
            "geometry.triangles",
            "projected geometry has no positive-area triangle");
        return false;
    }
    return true;
}

bool IsAabbCandidate(
    const BoundingBox& left,
    const BoundingBox& right,
    const double epsilon)
{
    const double overlapX =
        std::min(left.max.x, right.max.x)
        - std::max(left.min.x, right.min.x);
    const double overlapY =
        std::min(left.max.y, right.max.y)
        - std::max(left.min.y, right.min.y);
    return overlapX > epsilon && overlapY > epsilon;
}

SceneViewPoint IntersectWithClipLine(
    const SceneViewPoint& start,
    const SceneViewPoint& end,
    const SceneViewPoint& clipStart,
    const SceneViewPoint& clipEnd)
{
    const SceneViewPoint segment{
        end.xmm - start.xmm,
        end.ymm - start.ymm};
    const SceneViewPoint clip{
        clipEnd.xmm - clipStart.xmm,
        clipEnd.ymm - clipStart.ymm};
    const double denominator =
        clip.xmm * segment.ymm - clip.ymm * segment.xmm;
    if (std::abs(denominator) <= kGeometryTolerance)
    {
        return end;
    }
    const double numerator =
        clip.xmm * (clipStart.ymm - start.ymm)
        - clip.ymm * (clipStart.xmm - start.xmm);
    const double factor = numerator / denominator;
    return {
        start.xmm + factor * segment.xmm,
        start.ymm + factor * segment.ymm,
    };
}

bool IsInsideClipEdge(
    const SceneViewPoint& point,
    const SceneViewPoint& edgeStart,
    const SceneViewPoint& edgeEnd,
    const double orientation)
{
    return orientation
            * Cross(edgeStart, edgeEnd, point)
        >= -kGeometryTolerance;
}

std::vector<SceneViewPoint> ClipPolygon(
    std::vector<SceneViewPoint> polygon,
    const SceneViewTriangle& clipTriangle)
{
    const double orientation =
        TriangleSignedArea(clipTriangle) >= 0.0 ? 1.0 : -1.0;
    const std::array<SceneViewPoint, 3U> clipPoints{
        clipTriangle.a,
        clipTriangle.b,
        clipTriangle.c,
    };

    for (std::size_t edgeIndex = 0U;
         edgeIndex < clipPoints.size();
         ++edgeIndex)
    {
        if (polygon.empty())
        {
            break;
        }
        const SceneViewPoint clipStart = clipPoints[edgeIndex];
        const SceneViewPoint clipEnd =
            clipPoints[(edgeIndex + 1U) % clipPoints.size()];
        std::vector<SceneViewPoint> output;
        output.reserve(polygon.size() + 1U);
        SceneViewPoint previous = polygon.back();
        bool previousInside = IsInsideClipEdge(
            previous,
            clipStart,
            clipEnd,
            orientation);
        for (const SceneViewPoint& current : polygon)
        {
            const bool currentInside = IsInsideClipEdge(
                current,
                clipStart,
                clipEnd,
                orientation);
            if (currentInside != previousInside)
            {
                output.push_back(
                    IntersectWithClipLine(
                        previous,
                        current,
                        clipStart,
                        clipEnd));
            }
            if (currentInside)
            {
                output.push_back(current);
            }
            previous = current;
            previousInside = currentInside;
        }
        polygon = std::move(output);
    }
    return polygon;
}

double PolygonArea(const std::vector<SceneViewPoint>& polygon)
{
    if (polygon.size() < 3U)
    {
        return 0.0;
    }
    double twiceArea{0.0};
    for (std::size_t index = 0U;
         index < polygon.size();
         ++index)
    {
        const SceneViewPoint& current = polygon[index];
        const SceneViewPoint& next =
            polygon[(index + 1U) % polygon.size()];
        twiceArea +=
            current.xmm * next.ymm - current.ymm * next.xmm;
    }
    return std::abs(twiceArea) / 2.0;
}

bool TrianglesOverlapWithPositiveArea(
    const SceneViewTriangle& left,
    const SceneViewTriangle& right,
    const double contactEpsilon)
{
    if (std::abs(TriangleSignedArea(left))
            <= kGeometryTolerance
        || std::abs(TriangleSignedArea(right))
            <= kGeometryTolerance)
    {
        return false;
    }
    const std::vector<SceneViewPoint> intersection =
        ClipPolygon({left.a, left.b, left.c}, right);
    const double minimumArea = std::max(
        kGeometryTolerance,
        contactEpsilon * contactEpsilon);
    return PolygonArea(intersection) > minimumArea;
}

bool GeometriesOverlap(
    const SceneViewGeometry& left,
    const SceneViewGeometry& right,
    const double contactEpsilon)
{
    for (const SceneViewTriangle& leftTriangle : left.triangles)
    {
        for (const SceneViewTriangle& rightTriangle : right.triangles)
        {
            if (TrianglesOverlapWithPositiveArea(
                    leftTriangle,
                    rightTriangle,
                    contactEpsilon))
            {
                return true;
            }
        }
    }
    return false;
}

void ExtendSceneBounds(
    std::optional<SceneViewBounds>& sceneBounds,
    const BoundingBox& instanceBounds)
{
    const SceneViewBounds bounds =
        ToSceneViewBounds(instanceBounds);
    if (!sceneBounds.has_value())
    {
        sceneBounds = bounds;
        return;
    }
    sceneBounds->min.xmm =
        std::min(sceneBounds->min.xmm, bounds.min.xmm);
    sceneBounds->min.ymm =
        std::min(sceneBounds->min.ymm, bounds.min.ymm);
    sceneBounds->max.xmm =
        std::max(sceneBounds->max.xmm, bounds.max.xmm);
    sceneBounds->max.ymm =
        std::max(sceneBounds->max.ymm, bounds.max.ymm);
}

}  // namespace

bool SceneCollisionResult::IsValid() const
{
    return errors.empty();
}

std::string_view SceneCollisionErrorCodeName(
    const SceneCollisionErrorCode code)
{
    switch (code)
    {
    case SceneCollisionErrorCode::None:
        return "NONE";
    case SceneCollisionErrorCode::BuildVolumeUndefined:
        return "SCENE_BUILD_VOLUME_UNDEFINED";
    case SceneCollisionErrorCode::BuildVolumeInvalid:
        return "SCENE_BUILD_VOLUME_INVALID";
    case SceneCollisionErrorCode::BuildVolumeFixtureNotProduction:
        return "SCENE_BUILD_VOLUME_FIXTURE_NOT_PRODUCTION";
    case SceneCollisionErrorCode::InstanceBoundsInvalid:
        return "SCENE_INSTANCE_BOUNDS_INVALID";
    case SceneCollisionErrorCode::InstanceOutOfRange:
        return "SCENE_INSTANCE_OUT_OF_RANGE";
    case SceneCollisionErrorCode::InstanceAdmissionBlocked:
        return "SCENE_INSTANCE_ADMISSION_BLOCKED";
    case SceneCollisionErrorCode::ProjectionGeometryInvalid:
        return "SCENE_PROJECTION_GEOMETRY_INVALID";
    case SceneCollisionErrorCode::InstanceOverlapBlocked:
        return "SCENE_INSTANCE_OVERLAP_BLOCKED";
    case SceneCollisionErrorCode::SceneRevisionStale:
        return "SCENE_REVISION_STALE";
    }
    return "SCENE_COLLISION_UNKNOWN";
}

SceneCollisionResult EvaluateSceneCollisionAdmission(
    const SceneCollisionRequest& request)
{
    SceneCollisionResult result;
    result.sceneid = request.sceneid;
    result.sourcescenerevision = request.currentscenerevision;
    result.purpose = request.purpose;
    result.buildvolume = request.buildvolume;
    result.contactepsilonmm = request.contactepsilonmm;
    result.statistics.totalinstancecount = request.items.size();
    result.instances.reserve(request.items.size());

    for (const SceneCollisionItem& item : request.items)
    {
        SceneCollisionInstanceResult instanceResult;
        instanceResult.modelid = item.instance.modelid;
        instanceResult.instanceid = item.instance.instanceid;
        instanceResult.transformrevision =
            item.instance.transformrevision;
        if (item.geometry.has_value())
        {
            instanceResult.transformhash =
                item.geometry->transformhash;
        }
        instanceResult.visible = item.instance.visible;
        instanceResult.skippedhidden = !item.instance.visible;
        instanceResult.admissionstatus = item.admissionstatus;
        result.instances.push_back(std::move(instanceResult));
        if (item.instance.visible)
        {
            ++result.statistics.visibleinstancecount;
        }
        else
        {
            ++result.statistics.hiddeninstancecount;
        }
    }

    if (request.currentscenerevision
        != request.expectedscenerevision)
    {
        AppendGlobalError(
            result,
            MakeError(
                SceneCollisionErrorCode::SceneRevisionStale,
                request,
                {},
                {},
                {},
                "scenerevision",
                "scene revision changed before collision admission"));
    }

    const std::optional<SceneCollisionError> volumeError =
        ValidateBuildVolume(request);
    if (volumeError.has_value())
    {
        AppendGlobalError(result, *volumeError);
    }
    if (!result.errors.empty())
    {
        return result;
    }

    const SceneViewBounds volumeBounds =
        ResolveBuildVolumeBounds(request.buildvolume);
    std::vector<EvaluationState> states(request.items.size());

    for (std::size_t index = 0U;
         index < request.items.size();
         ++index)
    {
        const SceneCollisionItem& item = request.items[index];
        if (!item.instance.visible)
        {
            continue;
        }

        if (item.admissionstatus
            != SceneInstanceAdmissionStatus::Admitted)
        {
            AppendInstanceError(
                result,
                index,
                MakeError(
                    SceneCollisionErrorCode::
                        InstanceAdmissionBlocked,
                    request,
                    item.instance.modelid,
                    item.instance.instanceid,
                    {},
                    "admissionstatus",
                    "visible instance admission is not admitted"));
        }

        if (!IsFiniteBounds(item.instance.effectivebboxmm))
        {
            AppendInstanceError(
                result,
                index,
                MakeError(
                    SceneCollisionErrorCode::InstanceBoundsInvalid,
                    request,
                    item.instance.modelid,
                    item.instance.instanceid,
                    {},
                    "effectivebboxmm",
                    "visible instance effective XY bounds are invalid"));
            continue;
        }
        states[index].boundsvalid = true;
        result.instances[index].boundsvalid = true;
        ExtendSceneBounds(
            result.sceneboundsmm,
            item.instance.effectivebboxmm);

        const bool inBounds = IsInBounds(
            item.instance.effectivebboxmm,
            volumeBounds,
            request.contactepsilonmm);
        result.instances[index].inbounds = inBounds;
        if (!inBounds)
        {
            AppendInstanceError(
                result,
                index,
                MakeError(
                    SceneCollisionErrorCode::InstanceOutOfRange,
                    request,
                    item.instance.modelid,
                    item.instance.instanceid,
                    {},
                    "effectivebboxmm",
                    "visible instance exceeds the explicit build volume"));
        }

        SceneCollisionError geometryError;
        states[index].geometryvalid =
            ValidateGeometry(request, item, geometryError);
        if (!states[index].geometryvalid)
        {
            AppendInstanceError(
                result,
                index,
                std::move(geometryError));
        }
    }

    for (std::size_t left = 0U;
         left < request.items.size();
         ++left)
    {
        if (!request.items[left].instance.visible
            || !states[left].boundsvalid)
        {
            continue;
        }
        for (std::size_t right = left + 1U;
             right < request.items.size();
             ++right)
        {
            if (!request.items[right].instance.visible
                || !states[right].boundsvalid
                || !IsAabbCandidate(
                    request.items[left].instance.effectivebboxmm,
                    request.items[right].instance.effectivebboxmm,
                    request.contactepsilonmm))
            {
                continue;
            }
            ++result.statistics.aabbcandidatepaircount;
            if (!states[left].geometryvalid
                || !states[right].geometryvalid)
            {
                continue;
            }
            ++result.statistics.exacttestedpaircount;
            if (!GeometriesOverlap(
                    *request.items[left].geometry,
                    *request.items[right].geometry,
                    request.contactepsilonmm))
            {
                continue;
            }

            ++result.statistics.collisionpaircount;
            const std::string& leftId =
                request.items[left].instance.instanceid;
            const std::string& rightId =
                request.items[right].instance.instanceid;
            result.collisionpairs.push_back({leftId, rightId});
            result.instances[left].collisionids.push_back(rightId);
            result.instances[right].collisionids.push_back(leftId);

            SceneCollisionError leftError = MakeError(
                SceneCollisionErrorCode::InstanceOverlapBlocked,
                request,
                request.items[left].instance.modelid,
                leftId,
                rightId,
                "geometry.triangles",
                "visible instance projections overlap with positive area");
            result.instances[left].errors.push_back(leftError);
            result.errors.push_back(leftError);
            SceneCollisionError rightError = MakeError(
                SceneCollisionErrorCode::InstanceOverlapBlocked,
                request,
                request.items[right].instance.modelid,
                rightId,
                leftId,
                "geometry.triangles",
                "visible instance projections overlap with positive area");
            result.instances[right].errors.push_back(rightError);
            result.errors.push_back(std::move(rightError));
        }
    }

    result.scenestatus = result.errors.empty()
        ? SceneCollisionStatus::Passed
        : SceneCollisionStatus::Blocked;
    result.functionalallowed =
        result.errors.empty()
        && request.purpose
            == SceneValidationPurpose::FunctionalFixture;
    result.productionallowed =
        result.errors.empty()
        && request.purpose == SceneValidationPurpose::Production;
    return result;
}

}  // namespace slicer_core
