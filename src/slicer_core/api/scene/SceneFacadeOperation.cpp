#include "slicer_core/api/scene/SceneFacadeAuthority.h"

#include "slicer_core/scene/ModelInstance.h"
#include "slicer_core/scene/ModelTransform.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace slicer_core::api::scene_facade_detail
{
namespace
{

template <class T>
ApiResult<T> Failure(
    const std::string_view code,
    const std::string_view message,
    const std::string_view detail)
{
    return ApiResult<T>::Failure(
        {std::string(code), std::string(message), std::string(detail)});
}

std::string EscapeJson(const std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size() + 2U);
    for (const char character : value)
    {
        switch (character)
        {
        case '"':
            escaped.append("\\\"");
            break;
        case '\\':
            escaped.append("\\\\");
            break;
        case '\b':
            escaped.append("\\b");
            break;
        case '\f':
            escaped.append("\\f");
            break;
        case '\n':
            escaped.append("\\n");
            break;
        case '\r':
            escaped.append("\\r");
            break;
        case '\t':
            escaped.append("\\t");
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    return escaped;
}

std::string FormatNumber(const double value)
{
    char buffer[128]{};
    const auto [end, error] = std::to_chars(
        std::begin(buffer),
        std::end(buffer),
        value,
        std::chars_format::general);
    if (error != std::errc{})
    {
        throw std::runtime_error("failed to canonicalize operation number");
    }
    return {buffer, end};
}

std::string OperationTypeName(const SceneOperationType type)
{
    switch (type)
    {
    case SceneOperationType::AddInstance:
        return "addInstance";
    case SceneOperationType::RemoveInstance:
        return "removeInstance";
    case SceneOperationType::ApplyGridLayout:
        return "applyGridLayout";
    case SceneOperationType::Translate:
        return "translate";
    case SceneOperationType::RotateZ:
        return "rotateZ";
    case SceneOperationType::UniformScale:
        return "uniformScale";
    case SceneOperationType::MirrorX:
        return "mirrorX";
    case SceneOperationType::MirrorY:
        return "mirrorY";
    }
    return "invalid";
}

StructuredJsonObject BuildPreflightDelta(
    const SceneInstanceState& instance)
{
    StructuredJsonObject delta;
    delta.utf8_json = "{\"instanceId\":\""
        + EscapeJson(instance.instance.instance_id)
        + "\",\"outOfBounds\":"
        + (instance.out_of_bounds ? "true" : "false") + "}";
    return delta;
}

StructuredJsonObject BuildRemovedPreflightDelta(
    const std::string_view instanceId)
{
    StructuredJsonObject delta;
    delta.utf8_json = "{\"instanceId\":\""
        + EscapeJson(instanceId) + "\",\"removed\":true}";
    return delta;
}

ApiResult<void> AddRegisteredModelToScene(
    AuthorityState& candidate,
    const SceneOperation& operation)
{
    if (operation.instance_id.empty())
    {
        return Failure<void>(
            "PM-SLICER-PROFILE-0031",
            "addInstance requires an assigned instance identity",
            "instanceId");
    }
    if (operation.model_id == 0U)
    {
        return Failure<void>(
            "PM-SLICER-INPUT-0002",
            "addInstance requires a valid modelId",
            operation.instance_id);
    }
    const auto duplicate = std::find_if(
        candidate.seed.scene.instances.begin(),
        candidate.seed.scene.instances.end(),
        [&operation](const SceneModelInstance& instance)
        {
            return instance.instance.instanceid == operation.instance_id;
        });
    if (duplicate != candidate.seed.scene.instances.end())
    {
        return Failure<void>(
            "PM-SLICER-PROFILE-0031",
            "addInstance assigned an existing instance identity",
            operation.instance_id);
    }

    const auto registration = candidate.seed.registered_models.find(
        operation.model_id);
    if (registration == candidate.seed.registered_models.end()
        || !registration->second.model)
    {
        return Failure<void>(
            "PM-SLICER-INPUT-0001",
            "addInstance references an unavailable imported model",
            std::to_string(operation.model_id));
    }
    const SceneFacadeModelRegistration& model = registration->second;
    const ModelTransformValidationResult transformValidation =
        ValidateModelTransform(
            operation.initial_transform,
            operation.instance_id,
            model.scene_model_id);
    if (!transformValidation.IsValid())
    {
        return Failure<void>(
            "PM-SLICER-PROFILE-0031",
            transformValidation.error->message,
            transformValidation.error->field);
    }

    const auto source = std::find_if(
        candidate.seed.scene.models.begin(),
        candidate.seed.scene.models.end(),
        [&model](const ModelSource& value)
        {
            return value.modelid == model.scene_model_id;
        });
    if (source == candidate.seed.scene.models.end())
    {
        const auto scope = std::find_if(
            candidate.seed.scene.resourcescopes.begin(),
            candidate.seed.scene.resourcescopes.end(),
            [&model](const ResourceScope& value)
            {
                return value.resourcescopeid == model.scope.resourcescopeid;
            });
        if (scope == candidate.seed.scene.resourcescopes.end())
        {
            candidate.seed.scene.resourcescopes.push_back(model.scope);
        }
        candidate.seed.scene.models.push_back(model.source);
        candidate.seed.models_by_id[model.scene_model_id] = model.model;
        candidate.seed.api_model_ids[model.scene_model_id] =
            model.api_model_id;
    }
    else
    {
        const auto apiIdentity = candidate.seed.api_model_ids.find(
            model.scene_model_id);
        if (apiIdentity == candidate.seed.api_model_ids.end()
            || apiIdentity->second != model.api_model_id)
        {
            return Failure<void>(
                "PM-SLICER-PROFILE-0031",
                "scene model identity conflicts with modelId",
                model.scene_model_id);
        }
    }

    SceneModelInstance instance;
    instance.instance.instanceid = operation.instance_id;
    instance.instance.modelid = model.scene_model_id;
    instance.instance.sourcetransformidentity =
        model.source.sourcepath.generic_string();
    instance.instance.sourcebboxmm = model.model->bbox_mm;
    instance.requestedtransform = NormalizeModelTransform(
        operation.initial_transform);
    instance.effectivetransform = ComposeModelTransforms(
        instance.derivedlayouttransform,
        instance.requestedtransform);
    instance.instance.transform = instance.effectivetransform;
    instance.instance.effectivebboxmm = model.model->bbox_mm;
    instance.admissionstatus = SceneInstanceAdmissionStatus::Admitted;
    instance.resolvedprofileid = candidate.seed.scene.resolvedprofileid;
    candidate.seed.scene.instances.push_back(std::move(instance));
    return ApiResult<void>::Success();
}

ApiResult<ModelTransform> ApplyDelta(
    const ModelTransform& current,
    const SceneOperation& operation)
{
    if (!std::isfinite(operation.value_x)
        || !std::isfinite(operation.value_y)
        || !std::isfinite(operation.value_z))
    {
        return Failure<ModelTransform>(
            "PM-SLICER-PROFILE-0031",
            "scene operation contains a non-finite value",
            operation.instance_id);
    }

    ModelTransform candidate = current;
    switch (operation.type)
    {
    case SceneOperationType::Translate:
        if (operation.value_z != 0.0)
        {
            return Failure<ModelTransform>(
                "PM-SLICER-PROFILE-0031",
                "Stage 14B scene translation is limited to XY",
                operation.instance_id);
        }
        candidate.translatexmm += operation.value_x;
        candidate.translateymm += operation.value_y;
        break;
    case SceneOperationType::RotateZ:
        candidate.rotatezdeg += operation.value_z;
        break;
    case SceneOperationType::UniformScale:
        if (!(operation.value_x > 0.0))
        {
            return Failure<ModelTransform>(
                "PM-SLICER-PROFILE-0031",
                "uniform scale factor must be positive",
                operation.instance_id);
        }
        candidate.uniformscale *= operation.value_x;
        break;
    case SceneOperationType::MirrorX:
        candidate.mirrorx = !candidate.mirrorx;
        break;
    case SceneOperationType::MirrorY:
        candidate.mirrory = !candidate.mirrory;
        break;
    default:
        return Failure<ModelTransform>(
            "PM-SLICER-PROFILE-0031",
            "unsupported scene operation type",
            operation.instance_id);
    }

    const ModelTransformValidationResult validation = ValidateModelTransform(
        candidate,
        operation.instance_id,
        {});
    if (!validation.IsValid())
    {
        return Failure<ModelTransform>(
            "PM-SLICER-PROFILE-0031",
            validation.error->message,
            validation.error->field);
    }
    return ApiResult<ModelTransform>::Success(
        NormalizeModelTransform(candidate));
}

}  // namespace

ApiResult<AuthorityState> ApplyOperationBatch(
    const AuthorityState& current,
    const SceneOperationRequest& request,
    const ICancelToken& cancelToken) noexcept
{
    try
    {
        if (cancelToken.IsCancelRequested())
        {
            return Failure<AuthorityState>(
                "PM-SLICER-CANCELLED-0070",
                "scene operation was cancelled before mutation",
                request.operation_id);
        }
        if (request.operations.empty())
        {
            return Failure<AuthorityState>(
                "PM-SLICER-PROFILE-0031",
                "scene operation batch must not be empty",
                request.operation_id);
        }

        AuthorityState candidate = current;
        std::set<std::string> touchedInstances;
        std::set<std::string> removedInstances;
        for (const SceneOperation& operation : request.operations)
        {
            if (cancelToken.IsCancelRequested())
            {
                return Failure<AuthorityState>(
                    "PM-SLICER-CANCELLED-0070",
                    "scene operation was cancelled before commit",
                    request.operation_id);
            }
            if (operation.type == SceneOperationType::AddInstance)
            {
                const ApiResult<void> added = AddRegisteredModelToScene(
                    candidate,
                    operation);
                if (!added.IsOk())
                {
                    return Failure<AuthorityState>(
                        added.Error()->code,
                        added.Error()->message,
                        added.Error()->detail);
                }
                touchedInstances.insert(operation.instance_id);
                continue;
            }
            if (operation.type == SceneOperationType::ApplyGridLayout)
            {
                if (request.operations.size() != 1U)
                {
                    return Failure<AuthorityState>(
                        "PM-SLICER-PROFILE-0031",
                        "applyGridLayout must be the only operation in its batch",
                        request.operation_id);
                }
                const auto layout = ApplyGridLayout(
                    candidate,
                    operation.layout);
                if (!layout.IsOk())
                {
                    return Failure<AuthorityState>(
                        layout.Error()->code,
                        layout.Error()->message,
                        layout.Error()->detail);
                }
                touchedInstances.insert(
                    layout.Value()->begin(),
                    layout.Value()->end());
                continue;
            }

            const auto found = std::find_if(
                candidate.seed.scene.instances.begin(),
                candidate.seed.scene.instances.end(),
                [&operation](const SceneModelInstance& instance)
                {
                    return instance.instance.instanceid
                        == operation.instance_id;
                });
            if (found == candidate.seed.scene.instances.end())
            {
                return Failure<AuthorityState>(
                    "PM-SLICER-PROFILE-0031",
                    "scene operation references an unknown instance",
                    operation.instance_id);
            }
            if (found->instance.locked)
            {
                return Failure<AuthorityState>(
                    "PM-SLICER-PROFILE-0031",
                    "scene operation cannot modify a locked instance",
                    operation.instance_id);
            }
            if (operation.type == SceneOperationType::RemoveInstance)
            {
                removedInstances.insert(operation.instance_id);
                touchedInstances.erase(operation.instance_id);
                candidate.seed.scene.instances.erase(found);
                continue;
            }
            const ApiResult<ModelTransform> updated = ApplyDelta(
                found->requestedtransform,
                operation);
            if (!updated.IsOk())
            {
                return Failure<AuthorityState>(
                    updated.Error()->code,
                    updated.Error()->message,
                    updated.Error()->detail);
            }
            found->requestedtransform = *updated.Value();
            touchedInstances.insert(operation.instance_id);
        }

        for (SceneModelInstance& sceneInstance : candidate.seed.scene.instances)
        {
            if (!touchedInstances.contains(sceneInstance.instance.instanceid))
            {
                continue;
            }
            const ModelTransform effective = ComposeModelTransforms(
                sceneInstance.derivedlayouttransform,
                sceneInstance.requestedtransform);
            const ModelInstanceTransformUpdateResult update =
                UpdateModelInstanceTransform(
                    sceneInstance.instance,
                    effective,
                    sceneInstance.instance.transformrevision);
            if (!update.IsValid())
            {
                return Failure<AuthorityState>(
                    update.error->code == ModelTransformErrorCode::RevisionStale
                        ? "PM-SLICER-LAYOUT-0022"
                        : "PM-SLICER-PROFILE-0031",
                    update.error->message,
                    update.error->field);
            }
            sceneInstance.effectivetransform = effective;
        }

        ++candidate.seed.scene.scenerevision;
        ApiResult<AuthorityState> evaluated = BuildAuthorityState(
            std::move(candidate.seed));
        if (!evaluated.IsOk())
        {
            return evaluated;
        }
        AuthorityState evaluatedState = *evaluated.Value();
        for (const SceneInstanceState& instance :
             evaluatedState.snapshot.instances)
        {
            if (touchedInstances.contains(instance.instance.instance_id))
            {
                evaluatedState.preflight_delta.push_back(
                    BuildPreflightDelta(instance));
            }
        }
        for (const std::string& instanceId : removedInstances)
        {
            evaluatedState.preflight_delta.push_back(
                BuildRemovedPreflightDelta(instanceId));
        }
        return ApiResult<AuthorityState>::Success(
            std::move(evaluatedState));
    }
    catch (const std::exception& error)
    {
        return Failure<AuthorityState>(
            "PM-SLICER-INTERNAL-0099",
            "failed to apply authoritative scene operation",
            error.what());
    }
    catch (...)
    {
        return Failure<AuthorityState>(
            "PM-SLICER-INTERNAL-0099",
            "failed to apply authoritative scene operation",
            "unknown exception");
    }
}

std::string ComputeOperationFingerprint(const SceneOperationRequest& request)
{
    std::ostringstream canonical;
    canonical << "{\"currentSceneRevision\":"
              << request.current_scene_revision
              << ",\"expectedSceneRevision\":"
              << request.expected_scene_revision
              << ",\"operations\":[";
    for (std::size_t index = 0U; index < request.operations.size(); ++index)
    {
        const SceneOperation& operation = request.operations[index];
        if (index > 0U)
        {
            canonical << ',';
        }
        canonical << "{\"instanceId\":\""
                  << EscapeJson(operation.instance_id)
                  << "\",\"modelId\":" << operation.model_id
                  << ",\"layout\":{\"policy\":\""
                  << EscapeJson(operation.layout.policy)
                  << "\",\"maxColumns\":"
                  << operation.layout.maxcolumns
                  << ",\"maxRows\":"
                  << operation.layout.maxrows
                  << ",\"columnGapMm\":"
                  << FormatNumber(operation.layout.columngapmm)
                  << ",\"rowGapMm\":"
                  << FormatNumber(operation.layout.rowgapmm)
                  << ",\"spacingMode\":\""
                  << EscapeJson(operation.layout.spacingmode)
                  << "\",\"order\":\""
                  << EscapeJson(operation.layout.order) << "\"}"
                  << ",\"type\":\""
                  << OperationTypeName(operation.type)
                  << "\",\"initialTransform\":{\"translateXMm\":"
                  << FormatNumber(operation.initial_transform.translatexmm)
                  << ",\"translateYMm\":"
                  << FormatNumber(operation.initial_transform.translateymm)
                  << ",\"rotateZDeg\":"
                  << FormatNumber(operation.initial_transform.rotatezdeg)
                  << ",\"uniformScale\":"
                  << FormatNumber(operation.initial_transform.uniformscale)
                  << ",\"mirrorX\":"
                  << (operation.initial_transform.mirrorx ? "true" : "false")
                  << ",\"mirrorY\":"
                  << (operation.initial_transform.mirrory ? "true" : "false")
                  << '}'
                  << ",\"valueX\":" << FormatNumber(operation.value_x)
                  << ",\"valueY\":" << FormatNumber(operation.value_y)
                  << ",\"valueZ\":" << FormatNumber(operation.value_z)
                  << '}';
    }
    canonical << "],\"sceneContextIdentity\":\""
              << EscapeJson(request.scene_context_identity)
              << "\",\"scene_identity\":" << request.scene_id << '}';
    return ComputeSha256(canonical.str());
}

}  // namespace slicer_core::api::scene_facade_detail
