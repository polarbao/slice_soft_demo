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
 * @brief Simplifies one material group without exposing third-party types.
 * @param input Indexed positions, UVs, and triangles for one material group.
 * @param targetTriangleCount Maximum accepted output triangle count.
 * @param cancelToken Cooperative cancellation token.
 * @return Topology-preserving simplified indices or a stable ViewData error.
 */
[[nodiscard]] ApiResult<MeshSimplificationResult> SimplifyViewMesh(
    const MeshSimplificationInput& input,
    std::size_t targetTriangleCount,
    const ICancelToken& cancelToken) noexcept;

}  // namespace slicer_core::api::viewdata_detail
