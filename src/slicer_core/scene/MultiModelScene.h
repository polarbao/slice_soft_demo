#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/scene/ModelInstance.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace slicer_core
{

/**
 * @brief Default edge clearance between adjacent scene columns in millimetres.
 */
inline constexpr double kDefaultSceneColumnGapMm{10.0};

/**
 * @brief Default edge clearance between adjacent scene rows in millimetres.
 */
inline constexpr double kDefaultSceneRowGapMm{10.0};

/**
 * @brief Default clearance from the lower-left build-volume boundary.
 */
inline constexpr double kDefaultSceneBoundaryMarginMm{10.0};

/**
 * @brief Default device build-volume width along X in millimetres.
 */
inline constexpr double kDefaultDeviceBuildVolumeWidthMm{230.0};

/**
 * @brief Default device build-volume height along Y in millimetres.
 */
inline constexpr double kDefaultDeviceBuildVolumeHeightMm{100.0};

/**
 * @brief Default device build-volume limit along Z in millimetres.
 */
inline constexpr double kDefaultDeviceBuildVolumeZLimitMm{60.0};

/**
 * @brief Storage scope used to resolve one model's adjacent resources.
 */
enum class ResourceScopeKind
{
    ObjDirectory,
    ThreeMfPackage,
    StlFile,
};

/**
 * @brief Source of the printable XYZ build-volume limits.
 */
enum class BuildVolumeSource
{
    Unresolved,
    DeviceProfile,
    Fixture,
};

/**
 * @brief Origin used by a scene build volume.
 */
enum class BuildVolumeOrigin
{
    Unresolved,
    LowerLeft,
    Center,
};

/**
 * @brief Direction of a build-volume axis relative to the scene view.
 */
enum class BuildVolumeAxisDirection
{
    Unresolved,
    Positive,
    Negative,
};

/**
 * @brief P0 material binding policy for all instances in a scene.
 */
enum class SceneMaterialBindingMode
{
    SceneProfileOnly,
};

/**
 * @brief Admission state carried by an editable scene instance.
 */
enum class SceneInstanceAdmissionStatus
{
    Unknown,
    Admitted,
    Blocked,
};

/**
 * @brief Validation intent used to separate draft, fixture, and production.
 */
enum class SceneValidationPurpose
{
    Draft,
    FunctionalFixture,
    Production,
};

/**
 * @brief Stable scene schema and effective-config errors.
 */
enum class SceneValidationErrorCode
{
    None,
    SchemaUnsupported,
    SceneIdEmpty,
    SceneRevisionStale,
    SceneRevisionInvalid,
    ModelIdDuplicate,
    ModelSourceInvalid,
    InstanceIdDuplicate,
    InstanceModelReferenceMissing,
    InstanceTransformInvalid,
    ResourceScopeEscape,
    ResourceScopeMissing,
    BuildVolumeUndefined,
    BuildVolumeFixtureNotProduction,
    SceneProfileMismatch,
    SceneLayoutInvalid,
    EffectiveConfigCancelled,
    EffectiveConfigIntegrityFailed,
    EffectiveConfigWriteFailed,
};

/**
 * @brief Explicit resource-resolution boundary for OBJ, 3MF, or STL.
 */
struct ResourceScope
{
    std::string resourcescopeid;
    ResourceScopeKind kind{ResourceScopeKind::ObjDirectory};
    std::filesystem::path rootpath;
    std::filesystem::path packagepath;
    std::string partidentity;
};

/**
 * @brief Stable source model identity and resource hashes.
 */
struct ModelSource
{
    std::string modelid;
    std::filesystem::path sourcepath;
    std::string format;
    std::string resourcescopeid;
    std::string sourcehash;
    std::string resourcehash;
    std::string displayname;
};

/**
 * @brief Optional printable XYZ limits and their provenance.
 */
struct SceneBuildVolume
{
    BuildVolumeSource source{BuildVolumeSource::Unresolved};
    std::optional<double> widthmm;
    std::optional<double> heightmm;
    std::optional<double> zlimitmm;
    BuildVolumeOrigin origin{BuildVolumeOrigin::Unresolved};
    BuildVolumeAxisDirection xdirection{
        BuildVolumeAxisDirection::Unresolved};
    BuildVolumeAxisDirection ydirection{
        BuildVolumeAxisDirection::Unresolved};
    bool isfixture{false};
};

/**
 * @brief Deterministic P0 row-major layout request.
 */
struct SceneLayout
{
    std::string policy{"grid"};
    int maxcolumns{11};
    int maxrows{2};
    double columngapmm{kDefaultSceneColumnGapMm};
    double rowgapmm{kDefaultSceneRowGapMm};
    std::string spacingmode{"edge_clearance"};
    std::string order{"row_major"};
};

/**
 * @brief Scene instance with requested, derived, and effective transforms.
 */
struct SceneModelInstance
{
    ModelInstance instance;
    ModelTransform requestedtransform;
    ModelTransform derivedlayouttransform;
    ModelTransform effectivetransform;
    SceneInstanceAdmissionStatus admissionstatus{
        SceneInstanceAdmissionStatus::Unknown};
    std::string resolvedprofileid;
};

/**
 * @brief Multi-model scene draft and its stable identities.
 */
struct MultiModelScene
{
    std::string schema{"slicesoft.multimodel_scene.13b.1"};
    std::string subjecttype{"scene"};
    std::string sceneid;
    std::uint64_t scenerevision{0U};
    SceneBuildVolume buildvolume;
    SceneLayout layout;
    SceneMaterialBindingMode materialbindingmode{
        SceneMaterialBindingMode::SceneProfileOnly};
    std::string resolvedprofileid;
    std::vector<ResourceScope> resourcescopes;
    std::vector<ModelSource> models;
    std::vector<SceneModelInstance> instances;
};

/**
 * @brief Structured scene validation failure.
 */
struct SceneValidationError
{
    SceneValidationErrorCode code{SceneValidationErrorCode::None};
    std::string sceneid;
    std::string modelid;
    std::string instanceid;
    std::string field;
    std::string message;
};

/**
 * @brief Validation result containing every deterministic scene failure.
 */
struct SceneValidationResult
{
    std::vector<SceneValidationError> errors;
    std::vector<std::string> warnings;

    /**
     * @brief Report whether scene validation succeeded.
     * @return True when no blocking errors exist.
     */
    bool IsValid() const;
};

/**
 * @brief Result of decoding one multi-model scene document.
 */
struct MultiModelSceneDecodeResult
{
    MultiModelScene scene;
    std::optional<SceneValidationError> error;

    /**
     * @brief Report whether scene decoding succeeded.
     * @return True when no schema or JSON error exists.
     */
    bool IsValid() const;
};

/**
 * @brief Return the canonical scene schema name.
 * @return Stable scene schema string.
 */
std::string_view MultiModelSceneSchemaName();

/**
 * @brief Return the stable protocol name for a scene error.
 * @param code Scene validation error code.
 * @return Stable ASCII error name.
 */
std::string_view SceneValidationErrorCodeName(
    SceneValidationErrorCode code);

/**
 * @brief Create the default 230 x 100 x 60 mm device build volume.
 * @return Explicit device-profile build volume with production provenance.
 */
SceneBuildVolume MakeDefaultDeviceBuildVolume();

/**
 * @brief Validate identities, resources, transforms, Profile, and volume.
 * @param scene Scene to validate.
 * @param purpose Draft, fixture-functional, or device-production intent.
 * @return Deterministic validation errors and non-blocking warnings.
 */
SceneValidationResult ValidateMultiModelScene(
    const MultiModelScene& scene,
    SceneValidationPurpose purpose);

/**
 * @brief Validate a caller-observed scene revision before a transaction.
 * @param scene Current scene.
 * @param expectedRevision Revision observed by the caller.
 * @return Stable stale error when revisions differ.
 */
std::optional<SceneValidationError> ValidateSceneRevision(
    const MultiModelScene& scene,
    std::uint64_t expectedRevision);

/**
 * @brief Serialize a scene into its canonical JSON schema.
 * @param scene Scene to serialize.
 * @return Canonically ordered JSON object.
 */
Json SerializeMultiModelScene(const MultiModelScene& scene);

/**
 * @brief Decode a canonical multi-model scene.
 * @param document JSON document to decode.
 * @return Scene or a stable schema/JSON error.
 */
MultiModelSceneDecodeResult DeserializeMultiModelScene(
    const Json& document);

/**
 * @brief Compute the stable hash of a scene document.
 * @param scene Scene to hash.
 * @return Lowercase SHA-256 hash of canonical scene JSON.
 */
std::string ComputeMultiModelSceneHash(const MultiModelScene& scene);

/**
 * @brief Project one legacy single-model input into an identity scene draft.
 * @param sceneId Stable temporary scene identity.
 * @param source Source model identity.
 * @param scope Model resource scope.
 * @param instance Identity or existing model instance.
 * @param profileId Scene-wide resolved Profile.
 * @return One-model, one-instance scene draft.
 */
MultiModelScene ProjectSingleModelScene(
    std::string_view sceneId,
    const ModelSource& source,
    const ResourceScope& scope,
    const ModelInstance& instance,
    std::string_view profileId);

}  // namespace slicer_core
