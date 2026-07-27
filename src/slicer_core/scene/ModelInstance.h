#pragma once

#include "slicer_core/model.h"
#include "slicer_core/scene/ModelTransform.h"

#include <cstdint>
#include <optional>
#include <string>

namespace slicer_core
{

/**
 * @brief Stable scene instance identity and its effective transform state.
 */
struct ModelInstance
{
    std::string instanceid;
    std::string modelid;
    std::string sourcetransformidentity;
    ModelTransform transform;
    bool visible{true};
    bool locked{false};
    std::uint64_t transformrevision{0U};
    BoundingBox sourcebboxmm;
    BoundingBox effectivebboxmm;
};

/**
 * @brief Result of an optimistic model transform update.
 */
struct ModelInstanceTransformUpdateResult
{
    bool changed{false};
    std::optional<ModelTransformError> error;

    /**
     * @brief Report whether the transform update succeeded.
     * @return True when no validation or revision error is present.
     */
    bool IsValid() const;
};

/**
 * @brief Validate instance identity, source identity, and transform.
 * @param instance Model instance to validate.
 * @return Stable error when the instance is invalid.
 */
std::optional<ModelTransformError> ValidateModelInstance(
    const ModelInstance& instance);

/**
 * @brief Update an instance transform using optimistic revision checking.
 * @param instance Instance to mutate after all checks succeed.
 * @param transform Requested transform.
 * @param expectedRevision Revision observed by the caller.
 * @return Update result indicating whether an effective change occurred.
 */
ModelInstanceTransformUpdateResult UpdateModelInstanceTransform(
    ModelInstance& instance,
    const ModelTransform& transform,
    std::uint64_t expectedRevision);

}  // namespace slicer_core
