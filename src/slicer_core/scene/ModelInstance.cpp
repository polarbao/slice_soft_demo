#include "slicer_core/scene/ModelInstance.h"

namespace slicer_core
{
namespace
{

ModelTransformError MakeInstanceError(
    const ModelTransformErrorCode code,
    const ModelInstance& instance,
    const std::string_view field,
    const std::string_view message)
{
    ModelTransformError error;
    error.code = code;
    error.instanceid = instance.instanceid;
    error.modelid = instance.modelid;
    error.field = field;
    error.message = message;
    return error;
}

}  // namespace

bool ModelInstanceTransformUpdateResult::IsValid() const
{
    return !error.has_value();
}

std::optional<ModelTransformError> ValidateModelInstance(
    const ModelInstance& instance)
{
    if (instance.instanceid.empty())
    {
        return MakeInstanceError(
            ModelTransformErrorCode::InstanceIdEmpty,
            instance,
            "instanceid",
            "model instance id must not be empty");
    }
    if (instance.modelid.empty())
    {
        return MakeInstanceError(
            ModelTransformErrorCode::ModelIdEmpty,
            instance,
            "modelid",
            "model id must not be empty");
    }
    if (instance.sourcetransformidentity.empty())
    {
        return MakeInstanceError(
            ModelTransformErrorCode::SourceMissing,
            instance,
            "sourcetransformidentity",
            "source transform identity must not be empty");
    }

    const ModelTransformValidationResult validation =
        ValidateModelTransform(
            instance.transform,
            instance.instanceid,
            instance.modelid);
    return validation.error;
}

ModelInstanceTransformUpdateResult UpdateModelInstanceTransform(
    ModelInstance& instance,
    const ModelTransform& transform,
    const std::uint64_t expectedRevision)
{
    if (const std::optional<ModelTransformError> instanceError =
            ValidateModelInstance(instance);
        instanceError.has_value())
    {
        return {false, instanceError};
    }

    const ModelTransformValidationResult validation =
        ValidateModelTransform(
            transform,
            instance.instanceid,
            instance.modelid);
    if (!validation.IsValid())
    {
        return {false, validation.error};
    }

    if (expectedRevision != instance.transformrevision)
    {
        return {
            false,
            MakeInstanceError(
                ModelTransformErrorCode::RevisionStale,
                instance,
                "transformrevision",
                "model transform revision is stale")};
    }

    if (ModelTransformsEquivalent(instance.transform, transform))
    {
        return {};
    }

    instance.transform = NormalizeModelTransform(transform);
    ++instance.transformrevision;
    return {true, std::nullopt};
}

}  // namespace slicer_core
