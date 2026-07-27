#include "slicer_core/geometry/TransformedModelAdapter.h"
#include "slicer_core/scene/ModelInstance.h"
#include "slicer_core/scene/ModelTransform.h"
#include "slicer_core/system/Sha256.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace
{

constexpr double kTolerance = 1.0e-9;

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool NearlyEqual(const double left, const double right)
{
    return std::abs(left - right) <= kTolerance;
}

bool VecEqual(const slicer_core::Vec3& left, const slicer_core::Vec3& right)
{
    return NearlyEqual(left.x, right.x)
        && NearlyEqual(left.y, right.y)
        && NearlyEqual(left.z, right.z);
}

bool BoundingBoxEqual(
    const slicer_core::BoundingBox& left,
    const slicer_core::BoundingBox& right)
{
    return VecEqual(left.min, right.min) && VecEqual(left.max, right.max);
}

bool TriangleEqual(
    const slicer_core::Triangle& left,
    const slicer_core::Triangle& right)
{
    return VecEqual(left.a, right.a)
        && VecEqual(left.b, right.b)
        && VecEqual(left.c, right.c);
}

bool TextureEqual(
    const slicer_core::TriangleTextureInfo& left,
    const slicer_core::TriangleTextureInfo& right)
{
    if (left.has_uv != right.has_uv
        || left.material_name != right.material_name)
    {
        return false;
    }

    for (std::size_t index{0U}; index < left.uv.size(); ++index)
    {
        if (!NearlyEqual(left.uv.at(index).u, right.uv.at(index).u)
            || !NearlyEqual(left.uv.at(index).v, right.uv.at(index).v))
        {
            return false;
        }
    }
    return true;
}

slicer_core::SceneModel MakeScene()
{
    slicer_core::SceneModel scene;
    scene.model_path = "fixture.obj";
    scene.bbox_mm.min = {0.0, 0.0, 2.0};
    scene.bbox_mm.max = {2.0, 4.0, 4.0};
    scene.triangles = {
        {
            {2.0, 2.0, 2.0},
            {1.0, 3.0, 2.0},
            {1.0, 2.0, 4.0},
        },
        {
            {0.0, 0.0, 2.0},
            {2.0, 0.0, 2.0},
            {1.0, 4.0, 4.0},
        },
    };
    scene.triangle_count = scene.triangles.size();

    slicer_core::TriangleTextureInfo firstTexture;
    firstTexture.has_uv = true;
    firstTexture.uv = {
        slicer_core::TexCoord{0.1, 0.2},
        slicer_core::TexCoord{0.3, 0.4},
        slicer_core::TexCoord{0.5, 0.6},
    };
    firstTexture.material_name = "fixture";
    scene.triangle_textures = {
        firstTexture,
        slicer_core::TriangleTextureInfo{},
    };
    return scene;
}

slicer_core::ModelInstance MakeInstance()
{
    slicer_core::ModelInstance instance;
    instance.instanceid = "instance-1";
    instance.modelid = "model-1";
    instance.sourcetransformidentity = "source-transform-1";
    instance.sourcebboxmm = MakeScene().bbox_mm;
    instance.effectivebboxmm = instance.sourcebboxmm;
    return instance;
}

bool StableErrorsAndValidation()
{
    using slicer_core::ModelTransformErrorCode;

    const std::array<std::pair<ModelTransformErrorCode, std::string>, 7>
        expectedNames{{
            {ModelTransformErrorCode::None, "NONE"},
            {ModelTransformErrorCode::NonFinite, "MODEL_TRANSFORM_NON_FINITE"},
            {ModelTransformErrorCode::ScaleNonPositive,
             "MODEL_TRANSFORM_SCALE_NON_POSITIVE"},
            {ModelTransformErrorCode::SourceMissing,
             "MODEL_TRANSFORM_SOURCE_MISSING"},
            {ModelTransformErrorCode::InstanceIdEmpty,
             "MODEL_INSTANCE_ID_EMPTY"},
            {ModelTransformErrorCode::ModelIdEmpty,
             "MODEL_INSTANCE_MODEL_ID_EMPTY"},
            {ModelTransformErrorCode::RevisionStale,
             "MODEL_TRANSFORM_REVISION_STALE"},
        }};

    bool ok = true;
    for (const auto& [code, expectedName] : expectedNames)
    {
        ok = ExpectTrue(
                 slicer_core::ModelTransformErrorCodeName(code) == expectedName,
                 "transform error name matches contract")
            && ok;
    }

    slicer_core::ModelTransform nonFinite;
    nonFinite.translatexmm = std::numeric_limits<double>::quiet_NaN();
    const auto nonFiniteValidation =
        slicer_core::ValidateModelTransform(nonFinite, "instance-1", "model-1");
    ok = ExpectTrue(
             !nonFiniteValidation.IsValid()
                 && nonFiniteValidation.error->code
                     == ModelTransformErrorCode::NonFinite
                 && nonFiniteValidation.error->field == "translatexmm",
             "non-finite transform is rejected with field")
        && ok;

    slicer_core::ModelTransform invalidScale;
    invalidScale.uniformscale = 0.0;
    const auto scaleValidation =
        slicer_core::ValidateModelTransform(invalidScale, "instance-1", "model-1");
    ok = ExpectTrue(
             !scaleValidation.IsValid()
                 && scaleValidation.error->code
                     == ModelTransformErrorCode::ScaleNonPositive,
             "non-positive scale is rejected")
        && ok;

    slicer_core::ModelInstance missingInstanceId = MakeInstance();
    missingInstanceId.instanceid.clear();
    const auto instanceError =
        slicer_core::ValidateModelInstance(missingInstanceId);
    ok = ExpectTrue(
             instanceError.has_value()
                 && instanceError->code
                     == ModelTransformErrorCode::InstanceIdEmpty,
             "empty instance id is rejected")
        && ok;

    slicer_core::ModelInstance missingModelId = MakeInstance();
    missingModelId.modelid.clear();
    const auto modelError = slicer_core::ValidateModelInstance(missingModelId);
    ok = ExpectTrue(
             modelError.has_value()
                 && modelError->code == ModelTransformErrorCode::ModelIdEmpty,
             "empty model id is rejected")
        && ok;

    slicer_core::ModelInstance missingSource = MakeInstance();
    missingSource.sourcetransformidentity.clear();
    const auto sourceError =
        slicer_core::ValidateModelInstance(missingSource);
    return ExpectTrue(
               sourceError.has_value()
                   && sourceError->code
                       == ModelTransformErrorCode::SourceMissing,
               "missing source identity is rejected")
        && ok;
}

bool TransformHashIsStableAndSensitive()
{
    const std::string knownDigest = slicer_core::ComputeSha256("abc");
    if (!ExpectTrue(
            knownDigest
                == "ba7816bf8f01cfea414140de5dae2223"
                   "b00361a396177a9cb410ff61f20015ad",
            "shared SHA-256 implementation matches the known vector: "
                + knownDigest))
    {
        return false;
    }

    slicer_core::ModelTransform identity;
    const auto first = slicer_core::ComputeModelTransformHash(
        identity,
        "source-transform-1",
        "instance-1",
        "model-1");
    const auto second = slicer_core::ComputeModelTransformHash(
        identity,
        "source-transform-1",
        "instance-1",
        "model-1");

    slicer_core::ModelTransform equivalent = identity;
    equivalent.translatexmm = -0.0;
    equivalent.rotatezdeg = 360.0;
    const auto normalized = slicer_core::ComputeModelTransformHash(
        equivalent,
        "source-transform-1",
        "instance-1",
        "model-1");

    slicer_core::ModelTransform changed = identity;
    changed.mirrorx = true;
    const auto changedTransform = slicer_core::ComputeModelTransformHash(
        changed,
        "source-transform-1",
        "instance-1",
        "model-1");
    const auto changedSource = slicer_core::ComputeModelTransformHash(
        identity,
        "source-transform-2",
        "instance-1",
        "model-1");
    const auto missingSource = slicer_core::ComputeModelTransformHash(
        identity,
        "",
        "instance-1",
        "model-1");

    return ExpectTrue(first.IsValid(), "identity transform hash is valid")
        && ExpectTrue(first.hash.size() == 64U, "transform hash is SHA-256 hex")
        && ExpectTrue(first.hash == second.hash, "transform hash is stable")
        && ExpectTrue(
            first.hash == normalized.hash,
            "negative zero and 360 degree rotation normalize")
        && ExpectTrue(
            first.hash != changedTransform.hash,
            "transform change affects hash")
        && ExpectTrue(
            first.hash != changedSource.hash,
            "source identity affects hash")
        && ExpectTrue(
            !missingSource.IsValid()
                && missingSource.error->code
                    == slicer_core::ModelTransformErrorCode::SourceMissing,
            "hash rejects missing source identity");
}

bool RevisionChangesOnlyForEffectiveUpdates()
{
    slicer_core::ModelInstance instance = MakeInstance();

    const auto unchanged = slicer_core::UpdateModelInstanceTransform(
        instance,
        slicer_core::ModelTransform{},
        0U);
    if (!ExpectTrue(
            unchanged.IsValid() && !unchanged.changed
                && instance.transformrevision == 0U,
            "identity rewrite does not change revision"))
    {
        return false;
    }

    slicer_core::ModelTransform moved;
    moved.translatexmm = 2.5;
    const auto changed =
        slicer_core::UpdateModelInstanceTransform(instance, moved, 0U);
    if (!ExpectTrue(
            changed.IsValid() && changed.changed
                && instance.transformrevision == 1U,
            "effective transform increments revision"))
    {
        return false;
    }

    const auto repeated =
        slicer_core::UpdateModelInstanceTransform(instance, moved, 1U);
    if (!ExpectTrue(
            repeated.IsValid() && !repeated.changed
                && instance.transformrevision == 1U,
            "repeated transform keeps revision"))
    {
        return false;
    }

    const auto stale =
        slicer_core::UpdateModelInstanceTransform(instance, {}, 0U);
    if (!ExpectTrue(
            !stale.IsValid()
                && stale.error->code
                    == slicer_core::ModelTransformErrorCode::RevisionStale
                && instance.transformrevision == 1U,
            "stale revision is rejected without mutation"))
    {
        return false;
    }

    const auto reset =
        slicer_core::UpdateModelInstanceTransform(instance, {}, 1U);
    return ExpectTrue(
        reset.IsValid() && reset.changed && instance.transformrevision == 2U,
        "reset is an effective update");
}

bool IdentityAdapterDoesNotMutateSource()
{
    const slicer_core::SceneModel source = MakeScene();
    const slicer_core::SceneModel sourceBefore = source;
    const slicer_core::ModelInstance instance = MakeInstance();

    const slicer_core::TransformedModelResult result =
        slicer_core::AdaptTransformedModel(source, instance);
    if (!ExpectTrue(result.IsValid(), "identity adapter succeeds")
        || !ExpectTrue(
            result.geometry.triangles.size() == source.triangles.size(),
            "identity adapter preserves triangle count")
        || !ExpectTrue(
            BoundingBoxEqual(result.geometry.bboxmm, source.bbox_mm),
            "identity adapter preserves bbox"))
    {
        return false;
    }

    bool trianglesEqual = true;
    for (std::size_t index{0}; index < source.triangles.size(); ++index)
    {
        trianglesEqual = TriangleEqual(
                             result.geometry.triangles.at(index),
                             source.triangles.at(index))
            && TriangleEqual(
                source.triangles.at(index),
                sourceBefore.triangles.at(index))
            && trianglesEqual;
    }

    bool texturesEqual =
        result.geometry.triangletextures.size()
        == source.triangle_textures.size();
    for (std::size_t index{0U};
         texturesEqual && index < source.triangle_textures.size();
         ++index)
    {
        texturesEqual = TextureEqual(
            result.geometry.triangletextures.at(index),
            source.triangle_textures.at(index));
    }

    return ExpectTrue(trianglesEqual, "identity triangles and source stay unchanged")
        && ExpectTrue(texturesEqual, "identity adapter preserves texture attributes")
        && ExpectTrue(
            result.geometry.determinantsign == 1 && !result.geometry.mirrored,
            "identity determinant is positive");
}

bool TransformOrderPivotWindingAndUvAreCorrect()
{
    const slicer_core::SceneModel source = MakeScene();
    slicer_core::ModelInstance instance = MakeInstance();
    instance.transform.translatexmm = 10.0;
    instance.transform.translateymm = -3.0;
    instance.transform.rotatezdeg = 90.0;
    instance.transform.uniformscale = 2.0;
    instance.transform.mirrorx = true;
    instance.transformrevision = 7U;

    const slicer_core::TransformedModelResult result =
        slicer_core::AdaptTransformedModel(source, instance);
    if (!ExpectTrue(result.IsValid(), "composed transform succeeds"))
    {
        return false;
    }

    const slicer_core::Triangle& transformed =
        result.geometry.triangles.front();
    const slicer_core::TriangleTextureInfo& transformedTexture =
        result.geometry.triangletextures.front();

    return ExpectTrue(
               VecEqual(result.geometry.pivotmm, {1.0, 2.0, 2.0}),
               "pivot uses source bbox center and minZ")
        && ExpectTrue(
            VecEqual(transformed.a, {11.0, -3.0, 2.0}),
            "scale mirror rotate translate order is fixed")
        && ExpectTrue(
            VecEqual(transformed.b, {11.0, -1.0, 6.0}),
            "odd mirror swaps transformed c into b")
        && ExpectTrue(
            VecEqual(transformed.c, {9.0, -1.0, 2.0}),
            "odd mirror swaps transformed b into c")
        && ExpectTrue(
            result.geometry.determinantsign == -1
                && result.geometry.mirrored,
            "odd mirror reports negative determinant")
        && ExpectTrue(
            NearlyEqual(result.geometry.bboxmm.min.z, source.bbox_mm.min.z),
            "uniform scale preserves source minZ base")
        && ExpectTrue(
            transformedTexture.uv.at(1).u
                    == source.triangle_textures.front().uv.at(2).u
                && transformedTexture.uv.at(2).u
                    == source.triangle_textures.front().uv.at(1).u,
            "UV corners follow winding swap")
        && ExpectTrue(
            result.geometry.transformrevision == 7U,
            "adapter preserves transform revision");
}

bool IndividualTransformCasesAreCorrect()
{
    const slicer_core::SceneModel source = MakeScene();

    slicer_core::ModelInstance translated = MakeInstance();
    translated.transform.translatexmm = 3.0;
    translated.transform.translateymm = -2.0;
    const auto translatedResult =
        slicer_core::AdaptTransformedModel(source, translated);

    slicer_core::ModelInstance rotated = MakeInstance();
    rotated.transform.rotatezdeg = 180.0;
    const auto rotatedResult =
        slicer_core::AdaptTransformedModel(source, rotated);

    slicer_core::ModelInstance fullRotation = MakeInstance();
    fullRotation.transform.rotatezdeg = 360.0;
    const auto fullRotationResult =
        slicer_core::AdaptTransformedModel(source, fullRotation);

    slicer_core::ModelInstance scaled = MakeInstance();
    scaled.transform.uniformscale = 0.5;
    const auto scaledResult =
        slicer_core::AdaptTransformedModel(source, scaled);

    slicer_core::ModelInstance mirroredY = MakeInstance();
    mirroredY.transform.mirrory = true;
    const auto mirroredYResult =
        slicer_core::AdaptTransformedModel(source, mirroredY);

    return ExpectTrue(
               translatedResult.IsValid()
                   && VecEqual(
                       translatedResult.geometry.triangles.front().a,
                       {5.0, 0.0, 2.0}),
               "XY translation is applied after the pivot transform")
        && ExpectTrue(
            rotatedResult.IsValid()
                && VecEqual(
                    rotatedResult.geometry.triangles.front().a,
                    {0.0, 2.0, 2.0}),
            "180 degree rotation uses the fixed pivot")
        && ExpectTrue(
            fullRotationResult.IsValid()
                && TriangleEqual(
                    fullRotationResult.geometry.triangles.front(),
                    source.triangles.front()),
            "360 degree rotation is identity")
        && ExpectTrue(
            scaledResult.IsValid()
                && NearlyEqual(scaledResult.geometry.bboxmm.min.z, 2.0)
                && NearlyEqual(scaledResult.geometry.bboxmm.max.z, 3.0),
            "uniform scale preserves minZ and scales height")
        && ExpectTrue(
            mirroredYResult.IsValid()
                && mirroredYResult.geometry.mirrored
                && VecEqual(
                    mirroredYResult.geometry.triangles.front().c,
                    {1.0, 1.0, 2.0}),
            "mirrorY reflects around the pivot and reverses winding");
}

bool DoubleMirrorKeepsWinding()
{
    const slicer_core::SceneModel source = MakeScene();
    slicer_core::ModelInstance instance = MakeInstance();
    instance.transform.mirrorx = true;
    instance.transform.mirrory = true;

    const slicer_core::TransformedModelResult result =
        slicer_core::AdaptTransformedModel(source, instance);
    return ExpectTrue(result.IsValid(), "double mirror succeeds")
        && ExpectTrue(
            result.geometry.determinantsign == 1
                && !result.geometry.mirrored,
            "double mirror determinant is positive")
        && ExpectTrue(
            result.geometry.triangletextures.front().uv.at(1).u
                == source.triangle_textures.front().uv.at(1).u,
            "double mirror keeps winding and UV order");
}

bool MissingSourceGeometryIsRejected()
{
    const slicer_core::SceneModel empty;
    const slicer_core::TransformedModelResult result =
        slicer_core::AdaptTransformedModel(empty, MakeInstance());
    return ExpectTrue(
        !result.IsValid()
            && result.error->code
                == slicer_core::ModelTransformErrorCode::SourceMissing,
        "missing source geometry returns stable error");
}

}  // namespace

int main()
{
    const bool ok = StableErrorsAndValidation()
        && TransformHashIsStableAndSensitive()
        && RevisionChangesOnlyForEffectiveUpdates()
        && IdentityAdapterDoesNotMutateSource()
        && TransformOrderPivotWindingAndUvAreCorrect()
        && IndividualTransformCasesAreCorrect()
        && DoubleMirrorKeepsWinding()
        && MissingSourceGeometryIsRejected();
    if (!ok)
    {
        return 1;
    }

    std::cout << "model_transform_unit_tests: PASS\n";
    return 0;
}
