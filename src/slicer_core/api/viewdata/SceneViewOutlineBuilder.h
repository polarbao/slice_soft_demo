#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/SceneViewDtos.h"

#include <vector>

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief 从表面预览提取确定性的局部 XY 闭合轮廓。
 * @param preview 局部边界内有效的直通 Alpha RGBA8 表面预览。
 * @return 包含非连通分量和孔洞的闭合轮廓环，或稳定错误。
 */
[[nodiscard]] ApiResult<std::vector<ViewOutline>> BuildViewOutlines(
    const SurfacePreview& preview) noexcept;

}  // namespace slicer_core::api::viewdata_detail
