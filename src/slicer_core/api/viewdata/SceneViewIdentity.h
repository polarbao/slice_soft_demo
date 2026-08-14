#pragma once

#include "slicer_core/api/SceneViewDtos.h"

#include <string>

namespace slicer_core::api::viewdata_detail
{

/** @brief 计算已解码纹理像素的内容标识。 */
[[nodiscard]] std::string ComputeTextureIdentity(
    const ViewTexture& texture);

/** @brief 计算一个外观的材质绑定标识。 */
[[nodiscard]] std::string ComputeAppearanceIdentity(
    const ViewAppearance& appearance);

/** @brief 计算一个网格的几何和 LOD 标识。 */
[[nodiscard]] std::string ComputeMeshIdentity(const ViewMesh& mesh);

/** @brief 计算局部空间纹理俯视预览标识。 */
[[nodiscard]] std::string ComputePreviewIdentity(
    const SurfacePreview& preview);

/** @brief 计算完整场景视图快照的标识。 */
[[nodiscard]] std::string ComputeViewDataIdentity(
    const SceneViewData& viewData);

}  // namespace slicer_core::api::viewdata_detail
