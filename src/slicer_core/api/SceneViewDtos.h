#pragma once

#include "slicer_core/api/CommonDtos.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace slicer_core::api
{

enum class ViewMode
{
    Top,
    ThreeD
};

enum class TextureStatus
{
    Available,
    NotProvided
};

enum class ViewLod
{
    Auto,
    Lod0,
    Lod1,
    Lod2,
    OutlineOnly
};

enum class MeshTransform
{
    Local,
    World
};

/** @brief Selectable payload categories for one ViewData query. */
enum class ViewContent
{
    Bbox,
    Outline,
    SurfacePreview,
    Mesh,
    Appearance
};

/** @brief Texture resolution policy for a ViewData query. */
enum class TexturePolicy
{
    RequireIfPresent
};

/** @brief RGBA8 top-view preview with straight alpha. */
struct SurfacePreview
{
    int width_px{0};
    int height_px{0};
    std::vector<std::uint8_t> rgba8;
    Bounds3d local_bounds_mm;
    std::string preview_identity;
    std::string appearance_identity;
};

/** @brief Material range within the indexed mesh. */
struct ViewSubmesh
{
    std::uint32_t first_index{0};
    std::uint32_t index_count{0};
    std::string material_id;
};

/** @brief Indexed triangle mesh used by the three-dimensional view. */
struct ViewMesh
{
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texcoord0;
    std::vector<std::uint32_t> indices;
    std::vector<ViewSubmesh> submeshes;
    std::string mesh_identity;
    ViewLod lod{ViewLod::Lod0};
    MeshTransform mesh_transform{MeshTransform::Local};
};

/** @brief Embedded or referenced sRGB texture payload. */
struct ViewTexture
{
    std::string texture_id;
    std::string texture_identity;
    int width_px{0};
    int height_px{0};
    std::vector<std::uint8_t> rgba8;
};

/** @brief Material-to-texture binding for textured views. */
struct ViewMaterial
{
    std::string material_id;
    std::array<float, 4> base_color{1.0F, 1.0F, 1.0F, 1.0F};
    std::string texture_id;
    std::array<float, 9> uv_transform{
        1.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 1.0F};
    int uv_set{0};
    bool double_sided{false};
};

/** @brief Reusable appearance resolved by an instance identity. */
struct ViewAppearance
{
    std::string appearance_identity;
    std::vector<ViewMaterial> materials;
    std::vector<ViewTexture> textures;
};

/** @brief One closed local-space outline loop. */
struct ViewOutline
{
    std::vector<std::array<double, 2>> points_mm;
};

/** @brief Per-instance view binding using local bounds plus world transform. */
struct ViewInstance
{
    std::string instance_id;
    ModelId model_id{0};
    Matrix4d world_matrix;
    Bounds3d local_bounds_mm;
    TextureStatus texture_status{TextureStatus::NotProvided};
    std::string mesh_identity;
    std::string appearance_identity;
    std::string preview_identity;
    std::vector<ViewOutline> outlines;
    std::optional<SurfacePreview> surface_preview;
    std::optional<ViewMesh> mesh;
};

/** @brief Textured scene data for top or three-dimensional rendering. */
struct SceneViewData
{
    ViewMode view_mode{ViewMode::Top};
    std::uint64_t scene_revision{0};
    std::string viewdata_identity;
    std::vector<ViewAppearance> appearances;
    std::vector<ViewInstance> instances;
    bool truncated{false};
    std::string truncation_reason;
};

/** @brief Request for bounded, cacheable scene view data. */
struct SceneViewDataRequest
{
    SceneId scene_id{0};
    std::vector<ViewContent> content{
        ViewContent::Bbox,
        ViewContent::Outline,
        ViewContent::SurfacePreview,
        ViewContent::Mesh,
        ViewContent::Appearance};
    ViewMode view_mode{ViewMode::Top};
    TexturePolicy texture_policy{TexturePolicy::RequireIfPresent};
    ViewLod lod{ViewLod::Auto};
    MeshTransform mesh_transform{MeshTransform::Local};
    std::uint64_t expected_scene_revision{0};
    std::vector<std::string> instance_ids;
    std::uint64_t max_bytes{0};
};

}  // namespace slicer_core::api
