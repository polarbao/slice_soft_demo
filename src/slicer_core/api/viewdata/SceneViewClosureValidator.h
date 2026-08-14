#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/SceneViewDtos.h"

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief 验证所有 ViewData 资源和材质标识引用。
 * @param viewData 候选响应。
 * @return 载荷闭合时返回成功结果，否则返回稳定错误。
 */
[[nodiscard]] ApiResult<void> ValidateViewDataClosure(
    const SceneViewData& viewData) noexcept;

}  // namespace slicer_core::api::viewdata_detail
