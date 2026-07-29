#pragma once

#include "slicer_core/scene/ModelInstance.h"
#include "slicer_core/scene/SceneModel.h"
#include "slicer_core/texture_image.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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
    std::array<double, 3> zmm{};
    std::array<TexCoord, 3> uv{};
    int materialindex{-1};
    bool hasuv{false};
};

/**
 * @brief Display-only material resource retained by the +Z scene view.
 */
struct SceneViewMaterialAppearance
{
    std::string name;
    std::array<std::uint8_t, 3> diffusergb{66U, 144U, 139U};
    std::filesystem::path texturepath;
    bool hasdiffuse{false};
    bool hastexture{false};
    bool textureexists{false};
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
 * @brief Display-only +Z surface raster generated off the UI thread.
 */
struct SceneViewSurfacePreview
{
    int width{0};
    int height{0};
    std::vector<std::uint8_t> rgba;
    std::size_t texturedpixelcount{0U};
    std::size_t materialpixelcount{0U};
    std::string contenthash;

    /**
     * @brief Report whether the raster dimensions and bytes are usable.
     * @return True when RGBA storage exactly matches width and height.
     */
    bool IsValid() const;
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
    std::vector<SceneViewMaterialAppearance> materialappearances;
    SceneViewSurfacePreview surfacepreview;
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
    TextureSampleOptions textureoptions;
    SceneViewAdmissionStatus admissionstatus{
        SceneViewAdmissionStatus::Unknown};
    bool buildsurfacepreview{true};
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
 * @brief Recompute the stable hash after an approved geometry mutation.
 * @param geometry Scene-view geometry whose identity must be refreshed.
 */
void RefreshSceneViewGeometryHash(SceneViewGeometry& geometry);

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
