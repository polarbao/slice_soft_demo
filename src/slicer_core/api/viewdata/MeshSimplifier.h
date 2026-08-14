#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace slicer_core::api::viewdata_detail
{

struct MeshSimplificationInput
{
    std::vector<float> positions;
    std::vector<float> texcoord0;
    std::vector<std::uint32_t> indices;
};

struct MeshSimplificationResult
{
    std::vector<std::uint32_t> indices;
    float relative_error{0.0F};
};

/**
 * @brief 在不暴露第三方类型的前提下简化一个材质组。
 * @param input 一个材质组的索引位置、UV 和三角形。
 * @param targetTriangleCount 可接受的最大输出三角形数。
 * @param cancelToken 协作式取消令牌。
 * @return 保持拓扑的简化索引或稳定 ViewData 错误。
 */
[[nodiscard]] ApiResult<MeshSimplificationResult> SimplifyViewMesh(
    const MeshSimplificationInput& input,
    std::size_t targetTriangleCount,
    const ICancelToken& cancelToken) noexcept;

}  // namespace slicer_core::api::viewdata_detail
