#include "slicer_core/scene/MultiModelScene.h"

#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <set>
#include <stdexcept>
#include <utility>

namespace slicer_core
{
namespace
{

SceneValidationError MakeError(
    const SceneValidationErrorCode code,
    const MultiModelScene& scene,
    const std::string_view modelId,
    const std::string_view instanceId,
    const std::string_view field,
    const std::string_view message)
{
    SceneValidationError error;
    error.code = code;
    error.sceneid = scene.sceneid;
    error.modelid = modelId;
    error.instanceid = instanceId;
    error.field = field;
    error.message = message;
    return error;
}

std::string ResourceScopeKindValue(const ResourceScopeKind kind)
{
    switch (kind)
    {
    case ResourceScopeKind::ObjDirectory:
        return "obj_directory";
    case ResourceScopeKind::ThreeMfPackage:
        return "three_mf_package";
    case ResourceScopeKind::StlFile:
        return "stl_file";
    }
    throw std::runtime_error("unsupported resource scope kind");
}

ResourceScopeKind ParseResourceScopeKind(const std::string& value)
{
    if (value == "obj_directory")
    {
        return ResourceScopeKind::ObjDirectory;
    }
    if (value == "three_mf_package")
    {
        return ResourceScopeKind::ThreeMfPackage;
    }
    if (value == "stl_file")
    {
        return ResourceScopeKind::StlFile;
    }
    throw std::runtime_error("unsupported resource scope kind: " + value);
}

std::string BuildVolumeSourceValue(const BuildVolumeSource source)
{
    switch (source)
    {
    case BuildVolumeSource::Unresolved:
        return "unresolved";
    case BuildVolumeSource::DeviceProfile:
        return "device_profile";
    case BuildVolumeSource::Fixture:
        return "fixture";
    }
    throw std::runtime_error("unsupported build volume source");
}

BuildVolumeSource ParseBuildVolumeSource(const std::string& value)
{
    if (value == "unresolved")
    {
        return BuildVolumeSource::Unresolved;
    }
    if (value == "device_profile")
    {
        return BuildVolumeSource::DeviceProfile;
    }
    if (value == "fixture")
    {
        return BuildVolumeSource::Fixture;
    }
    throw std::runtime_error("unsupported build volume source: " + value);
}

std::string BuildVolumeOriginValue(const BuildVolumeOrigin origin)
{
    switch (origin)
    {
    case BuildVolumeOrigin::Unresolved:
        return "unresolved";
    case BuildVolumeOrigin::LowerLeft:
        return "lower_left";
    case BuildVolumeOrigin::Center:
        return "center";
    }
    throw std::runtime_error("unsupported build volume origin");
}

BuildVolumeOrigin ParseBuildVolumeOrigin(const std::string& value)
{
    if (value == "unresolved")
    {
        return BuildVolumeOrigin::Unresolved;
    }
    if (value == "lower_left")
    {
        return BuildVolumeOrigin::LowerLeft;
    }
    if (value == "center")
    {
        return BuildVolumeOrigin::Center;
    }
    throw std::runtime_error("unsupported build volume origin: " + value);
}

std::string AxisDirectionValue(
    const BuildVolumeAxisDirection direction)
{
    switch (direction)
    {
    case BuildVolumeAxisDirection::Unresolved:
        return "unresolved";
    case BuildVolumeAxisDirection::Positive:
        return "positive";
    case BuildVolumeAxisDirection::Negative:
        return "negative";
    }
    throw std::runtime_error("unsupported build volume axis direction");
}

BuildVolumeAxisDirection ParseAxisDirection(const std::string& value)
{
    if (value == "unresolved")
    {
        return BuildVolumeAxisDirection::Unresolved;
    }
    if (value == "positive")
    {
        return BuildVolumeAxisDirection::Positive;
    }
    if (value == "negative")
    {
        return BuildVolumeAxisDirection::Negative;
    }
    throw std::runtime_error(
        "unsupported build volume axis direction: " + value);
}

std::string AdmissionStatusValue(
    const SceneInstanceAdmissionStatus status)
{
    switch (status)
    {
    case SceneInstanceAdmissionStatus::Unknown:
        return "unknown";
    case SceneInstanceAdmissionStatus::Admitted:
        return "admitted";
    case SceneInstanceAdmissionStatus::Blocked:
        return "blocked";
    }
    throw std::runtime_error("unsupported scene admission status");
}

SceneInstanceAdmissionStatus ParseAdmissionStatus(
    const std::string& value)
{
    if (value == "unknown")
    {
        return SceneInstanceAdmissionStatus::Unknown;
    }
    if (value == "admitted")
    {
        return SceneInstanceAdmissionStatus::Admitted;
    }
    if (value == "blocked")
    {
        return SceneInstanceAdmissionStatus::Blocked;
    }
    throw std::runtime_error(
        "unsupported scene admission status: " + value);
}

double CanonicalSceneNumber(const double value)
{
    return value == 0.0 ? 0.0 : value;
}

Json SerializeVec3(const Vec3& value)
{
    return Json::object({
        {"x", CanonicalSceneNumber(value.x)},
        {"y", CanonicalSceneNumber(value.y)},
        {"z", CanonicalSceneNumber(value.z)},
    });
}

Vec3 DeserializeVec3(const Json& value)
{
    return {
        value.at("x").as_double(),
        value.at("y").as_double(),
        value.at("z").as_double(),
    };
}

Json SerializeBoundingBox(const BoundingBox& value)
{
    return Json::object({
        {"min", SerializeVec3(value.min)},
        {"max", SerializeVec3(value.max)},
    });
}

BoundingBox DeserializeBoundingBox(const Json& value)
{
    return {
        DeserializeVec3(value.at("min")),
        DeserializeVec3(value.at("max")),
    };
}

Json SerializeTransform(const ModelTransform& value)
{
    const ModelTransform normalized = NormalizeModelTransform(value);
    return Json::object({
        {"translateXMm", normalized.translatexmm},
        {"translateYMm", normalized.translateymm},
        {"rotateXDeg", normalized.rotatexdeg},
        {"rotateYDeg", normalized.rotateydeg},
        {"rotateZDeg", normalized.rotatezdeg},
        {"uniformScale", normalized.uniformscale},
        {"mirrorX", normalized.mirrorx},
        {"mirrorY", normalized.mirrory},
        {"landOnBuildPlate", normalized.landonbuildplate},
    });
}

ModelTransform DeserializeTransform(const Json& value)
{
    ModelTransform transform;
    transform.translatexmm = value.at("translateXMm").as_double();
    transform.translateymm = value.at("translateYMm").as_double();
    transform.rotatexdeg = value.value("rotateXDeg", 0.0);
    transform.rotateydeg = value.value("rotateYDeg", 0.0);
    transform.rotatezdeg = value.at("rotateZDeg").as_double();
    transform.uniformscale = value.at("uniformScale").as_double();
    transform.mirrorx = value.at("mirrorX").as_bool();
    transform.mirrory = value.at("mirrorY").as_bool();
    transform.landonbuildplate =
        value.value("landOnBuildPlate", false);
    return NormalizeModelTransform(transform);
}

Json SerializeBuildVolume(const SceneBuildVolume& value)
{
    Json::Object object{
        {"source", BuildVolumeSourceValue(value.source)},
        {"widthMm", value.widthmm.has_value()
                        ? Json(CanonicalSceneNumber(*value.widthmm))
                        : Json(nullptr)},
        {"heightMm", value.heightmm.has_value()
                         ? Json(CanonicalSceneNumber(*value.heightmm))
                         : Json(nullptr)},
        {"origin", BuildVolumeOriginValue(value.origin)},
        {"xDirection", AxisDirectionValue(value.xdirection)},
        {"yDirection", AxisDirectionValue(value.ydirection)},
        {"isFixture", value.isfixture},
    };
    if (value.zlimitmm.has_value())
    {
        object.emplace(
            "zLimitMm",
            CanonicalSceneNumber(*value.zlimitmm));
    }
    return Json(std::move(object));
}

std::optional<double> DeserializeOptionalDouble(const Json& value)
{
    if (value.is_null())
    {
        return std::nullopt;
    }
    return value.as_double();
}

std::uint64_t DeserializeRevision(const Json& value)
{
    constexpr double kMaxExactJsonInteger{9007199254740991.0};
    const double revision = value.as_double();
    if (!std::isfinite(revision)
        || revision < 0.0
        || std::floor(revision) != revision
        || revision > kMaxExactJsonInteger)
    {
        throw std::runtime_error(
            "scene revision must be a non-negative integer");
    }
    return static_cast<std::uint64_t>(revision);
}

SceneBuildVolume DeserializeBuildVolume(const Json& value)
{
    SceneBuildVolume volume;
    volume.source =
        ParseBuildVolumeSource(value.at("source").as_string());
    volume.widthmm =
        DeserializeOptionalDouble(value.at("widthMm"));
    volume.heightmm =
        DeserializeOptionalDouble(value.at("heightMm"));
    if (value.contains("zLimitMm"))
    {
        volume.zlimitmm =
            DeserializeOptionalDouble(value.at("zLimitMm"));
    }
    volume.origin =
        ParseBuildVolumeOrigin(value.at("origin").as_string());
    volume.xdirection =
        ParseAxisDirection(value.at("xDirection").as_string());
    volume.ydirection =
        ParseAxisDirection(value.at("yDirection").as_string());
    volume.isfixture = value.at("isFixture").as_bool();
    return volume;
}

Json SerializeLayout(const SceneLayout& value)
{
    return Json::object({
        {"policy", value.policy},
        {"maxColumns", value.maxcolumns},
        {"maxRows", value.maxrows},
        {"columnGapMm", CanonicalSceneNumber(value.columngapmm)},
        {"rowGapMm", CanonicalSceneNumber(value.rowgapmm)},
        {"spacingMode", value.spacingmode},
        {"order", value.order},
    });
}

SceneLayout DeserializeLayout(const Json& value)
{
    SceneLayout layout;
    layout.policy = value.at("policy").as_string();
    layout.maxcolumns = value.at("maxColumns").as_int();
    layout.maxrows = value.at("maxRows").as_int();
    layout.columngapmm = value.at("columnGapMm").as_double();
    layout.rowgapmm = value.at("rowGapMm").as_double();
    layout.spacingmode = value.at("spacingMode").as_string();
    layout.order = value.at("order").as_string();
    return layout;
}

Json SerializeResourceScope(const ResourceScope& value)
{
    return Json::object({
        {"resourceScopeId", value.resourcescopeid},
        {"kind", ResourceScopeKindValue(value.kind)},
        {"rootPath", value.rootpath.generic_string()},
        {"packagePath", value.packagepath.generic_string()},
        {"partIdentity", value.partidentity},
    });
}

ResourceScope DeserializeResourceScope(const Json& value)
{
    ResourceScope scope;
    scope.resourcescopeid =
        value.at("resourceScopeId").as_string();
    scope.kind = ParseResourceScopeKind(value.at("kind").as_string());
    scope.rootpath = value.at("rootPath").as_string();
    scope.packagepath = value.at("packagePath").as_string();
    scope.partidentity = value.at("partIdentity").as_string();
    return scope;
}

Json SerializeModelSource(const ModelSource& value)
{
    return Json::object({
        {"modelId", value.modelid},
        {"sourcePath", value.sourcepath.generic_string()},
        {"format", value.format},
        {"resourceScopeId", value.resourcescopeid},
        {"sourceHash", value.sourcehash},
        {"resourceHash", value.resourcehash},
        {"displayName", value.displayname},
    });
}

ModelSource DeserializeModelSource(const Json& value)
{
    ModelSource source;
    source.modelid = value.at("modelId").as_string();
    source.sourcepath = value.at("sourcePath").as_string();
    source.format = value.at("format").as_string();
    source.resourcescopeid =
        value.at("resourceScopeId").as_string();
    source.sourcehash = value.at("sourceHash").as_string();
    source.resourcehash = value.at("resourceHash").as_string();
    source.displayname = value.at("displayName").as_string();
    return source;
}

Json SerializeSceneInstance(const SceneModelInstance& value)
{
    return Json::object({
        {"instanceId", value.instance.instanceid},
        {"modelId", value.instance.modelid},
        {"sourceTransformIdentity",
         value.instance.sourcetransformidentity},
        {"requestedTransform",
         SerializeTransform(value.requestedtransform)},
        {"derivedLayoutTransform",
         SerializeTransform(value.derivedlayouttransform)},
        {"effectiveTransform",
         SerializeTransform(value.effectivetransform)},
        {"visible", value.instance.visible},
        {"locked", value.instance.locked},
        {"transformRevision",
         static_cast<double>(value.instance.transformrevision)},
        {"sourceBboxMm",
         SerializeBoundingBox(value.instance.sourcebboxmm)},
        {"effectiveBboxMm",
         SerializeBoundingBox(value.instance.effectivebboxmm)},
        {"admissionStatus",
         AdmissionStatusValue(value.admissionstatus)},
        {"resolvedProfileId", value.resolvedprofileid},
    });
}

SceneModelInstance DeserializeSceneInstance(const Json& value)
{
    SceneModelInstance instance;
    instance.instance.instanceid =
        value.at("instanceId").as_string();
    instance.instance.modelid = value.at("modelId").as_string();
    instance.instance.sourcetransformidentity =
        value.at("sourceTransformIdentity").as_string();
    instance.requestedtransform =
        DeserializeTransform(value.at("requestedTransform"));
    instance.derivedlayouttransform =
        DeserializeTransform(value.at("derivedLayoutTransform"));
    instance.effectivetransform =
        DeserializeTransform(value.at("effectiveTransform"));
    instance.instance.transform = instance.effectivetransform;
    instance.instance.visible = value.at("visible").as_bool();
    instance.instance.locked = value.at("locked").as_bool();
    instance.instance.transformrevision =
        DeserializeRevision(value.at("transformRevision"));
    instance.instance.sourcebboxmm =
        DeserializeBoundingBox(value.at("sourceBboxMm"));
    instance.instance.effectivebboxmm =
        DeserializeBoundingBox(value.at("effectiveBboxMm"));
    instance.admissionstatus =
        ParseAdmissionStatus(value.at("admissionStatus").as_string());
    instance.resolvedprofileid =
        value.at("resolvedProfileId").as_string();
    return instance;
}

bool PathIsWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate)
{
    if (root.empty() || candidate.empty())
    {
        return false;
    }

    const std::filesystem::path normalizedRoot =
        root.lexically_normal();
    const std::filesystem::path normalizedCandidate =
        candidate.lexically_normal();
    const std::filesystem::path relative =
        normalizedCandidate.lexically_relative(normalizedRoot);
    if (relative.empty())
    {
        return normalizedCandidate == normalizedRoot;
    }
    if (relative.is_absolute())
    {
        return false;
    }
    for (const auto& component : relative)
    {
        if (component == "..")
        {
            return false;
        }
    }
    return true;
}

const ResourceScope* FindResourceScope(
    const MultiModelScene& scene,
    const std::string& scopeId)
{
    const auto iterator = std::find_if(
        scene.resourcescopes.begin(),
        scene.resourcescopes.end(),
        [&scopeId](const ResourceScope& scope)
        {
            return scope.resourcescopeid == scopeId;
        });
    return iterator == scene.resourcescopes.end()
        ? nullptr
        : &*iterator;
}

bool HasResolvedBuildVolume(const SceneBuildVolume& volume)
{
    return volume.widthmm.has_value()
        && volume.heightmm.has_value()
        && std::isfinite(*volume.widthmm)
        && std::isfinite(*volume.heightmm)
        && *volume.widthmm > 0.0
        && *volume.heightmm > 0.0
        && volume.origin != BuildVolumeOrigin::Unresolved
        && volume.xdirection
            != BuildVolumeAxisDirection::Unresolved
        && volume.ydirection
            != BuildVolumeAxisDirection::Unresolved;
}

bool RevisionIsJsonExact(const std::uint64_t revision)
{
    constexpr std::uint64_t kMaxExactJsonInteger{
        9007199254740991ULL};
    return revision <= kMaxExactJsonInteger;
}

bool TransformsApproximatelyEquivalent(
    const ModelTransform& left,
    const ModelTransform& right)
{
    constexpr double kTolerance{1.0e-9};
    const ModelTransform normalizedLeft = NormalizeModelTransform(left);
    const ModelTransform normalizedRight = NormalizeModelTransform(right);
    const auto close =
        [](const double first, const double second)
    {
        const double scale = std::max(
            {1.0, std::abs(first), std::abs(second)});
        return std::abs(first - second) <= kTolerance * scale;
    };

    const auto buildLinear =
        [](const ModelTransform& transform)
    {
        const double radiansX =
            transform.rotatexdeg
            * std::numbers::pi_v<double> / 180.0;
        const double radiansY =
            transform.rotateydeg
            * std::numbers::pi_v<double> / 180.0;
        const double radiansZ =
            transform.rotatezdeg
            * std::numbers::pi_v<double> / 180.0;
        const double cosineX = std::cos(radiansX);
        const double sineX = std::sin(radiansX);
        const double cosineY = std::cos(radiansY);
        const double sineY = std::sin(radiansY);
        const double cosineZ = std::cos(radiansZ);
        const double sineZ = std::sin(radiansZ);
        const double mirrorX = transform.mirrorx ? -1.0 : 1.0;
        const double mirrorY = transform.mirrory ? -1.0 : 1.0;
        const double scale = transform.uniformscale;
        return std::array<double, 9>{
            cosineZ * cosineY * mirrorX * scale,
            (cosineZ * sineY * sineX - sineZ * cosineX)
                * mirrorY * scale,
            (cosineZ * sineY * cosineX + sineZ * sineX) * scale,
            sineZ * cosineY * mirrorX * scale,
            (sineZ * sineY * sineX + cosineZ * cosineX)
                * mirrorY * scale,
            (sineZ * sineY * cosineX - cosineZ * sineX) * scale,
            -sineY * mirrorX * scale,
            cosineY * sineX * mirrorY * scale,
            cosineY * cosineX * scale,
        };
    };
    const std::array<double, 9> leftLinear =
        buildLinear(normalizedLeft);
    const std::array<double, 9> rightLinear =
        buildLinear(normalizedRight);
    if (normalizedLeft.landonbuildplate
            != normalizedRight.landonbuildplate
        || !close(
            normalizedLeft.translatexmm,
            normalizedRight.translatexmm)
        || !close(
            normalizedLeft.translateymm,
            normalizedRight.translateymm))
    {
        return false;
    }
    for (std::size_t index = 0U; index < leftLinear.size(); ++index)
    {
        if (!close(leftLinear.at(index), rightLinear.at(index)))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

bool SceneValidationResult::IsValid() const
{
    return errors.empty();
}

bool MultiModelSceneDecodeResult::IsValid() const
{
    return !error.has_value();
}

std::string_view MultiModelSceneSchemaName()
{
    return "slicesoft.multimodel_scene.13b.1";
}

std::string_view SceneValidationErrorCodeName(
    const SceneValidationErrorCode code)
{
    switch (code)
    {
    case SceneValidationErrorCode::None:
        return "NONE";
    case SceneValidationErrorCode::SchemaUnsupported:
        return "SCENE_SCHEMA_UNSUPPORTED";
    case SceneValidationErrorCode::SceneIdEmpty:
        return "SCENE_ID_EMPTY";
    case SceneValidationErrorCode::SceneRevisionStale:
        return "SCENE_REVISION_STALE";
    case SceneValidationErrorCode::SceneRevisionInvalid:
        return "SCENE_REVISION_INVALID";
    case SceneValidationErrorCode::ModelIdDuplicate:
        return "MODEL_ID_DUPLICATE";
    case SceneValidationErrorCode::ModelSourceInvalid:
        return "MODEL_SOURCE_INVALID";
    case SceneValidationErrorCode::InstanceIdDuplicate:
        return "INSTANCE_ID_DUPLICATE";
    case SceneValidationErrorCode::InstanceModelReferenceMissing:
        return "INSTANCE_MODEL_REFERENCE_MISSING";
    case SceneValidationErrorCode::InstanceTransformInvalid:
        return "INSTANCE_TRANSFORM_INVALID";
    case SceneValidationErrorCode::ResourceScopeEscape:
        return "RESOURCE_SCOPE_ESCAPE";
    case SceneValidationErrorCode::ResourceScopeMissing:
        return "RESOURCE_SCOPE_MISSING";
    case SceneValidationErrorCode::BuildVolumeUndefined:
        return "BUILD_VOLUME_UNDEFINED";
    case SceneValidationErrorCode::BuildVolumeFixtureNotProduction:
        return "BUILD_VOLUME_FIXTURE_NOT_PRODUCTION";
    case SceneValidationErrorCode::SceneProfileMismatch:
        return "SCENE_PROFILE_MISMATCH";
    case SceneValidationErrorCode::SceneLayoutInvalid:
        return "SCENE_LAYOUT_INVALID";
    case SceneValidationErrorCode::EffectiveConfigCancelled:
        return "SCENE_EFFECTIVE_CONFIG_CANCELLED";
    case SceneValidationErrorCode::EffectiveConfigIntegrityFailed:
        return "SCENE_EFFECTIVE_CONFIG_INTEGRITY_FAILED";
    case SceneValidationErrorCode::EffectiveConfigWriteFailed:
        return "SCENE_EFFECTIVE_CONFIG_WRITE_FAILED";
    }
    return "SCENE_VALIDATION_UNKNOWN";
}

SceneBuildVolume MakeDefaultDeviceBuildVolume()
{
    SceneBuildVolume volume;
    volume.source = BuildVolumeSource::DeviceProfile;
    volume.widthmm = kDefaultDeviceBuildVolumeWidthMm;
    volume.heightmm = kDefaultDeviceBuildVolumeHeightMm;
    volume.zlimitmm = kDefaultDeviceBuildVolumeZLimitMm;
    volume.origin = BuildVolumeOrigin::Center;
    volume.xdirection = BuildVolumeAxisDirection::Positive;
    volume.ydirection = BuildVolumeAxisDirection::Positive;
    volume.isfixture = false;
    return volume;
}

SceneValidationResult ValidateMultiModelScene(
    const MultiModelScene& scene,
    const SceneValidationPurpose purpose)
{
    SceneValidationResult result;
    if (scene.schema != MultiModelSceneSchemaName()
        || scene.subjecttype != "scene")
    {
        result.errors.push_back(
            MakeError(
                SceneValidationErrorCode::SchemaUnsupported,
                scene,
                {},
                {},
                "schema",
                "scene schema or subject type is unsupported"));
        return result;
    }
    if (scene.sceneid.empty())
    {
        result.errors.push_back(
            MakeError(
                SceneValidationErrorCode::SceneIdEmpty,
                scene,
                {},
                {},
                "sceneid",
                "scene id must not be empty"));
        return result;
    }
    if (!RevisionIsJsonExact(scene.scenerevision))
    {
        result.errors.push_back(
            MakeError(
                SceneValidationErrorCode::SceneRevisionInvalid,
                scene,
                {},
                {},
                "scenerevision",
                "scene revision exceeds exact JSON integer range"));
        return result;
    }

    if (scene.layout.policy != "grid"
        || scene.layout.maxcolumns < 1
        || scene.layout.maxcolumns > 11
        || scene.layout.maxrows < 1
        || scene.layout.maxrows > 2
        || !std::isfinite(scene.layout.columngapmm)
        || !std::isfinite(scene.layout.rowgapmm)
        || scene.layout.columngapmm < 0.0
        || scene.layout.rowgapmm < 0.0
        || scene.layout.spacingmode != "edge_clearance"
        || scene.layout.order != "row_major")
    {
        result.errors.push_back(
            MakeError(
                SceneValidationErrorCode::SceneLayoutInvalid,
                scene,
                {},
                {},
                "layout",
                "scene layout must satisfy the P0 11x2 grid contract"));
    }

    if (scene.buildvolume.zlimitmm.has_value()
        && (!std::isfinite(*scene.buildvolume.zlimitmm)
            || *scene.buildvolume.zlimitmm <= 0.0))
    {
        result.errors.push_back(
            MakeError(
                SceneValidationErrorCode::BuildVolumeUndefined,
                scene,
                {},
                {},
                "buildvolume.zlimitmm",
                "build volume Z limit must be finite and positive"));
    }

    std::set<std::string> scopeIds;
    for (const ResourceScope& scope : scene.resourcescopes)
    {
        if (scope.resourcescopeid.empty()
            || !scopeIds.insert(scope.resourcescopeid).second)
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::ResourceScopeMissing,
                    scene,
                    {},
                    {},
                    "resourcescopes",
                    "resource scope id must be non-empty and unique"));
        }
    }

    std::set<std::string> modelIds;
    for (const ModelSource& model : scene.models)
    {
        if (model.modelid.empty()
            || !modelIds.insert(model.modelid).second)
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::ModelIdDuplicate,
                    scene,
                    model.modelid,
                    {},
                    "modelid",
                    "model id must be non-empty and unique"));
            continue;
        }
        if (model.sourcepath.empty()
            || model.format.empty()
            || model.sourcehash.empty()
            || model.resourcehash.empty())
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::ModelSourceInvalid,
                    scene,
                    model.modelid,
                    {},
                    "modelsource",
                    "model source path, format, and hashes are required"));
            continue;
        }

        const ResourceScope* scope =
            FindResourceScope(scene, model.resourcescopeid);
        if (scope == nullptr)
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::ResourceScopeMissing,
                    scene,
                    model.modelid,
                    {},
                    "resourcescopeid",
                    "model resource scope is missing"));
            continue;
        }

        bool scopeValid{false};
        switch (scope->kind)
        {
        case ResourceScopeKind::ObjDirectory:
            scopeValid =
                PathIsWithin(scope->rootpath, model.sourcepath);
            break;
        case ResourceScopeKind::ThreeMfPackage:
            scopeValid =
                !scope->partidentity.empty()
                && model.sourcepath.lexically_normal()
                    == scope->packagepath.lexically_normal()
                && scope->rootpath.lexically_normal()
                    == scope->packagepath.lexically_normal();
            break;
        case ResourceScopeKind::StlFile:
            scopeValid =
                model.sourcepath.lexically_normal()
                == scope->rootpath.lexically_normal();
            break;
        }
        if (!scopeValid)
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::ResourceScopeEscape,
                    scene,
                    model.modelid,
                    {},
                    "sourcepath",
                    "model source escapes its resource scope"));
        }
    }

    std::set<std::string> instanceIds;
    for (const SceneModelInstance& sceneInstance : scene.instances)
    {
        const ModelInstance& instance = sceneInstance.instance;
        if (!RevisionIsJsonExact(instance.transformrevision))
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::SceneRevisionInvalid,
                    scene,
                    instance.modelid,
                    instance.instanceid,
                    "transformrevision",
                    "transform revision exceeds exact JSON integer range"));
            continue;
        }
        if (instance.instanceid.empty()
            || !instanceIds.insert(instance.instanceid).second)
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::InstanceIdDuplicate,
                    scene,
                    instance.modelid,
                    instance.instanceid,
                    "instanceid",
                    "instance id must be non-empty and unique"));
            continue;
        }
        if (!modelIds.contains(instance.modelid))
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::
                        InstanceModelReferenceMissing,
                    scene,
                    instance.modelid,
                    instance.instanceid,
                    "modelid",
                    "instance references an unknown model"));
        }

        const ModelTransformValidationResult requestedValidation =
            ValidateModelTransform(
                sceneInstance.requestedtransform,
                instance.instanceid,
                instance.modelid);
        const ModelTransformValidationResult derivedValidation =
            ValidateModelTransform(
                sceneInstance.derivedlayouttransform,
                instance.instanceid,
                instance.modelid);
        const ModelTransformValidationResult effectiveValidation =
            ValidateModelTransform(
                sceneInstance.effectivetransform,
                instance.instanceid,
                instance.modelid);
        if (!requestedValidation.IsValid()
            || !derivedValidation.IsValid()
            || !effectiveValidation.IsValid()
            || ValidateModelInstance(instance).has_value()
            || !TransformsApproximatelyEquivalent(
                ComposeModelTransforms(
                    sceneInstance.derivedlayouttransform,
                    sceneInstance.requestedtransform),
                sceneInstance.effectivetransform)
            || !TransformsApproximatelyEquivalent(
                instance.transform,
                sceneInstance.effectivetransform))
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::InstanceTransformInvalid,
                    scene,
                    instance.modelid,
                    instance.instanceid,
                    "effectivetransform",
                    "instance effective transform is invalid or stale"));
        }

        if (scene.materialbindingmode
                == SceneMaterialBindingMode::SceneProfileOnly
            && (scene.resolvedprofileid.empty()
                || sceneInstance.resolvedprofileid
                    != scene.resolvedprofileid))
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::SceneProfileMismatch,
                    scene,
                    instance.modelid,
                    instance.instanceid,
                    "resolvedprofileid",
                    "all P0 scene instances must share one Profile"));
        }

        if (scene.buildvolume.zlimitmm.has_value()
            && std::isfinite(*scene.buildvolume.zlimitmm)
            && *scene.buildvolume.zlimitmm > 0.0
            && std::isfinite(instance.effectivebboxmm.max.z)
            && instance.effectivebboxmm.max.z
                > *scene.buildvolume.zlimitmm)
        {
            result.warnings.push_back(
                "instance " + instance.instanceid
                + " exceeds buildVolume.zLimitMm");
        }
    }

    if (purpose != SceneValidationPurpose::Draft)
    {
        if (!HasResolvedBuildVolume(scene.buildvolume))
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::BuildVolumeUndefined,
                    scene,
                    {},
                    {},
                    "buildvolume",
                    "resolved build volume is required"));
        }
        else if (purpose == SceneValidationPurpose::FunctionalFixture)
        {
            if (scene.buildvolume.source != BuildVolumeSource::Fixture
                || !scene.buildvolume.isfixture)
            {
                result.errors.push_back(
                    MakeError(
                        SceneValidationErrorCode::BuildVolumeUndefined,
                        scene,
                        {},
                        {},
                        "buildvolume",
                        "functional fixture validation requires fixture provenance"));
            }
            else
            {
                result.warnings.push_back(
                    "fixture build volume is not device production evidence");
            }
        }
        else if (scene.buildvolume.source
                     != BuildVolumeSource::DeviceProfile
                 || scene.buildvolume.isfixture)
        {
            result.errors.push_back(
                MakeError(
                    SceneValidationErrorCode::
                        BuildVolumeFixtureNotProduction,
                    scene,
                    {},
                    {},
                    "buildvolume",
                    "fixture build volume cannot claim device production readiness"));
        }
    }
    return result;
}

std::optional<SceneValidationError> ValidateSceneRevision(
    const MultiModelScene& scene,
    const std::uint64_t expectedRevision)
{
    if (scene.scenerevision == expectedRevision)
    {
        return std::nullopt;
    }
    return MakeError(
        SceneValidationErrorCode::SceneRevisionStale,
        scene,
        {},
        {},
        "scenerevision",
        "scene revision is stale");
}

Json SerializeMultiModelScene(const MultiModelScene& scene)
{
    Json::Array scopes;
    scopes.reserve(scene.resourcescopes.size());
    for (const ResourceScope& scope : scene.resourcescopes)
    {
        scopes.push_back(SerializeResourceScope(scope));
    }

    Json::Array models;
    models.reserve(scene.models.size());
    for (const ModelSource& model : scene.models)
    {
        models.push_back(SerializeModelSource(model));
    }

    Json::Array instances;
    instances.reserve(scene.instances.size());
    for (const SceneModelInstance& instance : scene.instances)
    {
        instances.push_back(SerializeSceneInstance(instance));
    }

    return Json::object({
        {"schema", scene.schema},
        {"subjectType", scene.subjecttype},
        {"sceneId", scene.sceneid},
        {"sceneRevision", static_cast<double>(scene.scenerevision)},
        {"buildVolume", SerializeBuildVolume(scene.buildvolume)},
        {"layout", SerializeLayout(scene.layout)},
        {"materialBindingMode", "scene_profile_only"},
        {"resolvedProfileId", scene.resolvedprofileid},
        {"resourceScopes", Json(std::move(scopes))},
        {"models", Json(std::move(models))},
        {"instances", Json(std::move(instances))},
    });
}

MultiModelSceneDecodeResult DeserializeMultiModelScene(
    const Json& document)
{
    try
    {
        if (!document.is_object())
        {
            throw std::runtime_error("scene document must be an object");
        }
        MultiModelScene scene;
        scene.schema = document.at("schema").as_string();
        scene.subjecttype = document.at("subjectType").as_string();
        if (scene.schema != MultiModelSceneSchemaName()
            || scene.subjecttype != "scene")
        {
            return {
                {},
                MakeError(
                    SceneValidationErrorCode::SchemaUnsupported,
                    scene,
                    {},
                    {},
                    "schema",
                    "scene schema or subject type is unsupported")};
        }
        scene.sceneid = document.at("sceneId").as_string();
        scene.scenerevision =
            DeserializeRevision(document.at("sceneRevision"));
        scene.buildvolume =
            DeserializeBuildVolume(document.at("buildVolume"));
        scene.layout = DeserializeLayout(document.at("layout"));
        if (document.at("materialBindingMode").as_string()
            != "scene_profile_only")
        {
            throw std::runtime_error(
                "unsupported scene material binding mode");
        }
        scene.materialbindingmode =
            SceneMaterialBindingMode::SceneProfileOnly;
        scene.resolvedprofileid =
            document.at("resolvedProfileId").as_string();

        for (const Json& scope :
             document.at("resourceScopes").as_array())
        {
            scene.resourcescopes.push_back(
                DeserializeResourceScope(scope));
        }
        for (const Json& model : document.at("models").as_array())
        {
            scene.models.push_back(DeserializeModelSource(model));
        }
        for (const Json& instance :
             document.at("instances").as_array())
        {
            scene.instances.push_back(
                DeserializeSceneInstance(instance));
        }
        return {std::move(scene), std::nullopt};
    }
    catch (const std::exception& error)
    {
        MultiModelScene scene;
        return {
            {},
            MakeError(
                SceneValidationErrorCode::SchemaUnsupported,
                scene,
                {},
                {},
                "document",
                error.what())};
    }
}

std::string ComputeMultiModelSceneHash(const MultiModelScene& scene)
{
    return ComputeSha256(SerializeMultiModelScene(scene).dump(0));
}

MultiModelScene ProjectSingleModelScene(
    const std::string_view sceneId,
    const ModelSource& source,
    const ResourceScope& scope,
    const ModelInstance& instance,
    const std::string_view profileId)
{
    MultiModelScene scene;
    scene.sceneid = sceneId;
    scene.resolvedprofileid = profileId;
    scene.resourcescopes.push_back(scope);
    scene.models.push_back(source);

    SceneModelInstance sceneInstance;
    sceneInstance.instance = instance;
    sceneInstance.requestedtransform = instance.transform;
    sceneInstance.effectivetransform = instance.transform;
    sceneInstance.resolvedprofileid = profileId;
    scene.instances.push_back(std::move(sceneInstance));
    return scene;
}

}  // namespace slicer_core
