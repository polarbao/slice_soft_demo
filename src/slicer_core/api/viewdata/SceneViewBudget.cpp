#include "slicer_core/api/viewdata/SceneViewBudget.h"

namespace slicer_core::api::viewdata_detail
{
namespace
{

std::uint64_t StringBytes(const std::string& value)
{
    return static_cast<std::uint64_t>(value.size()) + 8U;
}

}  // namespace

std::uint64_t EstimateViewDataBytes(const SceneViewData& viewData) noexcept
{
    std::uint64_t bytes{1024U};
    bytes += StringBytes(viewData.viewdata_identity);
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
    for (const ViewInstance& instance : viewData.instances)
    {
        bytes += 512U + StringBytes(instance.instance_id)
            + StringBytes(instance.mesh_identity)
            + StringBytes(instance.appearance_identity)
            + StringBytes(instance.preview_identity);
        if (instance.surface_preview.has_value())
        {
            bytes += 128U
                + static_cast<std::uint64_t>(
                    instance.surface_preview->rgba8.size());
        }
        if (instance.mesh.has_value())
        {
            const ViewMesh& mesh = *instance.mesh;
            bytes += 256U
                + static_cast<std::uint64_t>(mesh.positions.size())
                    * sizeof(float)
                + static_cast<std::uint64_t>(mesh.normals.size())
                    * sizeof(float)
                + static_cast<std::uint64_t>(mesh.texcoord0.size())
                    * sizeof(float)
                + static_cast<std::uint64_t>(mesh.indices.size())
                    * sizeof(std::uint32_t);
            for (const ViewSubmesh& submesh : mesh.submeshes)
            {
                bytes += 32U + StringBytes(submesh.material_id);
            }
        }
    }
    return bytes;
}

}  // namespace slicer_core::api::viewdata_detail
