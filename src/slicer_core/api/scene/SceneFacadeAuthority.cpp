#include "slicer_core/api/scene/SceneFacadeAuthority.h"

#include "slicer_core/geometry/TransformedModelAdapter.h"
#include "slicer_core/scene/ModelInstance.h"
#include "slicer_core/scene/ModelTransform.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

// 文件职责：维护场景权威候选状态并执行整批原子准入；
// 边界：任何操作失败、取消或修订冲突都必须保留原状态。
namespace slicer_core::api::scene_facade_detail
{
namespace
{

ApiError MakeError(
    const std::string_view code,
    const std::string_view message,
    const std::string_view detail)
{
    return {std::string(code), std::string(message), std::string(detail)};
}

template <class T>
ApiResult<T> Failure(
    const std::string_view code,
    const std::string_view message,
    const std::string_view detail)
{
    return ApiResult<T>::Failure(MakeError(code, message, detail));
}

Matrix4d BuildWorldMatrix(
    const BoundingBox& sourceBounds,
    const ModelTransform& transform,
    const double landingOffsetZMm)
{
    using Matrix3 = std::array<double, 9>;
    const auto multiply = [](const Matrix3& left, const Matrix3& right)
    {
        Matrix3 result{};
        for (std::size_t row = 0U; row < 3U; ++row)
        {
            for (std::size_t column = 0U; column < 3U; ++column)
            {
                for (std::size_t index = 0U; index < 3U; ++index)
                {
                    result.at(row * 3U + column) +=
                        left.at(row * 3U + index)
                        * right.at(index * 3U + column);
                }
            }
        }
        return result;
    };

    const ModelTransform canonical = NormalizeModelTransform(transform);
    const double pivotX = (sourceBounds.min.x + sourceBounds.max.x) * 0.5;
    const double pivotY = (sourceBounds.min.y + sourceBounds.max.y) * 0.5;
    const double pivotZ = sourceBounds.min.z;
    const double radiansX = canonical.rotatexdeg
        * std::numbers::pi_v<double> / 180.0;
    const double radiansY = canonical.rotateydeg
        * std::numbers::pi_v<double> / 180.0;
    const double radiansZ = canonical.rotatezdeg
        * std::numbers::pi_v<double> / 180.0;
    const double cosineX = std::cos(radiansX);
    const double sineX = std::sin(radiansX);
    const double cosineY = std::cos(radiansY);
    const double sineY = std::sin(radiansY);
    const double cosineZ = std::cos(radiansZ);
    const double sineZ = std::sin(radiansZ);
    const Matrix3 mirrorScale{
        canonical.uniformscale * (canonical.mirrorx ? -1.0 : 1.0),
        0.0,
        0.0,
        0.0,
        canonical.uniformscale * (canonical.mirrory ? -1.0 : 1.0),
        0.0,
        0.0,
        0.0,
        canonical.uniformscale};
    const Matrix3 rotationX{
        1.0, 0.0, 0.0,
        0.0, cosineX, -sineX,
        0.0, sineX, cosineX};
    const Matrix3 rotationY{
        cosineY, 0.0, sineY,
        0.0, 1.0, 0.0,
        -sineY, 0.0, cosineY};
    const Matrix3 rotationZ{
        cosineZ, -sineZ, 0.0,
        sineZ, cosineZ, 0.0,
        0.0, 0.0, 1.0};
    const Matrix3 linear = multiply(
        rotationZ,
        multiply(rotationY, multiply(rotationX, mirrorScale)));

    Matrix4d matrix;
    matrix.values = {
        linear.at(0U), linear.at(1U), linear.at(2U),
        pivotX - linear.at(0U) * pivotX
            - linear.at(1U) * pivotY
            - linear.at(2U) * pivotZ
            + canonical.translatexmm,
        linear.at(3U), linear.at(4U), linear.at(5U),
        pivotY - linear.at(3U) * pivotX
            - linear.at(4U) * pivotY
            - linear.at(5U) * pivotZ
            + canonical.translateymm,
        linear.at(6U), linear.at(7U), linear.at(8U),
        pivotZ - linear.at(6U) * pivotX
            - linear.at(7U) * pivotY
            - linear.at(8U) * pivotZ
            + landingOffsetZMm,
        0.0, 0.0, 0.0, 1.0};
    return matrix;
}

SceneViewAdmissionStatus ToViewAdmission(
    const SceneInstanceAdmissionStatus status)
{
    switch (status)
    {
    case SceneInstanceAdmissionStatus::Admitted:
        return SceneViewAdmissionStatus::Admitted;
    case SceneInstanceAdmissionStatus::Blocked:
        return SceneViewAdmissionStatus::Blocked;
    case SceneInstanceAdmissionStatus::Unknown:
        return SceneViewAdmissionStatus::Unknown;
    }
    return SceneViewAdmissionStatus::Unknown;
}

ApiResult<SceneViewGeometry> BuildCollisionGeometry(
    const MultiModelScene& scene,
    const SceneModelInstance& sceneInstance,
    const SceneModel& model,
    const TransformedModelGeometry& transformed)
{
    const ModelTransformHashResult transformHash = ComputeModelTransformHash(
        sceneInstance.instance.transform,
        sceneInstance.instance.sourcetransformidentity,
        sceneInstance.instance.instanceid,
        sceneInstance.instance.modelid);
    if (!transformHash.IsValid())
    {
        return Failure<SceneViewGeometry>(
            "PM-SLICER-PROFILE-0031",
            transformHash.error->message,
            transformHash.error->field);
    }

    SceneViewGeometry geometry;
    geometry.sceneid = scene.sceneid;
    geometry.modelid = sceneInstance.instance.modelid;
    geometry.instanceid = sceneInstance.instance.instanceid;
    geometry.scenerevision = scene.scenerevision;
    geometry.transformrevision = sceneInstance.instance.transformrevision;
    geometry.sourcebboxmm = model.bbox_mm;
    geometry.effectivebboxmm = transformed.bboxmm;
    geometry.worldboundsmm = {
        {transformed.bboxmm.min.x, transformed.bboxmm.min.y},
        {transformed.bboxmm.max.x, transformed.bboxmm.max.y}};
    geometry.visible = sceneInstance.instance.visible;
    geometry.locked = sceneInstance.instance.locked;
    geometry.admissionstatus = ToViewAdmission(sceneInstance.admissionstatus);
    geometry.sourcetrianglecount = model.triangles.size();
    geometry.transformhash = transformHash.hash;
    geometry.triangles.reserve(transformed.triangles.size());
    for (const Triangle& triangle : transformed.triangles)
    {
        SceneViewTriangle projected;
        projected.a = {triangle.a.x, triangle.a.y};
        projected.b = {triangle.b.x, triangle.b.y};
        projected.c = {triangle.c.x, triangle.c.y};
        projected.zmm = {triangle.a.z, triangle.b.z, triangle.c.z};
        geometry.triangles.push_back(projected);
    }
    return ApiResult<SceneViewGeometry>::Success(std::move(geometry));
}

bool IsReportableCollisionError(const SceneCollisionErrorCode code)
{
    return code == SceneCollisionErrorCode::InstanceOutOfRange
        || code == SceneCollisionErrorCode::InstanceOverlapBlocked;
}

ApiResult<AuthorityState> EvaluateCandidate(SceneFacadeSeed seed)
{
    std::unordered_map<std::string, double> landingOffsetsMm;
    landingOffsetsMm.reserve(seed.scene.instances.size());
    SceneCollisionRequest collisionRequest;
    collisionRequest.sceneid = seed.scene.sceneid;
    collisionRequest.currentscenerevision = seed.scene.scenerevision;
    collisionRequest.expectedscenerevision = seed.scene.scenerevision;
    collisionRequest.purpose = seed.validation_purpose;
    collisionRequest.buildvolume = seed.scene.buildvolume;
    collisionRequest.contactepsilonmm = seed.contact_epsilon_mm;
    collisionRequest.items.reserve(seed.scene.instances.size());

    for (SceneModelInstance& sceneInstance : seed.scene.instances)
    {
        const auto source = seed.models_by_id.find(
            sceneInstance.instance.modelid);
        if (source == seed.models_by_id.end() || !source->second)
        {
            return Failure<AuthorityState>(
                "PM-SLICER-INPUT-0001",
                "scene source model is not available",
                sceneInstance.instance.modelid);
        }
        const TransformedModelResult transformed = AdaptTransformedModel(
            *source->second,
            sceneInstance.instance);
        if (!transformed.IsValid())
        {
            return Failure<AuthorityState>(
                "PM-SLICER-PROFILE-0031",
                transformed.error->message,
                transformed.error->field);
        }
        sceneInstance.instance.effectivebboxmm = transformed.geometry.bboxmm;
        landingOffsetsMm.insert_or_assign(
            sceneInstance.instance.instanceid,
            transformed.geometry.landingoffsetzmm);

        SceneCollisionItem item;
        item.instance = sceneInstance.instance;
        item.admissionstatus = sceneInstance.admissionstatus;
        if (sceneInstance.instance.visible)
        {
            ApiResult<SceneViewGeometry> geometry = BuildCollisionGeometry(
                seed.scene,
                sceneInstance,
                *source->second,
                transformed.geometry);
            if (!geometry.IsOk())
            {
                return Failure<AuthorityState>(
                    geometry.Error()->code,
                    geometry.Error()->message,
                    geometry.Error()->detail);
            }
            item.geometry = *geometry.Value();
        }
        collisionRequest.items.push_back(std::move(item));
    }

    const SceneCollisionResult evaluated = EvaluateSceneCollisionAdmission(
        collisionRequest);
    for (const SceneCollisionError& error : evaluated.errors)
    {
        if (!IsReportableCollisionError(error.code))
        {
            return Failure<AuthorityState>(
                error.code == SceneCollisionErrorCode::SceneRevisionStale
                    ? "PM-SLICER-LAYOUT-0022"
                    : "PM-SLICER-INTERNAL-0099",
                error.message,
                std::string(SceneCollisionErrorCodeName(error.code)));
        }
    }

    AuthorityState state;
    state.seed = std::move(seed);
    state.snapshot.scene_id = state.seed.scene_id;
    state.snapshot.scene_revision = state.seed.scene.scenerevision;
    state.snapshot.scene_hash = ComputeMultiModelSceneHash(state.seed.scene);
    state.snapshot.scene = state.seed.scene;
    state.snapshot.build_volume.width_mm =
        *state.seed.scene.buildvolume.widthmm;
    state.snapshot.build_volume.height_mm =
        *state.seed.scene.buildvolume.heightmm;
    state.snapshot.build_volume.z_limit_mm =
        state.seed.scene.buildvolume.zlimitmm;
    for (const SceneCollisionPair& pair : evaluated.collisionpairs)
    {
        state.collision_report.collisions.push_back(
            {pair.firstinstanceid, pair.secondinstanceid});
    }
    for (const SceneCollisionInstanceResult& result : evaluated.instances)
    {
        if (result.visible && result.boundsvalid && !result.inbounds)
        {
            state.collision_report.out_of_bounds_instances.push_back(
                result.instanceid);
        }
    }

    const std::set<std::string> outOfBounds(
        state.collision_report.out_of_bounds_instances.begin(),
        state.collision_report.out_of_bounds_instances.end());
    state.snapshot.instances.reserve(state.seed.scene.instances.size());
    for (const SceneModelInstance& sceneInstance : state.seed.scene.instances)
    {
        const auto apiModel = state.seed.api_model_ids.find(
            sceneInstance.instance.modelid);
        if (apiModel == state.seed.api_model_ids.end())
        {
            return Failure<AuthorityState>(
                "PM-SLICER-INTERNAL-0099",
                "scene model has no API model identity",
                sceneInstance.instance.modelid);
        }
        SceneInstanceState instanceState;
        instanceState.instance.instance_id = sceneInstance.instance.instanceid;
        instanceState.instance.model_id = apiModel->second;
        instanceState.instance.world_matrix = BuildWorldMatrix(
            sceneInstance.instance.sourcebboxmm,
            sceneInstance.instance.transform,
            landingOffsetsMm.at(sceneInstance.instance.instanceid));
        instanceState.effective_bounds_mm.min_mm = {
            sceneInstance.instance.effectivebboxmm.min.x,
            sceneInstance.instance.effectivebboxmm.min.y,
            sceneInstance.instance.effectivebboxmm.min.z};
        instanceState.effective_bounds_mm.max_mm = {
            sceneInstance.instance.effectivebboxmm.max.x,
            sceneInstance.instance.effectivebboxmm.max.y,
            sceneInstance.instance.effectivebboxmm.max.z};
        instanceState.out_of_bounds = outOfBounds.contains(
            sceneInstance.instance.instanceid);
        state.snapshot.instances.push_back(std::move(instanceState));
    }
    return ApiResult<AuthorityState>::Success(std::move(state));
}

}  // namespace

ApiResult<AuthorityState> BuildAuthorityState(SceneFacadeSeed seed) noexcept
{
    try
    {
        if (seed.scene_id == 0U)
        {
            return Failure<AuthorityState>(
                "PM-SLICER-PROFILE-0031",
                "scene API identity must be non-zero",
                "scene_id");
        }
        if (!std::isfinite(seed.contact_epsilon_mm)
            || seed.contact_epsilon_mm < 0.0)
        {
            return Failure<AuthorityState>(
                "PM-SLICER-PROFILE-0031",
                "scene contact epsilon must be finite and non-negative",
                "contact_epsilon_mm");
        }
        if (!seed.scene.buildvolume.widthmm.has_value()
            || !seed.scene.buildvolume.heightmm.has_value())
        {
            return Failure<AuthorityState>(
                "PM-SLICER-PROFILE-0031",
                "SceneFacade requires a resolved build volume",
                "buildvolume");
        }
        const SceneValidationResult validation = ValidateMultiModelScene(
            seed.scene,
            seed.validation_purpose);
        if (!validation.IsValid())
        {
            const SceneValidationError& error = validation.errors.front();
            return Failure<AuthorityState>(
                error.code == SceneValidationErrorCode::SceneRevisionStale
                    ? "PM-SLICER-LAYOUT-0022"
                    : "PM-SLICER-INTERNAL-0099",
                error.message,
                std::string(SceneValidationErrorCodeName(error.code)));
        }
        ApiResult<AuthorityState> evaluated = EvaluateCandidate(
            std::move(seed));
        if (!evaluated.IsOk())
        {
            return evaluated;
        }
        AuthorityState state = *evaluated.Value();
        state.warnings = validation.warnings;
        return ApiResult<AuthorityState>::Success(std::move(state));
    }
    catch (const std::exception& error)
    {
        return Failure<AuthorityState>(
            "PM-SLICER-INTERNAL-0099",
            "failed to initialize authoritative scene state",
            error.what());
    }
    catch (...)
    {
        return Failure<AuthorityState>(
            "PM-SLICER-INTERNAL-0099",
            "failed to initialize authoritative scene state",
            "unknown exception");
    }
}

}  // namespace slicer_core::api::scene_facade_detail
