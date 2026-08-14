#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"
#include "slicer_core/scene/SceneModel.h"

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief 栅格化局部 +Z 方向的纹理表面预览。
 * @param model 不可变的已导入模型。
 * @param appearance 闭合的外观资源。
 * @param maximumDimension 最大预览宽度或高度。
 * @param cancelToken 协作式取消令牌。
 * @return 直通 Alpha 的 RGBA8 预览或稳定错误。
 */
[[nodiscard]] ApiResult<SurfacePreview> BuildSurfacePreview(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    int maximumDimension,
    const ICancelToken& cancelToken) noexcept;

}  // namespace slicer_core::api::viewdata_detail
