#include "slicer_core/api/viewdata/SceneViewClosureValidator.h"

#include <cstddef>
#include <set>
#include <string>
#include <string_view>

namespace slicer_core::api::viewdata_detail
{
namespace
{

ApiResult<void> Failure(
    const std::string_view message,
    const std::string& detail)
{
    return ApiResult<void>::Failure({
        "PM-SLICER-INTERNAL-0099",
        std::string(message),
        detail});
}

}  // namespace

ApiResult<void> ValidateViewDataClosure(
    const SceneViewData& viewData) noexcept
{
    std::set<std::string> appearanceIdentities;
    for (const ViewAppearance& appearance : viewData.appearances)
    {
        if (appearance.appearance_identity.empty()
            || !appearanceIdentities.emplace(
                appearance.appearance_identity).second)
        {
            return Failure(
                "ViewData appearance identity is empty or duplicated",
                appearance.appearance_identity);
        }
        std::set<std::string> textureIds;
        for (const ViewTexture& texture : appearance.textures)
        {
            const std::size_t expectedBytes =
                static_cast<std::size_t>(texture.width_px)
                * static_cast<std::size_t>(texture.height_px) * 4U;
            if (texture.texture_id.empty()
                || texture.texture_identity.empty()
                || texture.width_px <= 0 || texture.height_px <= 0
                || texture.rgba8.size() != expectedBytes
                || !textureIds.emplace(texture.texture_id).second)
            {
                return Failure(
                    "ViewData texture resource is invalid",
                    texture.texture_id);
            }
        }

        std::set<std::string> materialIds;
        for (const ViewMaterial& material : appearance.materials)
        {
            if (material.material_id.empty()
                || !materialIds.emplace(material.material_id).second)
            {
                return Failure(
                    "ViewData material identity is empty or duplicated",
                    material.material_id);
            }
            if (!material.texture_id.empty()
                && !textureIds.contains(material.texture_id))
            {
                return Failure(
                    "ViewData material references an unknown texture",
                    material.texture_id);
            }
        }
    }

    std::set<std::string> instanceIds;
    for (const ViewInstance& instance : viewData.instances)
    {
        if (instance.instance_id.empty()
            || !instanceIds.emplace(instance.instance_id).second
            || !appearanceIdentities.contains(
                instance.appearance_identity))
        {
            return Failure(
                "ViewData instance identity closure is invalid",
                instance.instance_id);
        }
        if (viewData.view_mode == ViewMode::Top)
        {
            if (!instance.surface_preview.has_value()
                || instance.surface_preview->preview_identity.empty()
                || instance.preview_identity
                    != instance.surface_preview->preview_identity
                || instance.appearance_identity
                    != instance.surface_preview->appearance_identity)
            {
                return Failure(
                    "top ViewData preview identity is not closed",
                    instance.instance_id);
            }
            const SurfacePreview& preview = *instance.surface_preview;
            const std::size_t expectedBytes =
                static_cast<std::size_t>(preview.width_px)
                * static_cast<std::size_t>(preview.height_px) * 4U;
            if (preview.width_px <= 0 || preview.height_px <= 0
                || preview.rgba8.size() != expectedBytes)
            {
                return Failure(
                    "top ViewData preview RGBA8 payload is invalid",
                    instance.instance_id);
            }
        }
        else
        {
            if (!instance.mesh.has_value()
                || instance.mesh_identity.empty()
                || instance.mesh_identity
                    != instance.mesh->mesh_identity)
            {
                return Failure(
                    "three_d ViewData mesh identity is not closed",
                    instance.instance_id);
            }
            const ViewMesh& mesh = *instance.mesh;
            const std::size_t vertexCount = mesh.positions.size() / 3U;
            if (mesh.positions.size() % 3U != 0U
                || mesh.normals.size() != mesh.positions.size()
                || mesh.texcoord0.size() != vertexCount * 2U
                || mesh.indices.empty())
            {
                return Failure(
                    "three_d ViewData mesh buffers are invalid",
                    instance.instance_id);
            }
            const ViewAppearance* appearance{nullptr};
            for (const ViewAppearance& candidate : viewData.appearances)
            {
                if (candidate.appearance_identity
                    == instance.appearance_identity)
                {
                    appearance = &candidate;
                    break;
                }
            }
            std::set<std::string> materialIds;
            for (const ViewMaterial& material : appearance->materials)
            {
                materialIds.emplace(material.material_id);
            }
            for (const ViewSubmesh& submesh : mesh.submeshes)
            {
                if (!materialIds.contains(submesh.material_id)
                    || submesh.index_count == 0U
                    || static_cast<std::size_t>(submesh.first_index)
                            + submesh.index_count
                        > mesh.indices.size())
                {
                    return Failure(
                        "three_d ViewData submesh binding is invalid",
                        submesh.material_id);
                }
            }
        }
    }
    return ApiResult<void>::Success();
}

}  // namespace slicer_core::api::viewdata_detail
