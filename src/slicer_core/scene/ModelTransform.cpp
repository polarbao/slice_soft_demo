#include "slicer_core/scene/ModelTransform.h"

#include "slicer_core/system/Sha256.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <numbers>
#include <sstream>

namespace slicer_core
{
namespace
{

constexpr std::string_view kModelTransformSchema{
    "slicesoft.model_transform.1"};

ModelTransformError MakeError(
    const ModelTransformErrorCode code,
    const std::string_view instanceId,
    const std::string_view modelId,
    const std::string_view field,
    const std::string_view message)
{
    ModelTransformError error;
    error.code = code;
    error.instanceid = instanceId;
    error.modelid = modelId;
    error.field = field;
    error.message = message;
    return error;
}

double NormalizeSignedZero(const double value)
{
    return value == 0.0 ? 0.0 : value;
}

void AppendLengthPrefixed(
    std::ostringstream& payload,
    const std::string_view value)
{
    payload << value.size() << ':';
    payload.write(value.data(), static_cast<std::streamsize>(value.size()));
    payload << '\n';
}

}  // namespace

bool ModelTransformValidationResult::IsValid() const
{
    return !error.has_value();
}

bool ModelTransformHashResult::IsValid() const
{
    return !error.has_value();
}

std::string_view ModelTransformErrorCodeName(
    const ModelTransformErrorCode code)
{
    switch (code)
    {
    case ModelTransformErrorCode::None:
        return "NONE";
    case ModelTransformErrorCode::NonFinite:
        return "MODEL_TRANSFORM_NON_FINITE";
    case ModelTransformErrorCode::ScaleNonPositive:
        return "MODEL_TRANSFORM_SCALE_NON_POSITIVE";
    case ModelTransformErrorCode::SourceMissing:
        return "MODEL_TRANSFORM_SOURCE_MISSING";
    case ModelTransformErrorCode::InstanceIdEmpty:
        return "MODEL_INSTANCE_ID_EMPTY";
    case ModelTransformErrorCode::ModelIdEmpty:
        return "MODEL_INSTANCE_MODEL_ID_EMPTY";
    case ModelTransformErrorCode::RevisionStale:
        return "MODEL_TRANSFORM_REVISION_STALE";
    }
    return "MODEL_TRANSFORM_UNKNOWN";
}

ModelTransformValidationResult ValidateModelTransform(
    const ModelTransform& transform,
    const std::string_view instanceId,
    const std::string_view modelId)
{
    const struct
    {
        std::string_view field;
        double value;
    } finiteValues[]{
        {"translatexmm", transform.translatexmm},
        {"translateymm", transform.translateymm},
        {"rotatezdeg", transform.rotatezdeg},
        {"uniformscale", transform.uniformscale},
    };

    for (const auto& finiteValue : finiteValues)
    {
        if (!std::isfinite(finiteValue.value))
        {
            return {
                MakeError(
                    ModelTransformErrorCode::NonFinite,
                    instanceId,
                    modelId,
                    finiteValue.field,
                    "model transform value must be finite")};
        }
    }

    if (transform.uniformscale <= 0.0)
    {
        return {
            MakeError(
                ModelTransformErrorCode::ScaleNonPositive,
                instanceId,
                modelId,
                "uniformscale",
                "model transform uniform scale must be positive")};
    }
    return {};
}

ModelTransform NormalizeModelTransform(const ModelTransform& transform)
{
    ModelTransform normalized = transform;
    normalized.translatexmm = NormalizeSignedZero(normalized.translatexmm);
    normalized.translateymm = NormalizeSignedZero(normalized.translateymm);
    normalized.uniformscale = NormalizeSignedZero(normalized.uniformscale);
    normalized.rotatezdeg = std::fmod(normalized.rotatezdeg, 360.0);
    if (normalized.rotatezdeg < 0.0)
    {
        normalized.rotatezdeg += 360.0;
    }
    normalized.rotatezdeg = NormalizeSignedZero(normalized.rotatezdeg);
    return normalized;
}

bool ModelTransformsEquivalent(
    const ModelTransform& left,
    const ModelTransform& right)
{
    const ModelTransform normalizedLeft = NormalizeModelTransform(left);
    const ModelTransform normalizedRight = NormalizeModelTransform(right);
    return normalizedLeft.translatexmm == normalizedRight.translatexmm
        && normalizedLeft.translateymm == normalizedRight.translateymm
        && normalizedLeft.rotatezdeg == normalizedRight.rotatezdeg
        && normalizedLeft.uniformscale == normalizedRight.uniformscale
        && normalizedLeft.mirrorx == normalizedRight.mirrorx
        && normalizedLeft.mirrory == normalizedRight.mirrory;
}

ModelTransform ComposeModelTransforms(
    const ModelTransform& outer,
    const ModelTransform& inner)
{
    const ModelTransform normalizedOuter =
        NormalizeModelTransform(outer);
    const ModelTransform normalizedInner =
        NormalizeModelTransform(inner);

    const auto buildLinear =
        [](const ModelTransform& transform)
    {
        const double radians =
            transform.rotatezdeg
            * std::numbers::pi_v<double> / 180.0;
        const double cosine = std::cos(radians);
        const double sine = std::sin(radians);
        const double mirrorX = transform.mirrorx ? -1.0 : 1.0;
        const double mirrorY = transform.mirrory ? -1.0 : 1.0;
        const double scale = transform.uniformscale;
        return std::array<double, 4>{
            cosine * mirrorX * scale,
            -sine * mirrorY * scale,
            sine * mirrorX * scale,
            cosine * mirrorY * scale,
        };
    };

    const std::array<double, 4> outerLinear =
        buildLinear(normalizedOuter);
    const std::array<double, 4> innerLinear =
        buildLinear(normalizedInner);
    const std::array<double, 4> combined{
        outerLinear.at(0U) * innerLinear.at(0U)
            + outerLinear.at(1U) * innerLinear.at(2U),
        outerLinear.at(0U) * innerLinear.at(1U)
            + outerLinear.at(1U) * innerLinear.at(3U),
        outerLinear.at(2U) * innerLinear.at(0U)
            + outerLinear.at(3U) * innerLinear.at(2U),
        outerLinear.at(2U) * innerLinear.at(1U)
            + outerLinear.at(3U) * innerLinear.at(3U),
    };

    ModelTransform result;
    result.uniformscale =
        normalizedOuter.uniformscale
        * normalizedInner.uniformscale;
    const double determinant =
        combined.at(0U) * combined.at(3U)
        - combined.at(1U) * combined.at(2U);
    result.mirrorx = determinant < 0.0;
    result.mirrory = false;
    const double inverseScale = 1.0 / result.uniformscale;
    const double radians = result.mirrorx
        ? std::atan2(
              -combined.at(2U) * inverseScale,
              -combined.at(0U) * inverseScale)
        : std::atan2(
              combined.at(2U) * inverseScale,
              combined.at(0U) * inverseScale);
    result.rotatezdeg =
        radians * 180.0 / std::numbers::pi_v<double>;
    result.translatexmm =
        outerLinear.at(0U) * normalizedInner.translatexmm
        + outerLinear.at(1U) * normalizedInner.translateymm
        + normalizedOuter.translatexmm;
    result.translateymm =
        outerLinear.at(2U) * normalizedInner.translatexmm
        + outerLinear.at(3U) * normalizedInner.translateymm
        + normalizedOuter.translateymm;
    return NormalizeModelTransform(result);
}

ModelTransformHashResult ComputeModelTransformHash(
    const ModelTransform& transform,
    const std::string_view sourceTransformIdentity,
    const std::string_view instanceId,
    const std::string_view modelId)
{
    const ModelTransformValidationResult validation =
        ValidateModelTransform(transform, instanceId, modelId);
    if (!validation.IsValid())
    {
        return {{}, validation.error};
    }
    if (sourceTransformIdentity.empty())
    {
        return {
            {},
            MakeError(
                ModelTransformErrorCode::SourceMissing,
                instanceId,
                modelId,
                "sourcetransformidentity",
                "source transform identity must not be empty")};
    }

    const ModelTransform normalized = NormalizeModelTransform(transform);
    std::ostringstream payload;
    payload.imbue(std::locale::classic());
    AppendLengthPrefixed(payload, kModelTransformSchema);
    AppendLengthPrefixed(payload, sourceTransformIdentity);
    AppendLengthPrefixed(payload, instanceId);
    AppendLengthPrefixed(payload, modelId);
    payload << std::setprecision(std::numeric_limits<double>::max_digits10)
            << normalized.translatexmm << '\n'
            << normalized.translateymm << '\n'
            << normalized.rotatezdeg << '\n'
            << normalized.uniformscale << '\n'
            << (normalized.mirrorx ? 1 : 0) << '\n'
            << (normalized.mirrory ? 1 : 0);
    return {ComputeSha256(payload.str()), std::nullopt};
}

}  // namespace slicer_core
