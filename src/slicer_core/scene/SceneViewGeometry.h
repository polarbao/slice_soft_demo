#pragma once

#include "slicer_core/scene/ModelInstance.h"
#include "slicer_core/scene/SceneModel.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace slicer_core
{

/**
 * @brief Admission state displayed by the read-only scene view.
 */
enum class SceneViewAdmissionStatus
{
    Unknown,
    Admitted,
    Blocked,
};

/**
 * @brief Stable failures produced while building scene-view geometry.
 */
enum class SceneViewGeometryErrorCode
{
    None,
    SceneIdEmpty,
    ModelIdEmpty,
    InstanceIdEmpty,
    RevisionStale,
    SourceGeometryInvalid,
    GeometryNonFinite,
    TransformInvalid,
};

/**
 * @brief One point in the +Z projected XY plane, in millimeters.
 */
struct SceneViewPoint
{
    double xmm{0.0};
    double ymm{0.0};
};

/**
 * @brief One triangle projected from transformed model geometry.
 */
struct SceneViewTriangle
{
    SceneViewPoint a;
    SceneViewPoint b;
    SceneViewPoint c;
};

/**
 * @brief Axis-aligned XY bounds for scene-view geometry.
 */
struct SceneViewBounds
{
    SceneViewPoint min;
    SceneViewPoint max;
};

/**
 * @brief Immutable projected geometry and its scene identity.
 */
struct SceneViewGeometry
{
    std::string sceneid;
    std::string modelid;
    std::string instanceid;
    std::uint64_t scenerevision{0U};
    std::uint64_t transformrevision{0U};
    std::vector<SceneViewTriangle> triangles;
    BoundingBox sourcebboxmm;
    BoundingBox effectivebboxmm;
    SceneViewBounds worldboundsmm;
    bool visible{true};
    bool locked{false};
    SceneViewAdmissionStatus admissionstatus{
        SceneViewAdmissionStatus::Unknown};
    std::size_t sourcetrianglecount{0U};
    std::size_t texturedtrianglecount{0U};
    std::size_t materialcount{0U};
    bool hastexturecoordinates{false};
    std::string geometryhash;
    std::string transformhash;
};

/**
 * @brief Request used to build one scene instance's read-only top view.
 */
struct SceneViewGeometryRequest
{
    std::string sceneid;
    std::uint64_t scenerevision{0U};
    std::optional<std::uint64_t> expectedscenerevision;
    std::optional<std::uint64_t> expectedtransformrevision;
    ModelInstance instance;
    SceneViewAdmissionStatus admissionstatus{
        SceneViewAdmissionStatus::Unknown};
};

/**
 * @brief Structured scene-view geometry failure.
 */
struct SceneViewGeometryError
{
    SceneViewGeometryErrorCode code{SceneViewGeometryErrorCode::None};
    std::string sceneid;
    std::string modelid;
    std::string instanceid;
    std::string field;
    std::string message;
};

/**
 * @brief Result of building projected scene-view geometry.
 */
struct SceneViewGeometryResult
{
    SceneViewGeometry geometry;
    std::optional<SceneViewGeometryError> error;

    /**
     * @brief Report whether projected geometry was produced.
     * @return True when no validation or projection error is present.
     */
    bool IsValid() const;
};

/**
 * @brief Return the stable protocol name for a scene-view error.
 * @param code Scene-view geometry error code.
 * @return Stable ASCII error name.
 */
std::string_view SceneViewGeometryErrorCodeName(
    SceneViewGeometryErrorCode code);

/**
 * @brief Build immutable +Z projected geometry for one transformed instance.
 * @param source Source-transformed model; the function never mutates it.
 * @param request Scene identity, instance transform, revision, and admission.
 * @return Projected triangles and stable identity, or a structured error.
 */
SceneViewGeometryResult BuildSceneViewGeometry(
    const SceneModel& source,
    const SceneViewGeometryRequest& request);

}  // namespace slicer_core
