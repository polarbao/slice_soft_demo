#pragma once

#include <cstdint>
#include <vector>

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief Quantizes IEEE-754 float attributes to IEEE-754 binary16 values.
 * @param values Source float attributes.
 * @return Binary16 bit patterns in source order.
 */
[[nodiscard]] std::vector<std::uint16_t> QuantizeMeshAttributesToHalf(
    const std::vector<float>& values);

}  // namespace slicer_core::api::viewdata_detail
