#include "slicer_core/scene/ModelTransform.h"

#include "slicer_core/system/Sha256.h"

#include <algorithm>
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
    "slicesoft.model_transform.2"};

using Matrix3 = std::array<double, 9>;

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

double NormalizeDegrees(const double value)
{
    double normalized = std::fmod(value, 360.0);
    if (normalized < 0.0)
    {
        normalized += 360.0;
    }
    return NormalizeSignedZero(normalized);
}

Matrix3 Multiply(const Matrix3& left, const Matrix3& right)
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
}

Matrix3 BuildLinear(const ModelTransform& transform)
{
    const double radiansX =
        transform.rotatexdeg * std::numbers::pi_v<double> / 180.0;
    const double radiansY =
        transform.rotateydeg * std::numbers::pi_v<double> / 180.0;
    const double radiansZ =
        transform.rotatezdeg * std::numbers::pi_v<double> / 180.0;
    const double cosineX = std::cos(radiansX);
    const double sineX = std::sin(radiansX);
    const double cosineY = std::cos(radiansY);
    const double sineY = std::sin(radiansY);
    const double cosineZ = std::cos(radiansZ);
    const double sineZ = std::sin(radiansZ);

    const Matrix3 mirrorScale{
        (transform.mirrorx ? -1.0 : 1.0) * transform.uniformscale,
        0.0,
        0.0,
        0.0,
        (transform.mirrory ? -1.0 : 1.0) * transform.uniformscale,
        0.0,
        0.0,
        0.0,
        transform.uniformscale,
    };
    const Matrix3 rotationX{
        1.0, 0.0, 0.0,
        0.0, cosineX, -sineX,
        0.0, sineX, cosineX,
    };
    const Matrix3 rotationY{
        cosineY, 0.0, sineY,
        0.0, 1.0, 0.0,
        -sineY, 0.0, cosineY,
    };
    const Matrix3 rotationZ{
        cosineZ, -sineZ, 0.0,
        sineZ, cosineZ, 0.0,
        0.0, 0.0, 1.0,
    };
    return Multiply(
        rotationZ,
        Multiply(rotationY, Multiply(rotationX, mirrorScale)));
}

double Determinant(const Matrix3& matrix)
{
    return matrix.at(0U)
            * (matrix.at(4U) * matrix.at(8U)
               - matrix.at(5U) * matrix.at(7U))
        - matrix.at(1U)
            * (matrix.at(3U) * matrix.at(8U)
               - matrix.at(5U) * matrix.at(6U))
        + matrix.at(2U)
            * (matrix.at(3U) * matrix.at(7U)
               - matrix.at(4U) * matrix.at(6U));
}

void SetEulerAnglesFromRotation(
    const Matrix3& rotation,
    ModelTransform& transform)
{
    constexpr double kGimbalTolerance{1.0e-12};
    const double sineY = std::clamp(-rotation.at(6U), -1.0, 1.0);
    const double radiansY = std::asin(sineY);
    const double cosineY = std::cos(radiansY);
    double radiansX{0.0};
    double radiansZ{0.0};
    if (std::abs(cosineY) > kGimbalTolerance)
    {
        radiansX = std::atan2(rotation.at(7U), rotation.at(8U));
        radiansZ = std::atan2(rotation.at(3U), rotation.at(0U));
    }
    else
    {
        radiansZ = std::atan2(-rotation.at(1U), rotation.at(4U));
    }

    transform.rotatexdeg =
        radiansX * 180.0 / std::numbers::pi_v<double>;
    transform.rotateydeg =
        radiansY * 180.0 / std::numbers::pi_v<double>;
    transform.rotatezdeg =
        radiansZ * 180.0 / std::numbers::pi_v<double>;
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
        {"rotatexdeg", transform.rotatexdeg},
        {"rotateydeg", transform.rotateydeg},
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
    normalized.rotatexdeg = NormalizeDegrees(normalized.rotatexdeg);
    normalized.rotateydeg = NormalizeDegrees(normalized.rotateydeg);
    normalized.rotatezdeg = NormalizeDegrees(normalized.rotatezdeg);
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
        && normalizedLeft.rotatexdeg == normalizedRight.rotatexdeg
        && normalizedLeft.rotateydeg == normalizedRight.rotateydeg
        && normalizedLeft.rotatezdeg == normalizedRight.rotatezdeg
        && normalizedLeft.uniformscale == normalizedRight.uniformscale
        && normalizedLeft.mirrorx == normalizedRight.mirrorx
        && normalizedLeft.mirrory == normalizedRight.mirrory
        && normalizedLeft.landonbuildplate
            == normalizedRight.landonbuildplate;
}

ModelTransform ComposeModelTransforms(
    const ModelTransform& outer,
    const ModelTransform& inner)
{
    const ModelTransform normalizedOuter =
        NormalizeModelTransform(outer);
    const ModelTransform normalizedInner =
        NormalizeModelTransform(inner);

    const bool outerIsTranslationOnly =
        normalizedOuter.rotatexdeg == 0.0
        && normalizedOuter.rotateydeg == 0.0
        && normalizedOuter.rotatezdeg == 0.0
        && normalizedOuter.uniformscale == 1.0
        && !normalizedOuter.mirrorx
        && !normalizedOuter.mirrory;
    if (outerIsTranslationOnly)
    {
        ModelTransform result = normalizedInner;
        result.translatexmm += normalizedOuter.translatexmm;
        result.translateymm += normalizedOuter.translateymm;
        result.landonbuildplate =
            normalizedOuter.landonbuildplate
            || normalizedInner.landonbuildplate;
        return NormalizeModelTransform(result);
    }

    const Matrix3 outerLinear = BuildLinear(normalizedOuter);
    const Matrix3 innerLinear = BuildLinear(normalizedInner);
    const Matrix3 combined = Multiply(outerLinear, innerLinear);

    ModelTransform result;
    result.uniformscale =
        normalizedOuter.uniformscale
        * normalizedInner.uniformscale;
    result.mirrorx = Determinant(combined) < 0.0;
    result.mirrory = false;
    Matrix3 rotation = combined;
    for (double& value : rotation)
    {
        value /= result.uniformscale;
    }
    if (result.mirrorx)
    {
        rotation.at(0U) *= -1.0;
        rotation.at(3U) *= -1.0;
        rotation.at(6U) *= -1.0;
    }
    SetEulerAnglesFromRotation(rotation, result);
    result.translatexmm =
        outerLinear.at(0U) * normalizedInner.translatexmm
        + outerLinear.at(1U) * normalizedInner.translateymm
        + normalizedOuter.translatexmm;
    result.translateymm =
        outerLinear.at(3U) * normalizedInner.translatexmm
        + outerLinear.at(4U) * normalizedInner.translateymm
        + normalizedOuter.translateymm;
    result.landonbuildplate =
        normalizedOuter.landonbuildplate
        || normalizedInner.landonbuildplate;
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
            << normalized.rotatexdeg << '\n'
            << normalized.rotateydeg << '\n'
            << normalized.rotatezdeg << '\n'
            << normalized.uniformscale << '\n'
            << (normalized.mirrorx ? 1 : 0) << '\n'
            << (normalized.mirrory ? 1 : 0) << '\n'
            << (normalized.landonbuildplate ? 1 : 0);
    return {ComputeSha256(payload.str()), std::nullopt};
}

}  // namespace slicer_core
