#include "slicer_core/api/viewdata/SceneViewBudget.h"

namespace slicer_core::api::viewdata_detail
{
namespace
{

std::uint64_t StringBytes(const std::string& value)
{
    return static_cast<std::uint64_t>(value.size()) + 8U;
}

std::uint64_t AttributeScalarBytes(const MeshAttributeFormat format)
{
    return format == MeshAttributeFormat::Float16
        ? sizeof(std::uint16_t)
        : sizeof(float);
}

}  // namespace

std::uint64_t EstimateViewMeshBytes(const ViewMesh& mesh) noexcept
{
    const std::uint64_t scalarBytes = AttributeScalarBytes(
        mesh.attribute_format);
    std::uint64_t bytes = 256U + StringBytes(mesh.mesh_identity)
        + static_cast<std::uint64_t>(mesh.positions.size()) * scalarBytes
        + static_cast<std::uint64_t>(mesh.normals.size()) * scalarBytes
        + static_cast<std::uint64_t>(mesh.texcoord0.size()) * scalarBytes
        + static_cast<std::uint64_t>(mesh.indices.size())
            * sizeof(std::uint32_t);
    for (const ViewSubmesh& submesh : mesh.submeshes)
    {
        bytes += 32U + StringBytes(submesh.material_id);
    }
    return bytes;
}

std::uint64_t EstimateViewDataBytes(const SceneViewData& viewData) noexcept
{
    std::uint64_t bytes{1024U};
    bytes += StringBytes(viewData.viewdata_identity);
    bytes += StringBytes(viewData.truncation_reason);
    for (const ViewAppearance& appearance : viewData.appearances)
    {
        bytes += 128U + StringBytes(appearance.appearance_identity);
        for (const ViewMaterial& material : appearance.materials)
        {
            bytes += 128U + StringBytes(material.material_id)
                + StringBytes(material.texture_id);
        }
        for (const ViewTexture& texture : appearance.textures)
        {
            bytes += 128U + StringBytes(texture.texture_id)
                + StringBytes(texture.texture_identity)
                + static_cast<std::uint64_t>(texture.rgba8.size());
        }
    }
    for (const ViewMesh& mesh : viewData.meshes)
    {
        bytes += EstimateViewMeshBytes(mesh);
    }
    for (const ViewInstance& instance : viewData.instances)
    {
        bytes += 512U + StringBytes(instance.instance_id)
            + StringBytes(instance.mesh_identity)
            + StringBytes(instance.appearance_identity)
            + StringBytes(instance.preview_identity);
        for (const ViewOutline& outline : instance.outlines)
        {
            bytes += 32U
                + static_cast<std::uint64_t>(outline.points_mm.size())
                    * sizeof(double) * 2U;
        }
        if (instance.surface_preview.has_value())
        {
            bytes += 128U
                + static_cast<std::uint64_t>(
                    instance.surface_preview->rgba8.size());
        }
        if (viewData.meshes.empty() && instance.mesh.has_value())
        {
            const ViewMesh& mesh = *instance.mesh;
            bytes += EstimateViewMeshBytes(mesh);
        }
    }
    return bytes;
}

}  // namespace slicer_core::api::viewdata_detail
