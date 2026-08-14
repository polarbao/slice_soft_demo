#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"
#include "slicer_core/scene/SceneModel.h"

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief 控制显示网格派生顶点法线的平滑策略。
 *
 * 这是 ViewData 内部构造策略，不扩展公共 SPI 请求 DTO，
 * 因而保持现有宿主的源码兼容性。
 */
struct ViewMeshNormalOptions
{
    double crease_angle_degrees{40.0};
};

/**
 * @brief 构造接缝安全且材质绑定完整的索引网格。
 * @param model 不可变的已导入模型。
 * @param appearance 闭合的外观资源。
 * @param worldMatrix 权威实例变换。
 * @param lod 请求的实际网格 LOD。
 * @param meshTransform 局部或世界缓冲区语义。
 * @param attributeFormat 序列化网格属性的标量编码。
 * @param cancelToken 协作式取消令牌。
 * @param normalOptions 派生显示法线的平滑策略。
 * @return 网格载荷或稳定的几何/材质错误。
 */
[[nodiscard]] ApiResult<ViewMesh> BuildViewMesh(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    const Matrix4d& worldMatrix,
    ViewLod lod,
    MeshTransform meshTransform,
    MeshAttributeFormat attributeFormat,
    const ICancelToken& cancelToken,
    const ViewMeshNormalOptions& normalOptions = {}) noexcept;

}  // namespace slicer_core::api::viewdata_detail
