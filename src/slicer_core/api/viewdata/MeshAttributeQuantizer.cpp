#include "slicer_core/api/viewdata/MeshAttributeQuantizer.h"

#include <meshoptimizer.h>

namespace slicer_core::api::viewdata_detail
{

std::vector<std::uint16_t> QuantizeMeshAttributesToHalf(
    const std::vector<float>& values)
{
    std::vector<std::uint16_t> quantized;
    quantized.reserve(values.size());
    for (const float value : values)
    {
        quantized.push_back(meshopt_quantizeHalf(value));
    }
    return quantized;
}

}  // namespace slicer_core::api::viewdata_detail
