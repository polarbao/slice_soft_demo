#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/SceneViewResources.h"

#include <map>
#include <string>
#include <vector>

namespace slicer_core::api::viewdata_detail
{

/** @brief 已解析为 ViewData 标识的一个源材质绑定。 */
struct ResolvedViewMaterial
{
    std::string source_name;
    ViewMaterial material;
    bool has_texture{false};
    std::size_t texture_index{0U};
};

/** @brief 一个模型使用的闭合材质和纹理资源。 */
struct ResolvedViewAppearance
{
    ViewAppearance appearance;
    std::vector<ResolvedViewMaterial> materials;
    std::map<std::string, std::size_t> material_indices;
    bool has_texture{false};
};

/**
 * @brief 仅解析模型三角形可达的材质资源。
 * @param model 不可变的已导入模型。
 * @param textureSource 按失败即拒绝规则工作的纹理解码器。
 * @param cancelToken 协作式取消令牌。
 * @return 闭合外观或稳定的 PM-SLICER 错误。
 */
[[nodiscard]] ApiResult<ResolvedViewAppearance> ResolveViewAppearance(
    const SceneModel& model,
    const ISceneViewTextureSource& textureSource,
    const ICancelToken& cancelToken) noexcept;

/**
 * @brief 解析一个三角形使用的外观材质。
 * @param model 源模型。
 * @param appearance 闭合外观。
 * @param triangleIndex 三角形索引。
 * @return 已解析材质索引或稳定绑定错误。
 */
[[nodiscard]] ApiResult<std::size_t> ResolveTriangleMaterialIndex(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    std::size_t triangleIndex) noexcept;

}  // namespace slicer_core::api::viewdata_detail
