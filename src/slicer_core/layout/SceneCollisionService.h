#pragma once

#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneViewGeometry.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace slicer_core
{

/**
 * @brief Stable failures produced by scene build-volume and collision checks.
 */
enum class SceneCollisionErrorCode
{
    None,
    BuildVolumeUndefined,
    BuildVolumeInvalid,
    BuildVolumeFixtureNotProduction,
    InstanceBoundsInvalid,
    InstanceOutOfRange,
    InstanceAdmissionBlocked,
    ProjectionGeometryInvalid,
    InstanceOverlapBlocked,
    SceneRevisionStale,
};

/**
 * @brief Overall result state of one immutable scene admission evaluation.
 */
enum class SceneCollisionStatus
{
    Passed,
    Blocked,
};

/**
 * @brief Structured scene admission failure with stable instance identity.
 */
struct SceneCollisionError
{
    SceneCollisionErrorCode code{SceneCollisionErrorCode::None};
    std::string sceneid;
    std::string modelid;
    std::string instanceid;
    std::string otherinstanceid;
    std::string field;
    std::string message;
};

/**
 * @brief One immutable scene item used by build-volume and collision checks.
 */
struct SceneCollisionItem
{
    ModelInstance instance;
    SceneInstanceAdmissionStatus admissionstatus{
        SceneInstanceAdmissionStatus::Unknown};
    std::optional<SceneViewGeometry> geometry;
};

/**
 * @brief Complete input for one deterministic scene admission evaluation.
 */
struct SceneCollisionRequest
{
    std::string sceneid;
    std::uint64_t currentscenerevision{0U};
    std::uint64_t expectedscenerevision{0U};
    SceneValidationPurpose purpose{SceneValidationPurpose::Draft};
    SceneBuildVolume buildvolume;
    double contactepsilonmm{0.0};
    std::vector<SceneCollisionItem> items;
};

/**
 * @brief Per-instance build-volume, admission, and collision evidence.
 */
struct SceneCollisionInstanceResult
{
    std::string modelid;
    std::string instanceid;
    std::uint64_t transformrevision{0U};
    std::string transformhash;
    bool visible{true};
    bool skippedhidden{false};
    SceneInstanceAdmissionStatus admissionstatus{
        SceneInstanceAdmissionStatus::Unknown};
    bool boundsvalid{false};
    bool inbounds{false};
    std::vector<std::string> collisionids;
    std::vector<SceneCollisionError> errors;
};

/**
 * @brief One precise positive-area projected collision pair.
 */
struct SceneCollisionPair
{
    std::string firstinstanceid;
    std::string secondinstanceid;
};

/**
 * @brief Deterministic counters for the two-stage collision evaluation.
 */
struct SceneCollisionStatistics
{
    std::size_t totalinstancecount{0U};
    std::size_t visibleinstancecount{0U};
    std::size_t hiddeninstancecount{0U};
    std::size_t aabbcandidatepaircount{0U};
    std::size_t exacttestedpaircount{0U};
    std::size_t collisionpaircount{0U};
};

/**
 * @brief Complete immutable output of scene collision admission.
 */
struct SceneCollisionResult
{
    std::string sceneid;
    std::uint64_t sourcescenerevision{0U};
    SceneValidationPurpose purpose{SceneValidationPurpose::Draft};
    SceneBuildVolume buildvolume;
    double contactepsilonmm{0.0};
    SceneCollisionStatus scenestatus{SceneCollisionStatus::Blocked};
    std::vector<SceneCollisionInstanceResult> instances;
    std::vector<SceneCollisionPair> collisionpairs;
    std::vector<SceneCollisionError> errors;
    std::optional<SceneViewBounds> sceneboundsmm;
    SceneCollisionStatistics statistics;
    bool functionalallowed{false};
    bool productionallowed{false};

    /**
     * @brief Report whether the requested admission purpose passed.
     * @return True when no blocking scene or instance error exists.
     */
    bool IsValid() const;
};

/**
 * @brief Return the stable protocol name for a scene collision error.
 * @param code Scene collision error code.
 * @return Stable ASCII error name.
 */
std::string_view SceneCollisionErrorCodeName(
    SceneCollisionErrorCode code);

/**
 * @brief Evaluate build-volume, per-instance admission, and XY collisions.
 * @param request Immutable scene, revision, volume, geometry, and purpose.
 * @return Deterministic complete result without mutating the request.
 */
SceneCollisionResult EvaluateSceneCollisionAdmission(
    const SceneCollisionRequest& request);

}  // namespace slicer_core
