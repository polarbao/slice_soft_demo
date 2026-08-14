#pragma once

#include "slicer_core/api/SceneViewDtos.h"

#include <cstdint>

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief 估算一个网格的序列化描述符和二进制载荷字节数。
 * @param mesh 按请求传输格式编码属性的网格。
 * @return 保守的序列化网格字节估算值。
 */
[[nodiscard]] std::uint64_t EstimateViewMeshBytes(
    const ViewMesh& mesh) noexcept;

/**
 * @brief 估算内联 ViewData 资源的序列化载荷字节数。
 * @param viewData 完整候选响应。
 * @return 包含资源载荷的保守字节估算值。
 */
[[nodiscard]] std::uint64_t EstimateViewDataBytes(
    const SceneViewData& viewData) noexcept;

}  // namespace slicer_core::api::viewdata_detail
