#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/MeshSimplifier.h"
#include "slicer_core/scene/SceneModel.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief 在 LOD 简化前为一个材质构造共享拓扑。
 * @param model 不可变的已导入模型。
 * @param triangleIndices 属于一个材质的源三角形。
 * @param worldMatrix 权威实例变换。
 * @param meshTransform 局部或世界缓冲区语义。
 * @param cancelToken 协作式取消令牌。
 * @return 索引位置与 UV，或稳定几何错误。
 */
[[nodiscard]] ApiResult<MeshSimplificationInput> BuildViewMeshGroupInput(
    const SceneModel& model,
    const std::vector<std::size_t>& triangleIndices,
    const Matrix4d& worldMatrix,
    MeshTransform meshTransform,
    const ICancelToken& cancelToken);

/**
 * @brief 从已验证的简化输入读取一个位置。
 * @param input 已验证的简化输入。
 * @param index 顶点索引。
 * @return 位置值。
 */
[[nodiscard]] Vec3 ReadSimplificationPoint(
    const MeshSimplificationInput& input,
    std::uint32_t index);

/**
 * @brief 从已验证的简化输入读取一个 UV 坐标。
 * @param input 已验证的简化输入。
 * @param index 顶点索引。
 * @return UV 值。
 */
[[nodiscard]] TexCoord ReadSimplificationUv(
    const MeshSimplificationInput& input,
    std::uint32_t index);

}  // namespace slicer_core::api::viewdata_detail
