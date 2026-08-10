#include "TestSupport.h"

#include "slicer_core/api/viewdata/MeshAttributeQuantizer.h"
#include "slicer_core/api/viewdata/SceneViewBudget.h"

namespace stage14b03a
{

void RunQuantizationCases()
{
    const std::vector<float> source{0.0F, 1.0F, -2.0F, 65504.0F};
    const std::vector<std::uint16_t> half =
        slicer_core::api::viewdata_detail::QuantizeMeshAttributesToHalf(
            source);
    Require(
        half == std::vector<std::uint16_t>{
            0x0000U, 0x3C00U, 0xC000U, 0x7BFFU},
        "meshoptimizer half quantization drifted");

    constexpr std::size_t triangleCount{25000U};
    constexpr std::size_t vertexCount{triangleCount * 3U};
    slicer_core::api::ViewMesh mesh;
    mesh.mesh_identity = "quantization-threshold-fixture";
    mesh.positions.resize(vertexCount * 3U);
    mesh.normals.resize(vertexCount * 3U);
    mesh.texcoord0.resize(vertexCount * 2U);
    mesh.indices.resize(triangleCount * 3U);
    mesh.submeshes.push_back({
        0U,
        static_cast<std::uint32_t>(mesh.indices.size()),
        "quantization-material"});

    mesh.attribute_format = slicer_core::api::MeshAttributeFormat::Float32;
    const std::uint64_t floatBytes =
        slicer_core::api::viewdata_detail::EstimateViewMeshBytes(mesh);
    mesh.attribute_format = slicer_core::api::MeshAttributeFormat::Float16;
    const std::uint64_t halfBytes =
        slicer_core::api::viewdata_detail::EstimateViewMeshBytes(mesh);
    constexpr std::uint64_t perInstanceBudget =
        (32U * 1024U * 1024U) / 22U;
    Require(
        halfBytes * 100U <= floatBytes * 60U,
        "half mesh wire estimate did not shrink by at least 40 percent");
    Require(
        floatBytes > perInstanceBudget && halfBytes <= perInstanceBudget,
        "half encoding did not raise the 22-instance threshold to 25k triangles");
}

}  // namespace stage14b03a
