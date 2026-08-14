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

/** @brief 选择序列化网格属性使用的标量编码。 */
enum class MeshAttributeFormat
{
    Float32,
    Float16
};

/** @brief 一次 ViewData 查询可选择的载荷类别。 */
enum class ViewContent
{
    Bbox,
    Outline,
    SurfacePreview,
    Mesh,
    Appearance
};

/** @brief ViewData 查询的纹理分辨率策略。 */
enum class TexturePolicy
{
    RequireIfPresent
};

/** @brief 使用直通 Alpha 的 RGBA8 俯视预览。 */
struct SurfacePreview
{
    int width_px{0};
    int height_px{0};
    std::vector<std::uint8_t> rgba8;
    Bounds3d local_bounds_mm;
    std::string preview_identity;
    std::string appearance_identity;
};

/** @brief 索引网格中的材质范围。 */
struct ViewSubmesh
{
    std::uint32_t first_index{0};
    std::uint32_t index_count{0};
    std::string material_id;
};

/** @brief 三维视图使用的索引三角网格。 */
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
    MeshAttributeFormat attribute_format{MeshAttributeFormat::Float32};
};

/** @brief 内嵌或引用的 sRGB 纹理载荷。 */
struct ViewTexture
{
    std::string texture_id;
    std::string texture_identity;
    int width_px{0};
    int height_px{0};
    std::vector<std::uint8_t> rgba8;
};

/** @brief 纹理视图的材质到纹理绑定。 */
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

/** @brief 由实例标识解析出的可复用外观。 */
struct ViewAppearance
{
    std::string appearance_identity;
    std::vector<ViewMaterial> materials;
    std::vector<ViewTexture> textures;
};

/** @brief 一条闭合的局部空间轮廓环。 */
struct ViewOutline
{
    std::vector<std::array<double, 2>> points_mm;
};

/** @brief 使用局部边界和世界变换的逐实例视图绑定。 */
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
    /** @brief 已弃用的内联兼容载荷；规范数据使用 SceneViewData::meshes。 */
    std::optional<ViewMesh> mesh;
};

/** @brief 用于俯视或三维渲染的纹理场景数据。 */
struct SceneViewData
{
    ViewMode view_mode{ViewMode::Top};
    std::uint64_t scene_revision{0};
    std::string viewdata_identity;
    std::vector<ViewAppearance> appearances;
    /** @brief 以 ViewMesh::mesh_identity 为键的可复用网格。 */
    std::vector<ViewMesh> meshes;
    std::vector<ViewInstance> instances;
    bool truncated{false};
    std::string truncation_reason;
};

/** @brief 请求有界且可缓存的场景视图数据。 */
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
    MeshAttributeFormat mesh_attribute_format{MeshAttributeFormat::Float32};
    std::uint64_t expected_scene_revision{0};
    std::vector<std::string> instance_ids;
    std::uint64_t max_bytes{0};
};

}  // namespace slicer_core::api
