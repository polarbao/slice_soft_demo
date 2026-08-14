#pragma once

#include <cstdint>
#include <vector>

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief 将 IEEE-754 浮点属性量化为 IEEE-754 binary16 值。
 * @param values 源浮点属性。
 * @return 按源顺序排列的 binary16 位模式。
 */
[[nodiscard]] std::vector<std::uint16_t> QuantizeMeshAttributesToHalf(
    const std::vector<float>& values);

}  // namespace slicer_core::api::viewdata_detail
