#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace slicer_core
{

/**
 * @brief User-controlled instance transform applied after SourceTransform.
 */
struct ModelTransform
{
    double translatexmm{0.0};
    double translateymm{0.0};
    double rotatexdeg{0.0};
    double rotateydeg{0.0};
    double rotatezdeg{0.0};
    double uniformscale{1.0};
    bool mirrorx{false};
    bool mirrory{false};
    bool landonbuildplate{false};
    double translatezmm{0.0};
};

/**
 * @brief Stable validation and optimistic-update error codes.
 */
enum class ModelTransformErrorCode
{
    None,
    NonFinite,
    ScaleNonPositive,
    SourceMissing,
    InstanceIdEmpty,
    ModelIdEmpty,
    RevisionStale,
};

/**
 * @brief Structured transform error suitable for reports and UI translation.
 */
struct ModelTransformError
{
    ModelTransformErrorCode code{ModelTransformErrorCode::None};
    std::string instanceid;
    std::string modelid;
    std::string field;
    std::string message;
};

/**
 * @brief Validation result for a model transform.
 */
struct ModelTransformValidationResult
{
    std::optional<ModelTransformError> error;

    /**
     * @brief Report whether transform validation succeeded.
     * @return True when no validation error is present.
     */
    bool IsValid() const;
};

/**
 * @brief Stable hash result for a source and instance transform identity.
 */
struct ModelTransformHashResult
{
    std::string hash;
    std::optional<ModelTransformError> error;

    /**
     * @brief Report whether the transform hash was generated.
     * @return True when no validation error is present.
     */
    bool IsValid() const;
};

/**
 * @brief Return the stable protocol name for a transform error.
 * @param code Transform error code.
 * @return Stable ASCII error name.
 */
std::string_view ModelTransformErrorCodeName(ModelTransformErrorCode code);

/**
 * @brief Validate finite values and positive scale.
 * @param transform Instance transform to validate.
 * @param instanceId Instance identity included in an error.
 * @param modelId Source model identity included in an error.
 * @return Validation result with a stable error when invalid.
 */
ModelTransformValidationResult ValidateModelTransform(
    const ModelTransform& transform,
    std::string_view instanceId,
    std::string_view modelId);

/**
 * @brief Normalize rotation and signed zero for stable comparison.
 * @param transform Transform to normalize.
 * @return Canonical transform without reducing calculation precision.
 */
ModelTransform NormalizeModelTransform(const ModelTransform& transform);

/**
 * @brief Compare two transforms after contract normalization.
 * @param left First transform.
 * @param right Second transform.
 * @return True when both transforms have the same effective values.
 */
bool ModelTransformsEquivalent(
    const ModelTransform& left,
    const ModelTransform& right);

/**
 * @brief Compose two instance transforms using outer * inner order.
 * @param outer Transform applied after the inner transform.
 * @param inner Transform applied first around the same source pivot.
 * @return Canonical transform equivalent to the composition.
 */
ModelTransform ComposeModelTransforms(
    const ModelTransform& outer,
    const ModelTransform& inner);

/**
 * @brief Compute the stable transform identity hash.
 * @param transform Instance transform.
 * @param sourceTransformIdentity Identity of the preceding source transform.
 * @param instanceId Stable instance identity.
 * @param modelId Stable source model identity.
 * @return Hash result or stable validation error.
 */
ModelTransformHashResult ComputeModelTransformHash(
    const ModelTransform& transform,
    std::string_view sourceTransformIdentity,
    std::string_view instanceId,
    std::string_view modelId);

}  // namespace slicer_core
