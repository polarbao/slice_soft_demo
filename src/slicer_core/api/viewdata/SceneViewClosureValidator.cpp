#include "slicer_core/api/viewdata/SceneViewClosureValidator.h"

#include <cstddef>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <string_view>

// 文件职责：验证 ViewData 内网格、材质、纹理和实例引用的闭合性；
// 边界：只做结构准入，不修补缺失资源或猜测绑定关系。
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

bool IsIdentity(const Matrix4d& matrix)
{
    const Matrix4d identity;
    for (std::size_t index{0U}; index < matrix.values.size(); ++index)
    {
        if (std::abs(matrix.values.at(index) - identity.values.at(index))
            > 1.0e-12)
        {
            return false;
        }
    }
    return true;
}

ApiResult<void> ValidateMesh(const ViewMesh& mesh)
{
    const std::size_t vertexCount = mesh.positions.size() / 3U;
    if (mesh.mesh_identity.empty()
        || mesh.positions.size() % 3U != 0U
        || mesh.normals.size() != mesh.positions.size()
        || mesh.texcoord0.size() != vertexCount * 2U
        || mesh.indices.empty())
    {
        return Failure(
            "three_d ViewData mesh buffers are invalid",
            mesh.mesh_identity);
    }
    for (const ViewSubmesh& submesh : mesh.submeshes)
    {
        if (submesh.material_id.empty()
            || submesh.index_count == 0U
            || static_cast<std::size_t>(submesh.first_index)
                    + submesh.index_count
                > mesh.indices.size())
        {
            return Failure(
                "three_d ViewData submesh range is invalid",
                submesh.material_id);
        }
    }
    return ApiResult<void>::Success();
}

}  // namespace

ApiResult<void> ValidateViewDataClosure(
    const SceneViewData& viewData) noexcept
{
    if (viewData.truncated != !viewData.truncation_reason.empty())
    {
        return Failure(
            "ViewData truncation state and reason are inconsistent",
            viewData.truncation_reason);
    }
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

    std::map<std::string, const ViewMesh*> meshesByIdentity;
    for (const ViewMesh& mesh : viewData.meshes)
    {
        const ApiResult<void> validMesh = ValidateMesh(mesh);
        if (!validMesh.IsOk())
        {
            return validMesh;
        }
        if (!meshesByIdentity.emplace(
                mesh.mesh_identity,
                &mesh).second)
        {
            return Failure(
                "ViewData mesh identity is duplicated",
                mesh.mesh_identity);
        }
    }
    if (viewData.view_mode == ViewMode::Top && !viewData.meshes.empty())
    {
        return Failure(
            "top ViewData must not include three_d meshes",
            std::to_string(viewData.meshes.size()));
    }
    if (viewData.view_mode == ViewMode::ThreeD && viewData.meshes.empty())
    {
        return Failure(
            "three_d ViewData has no reusable meshes",
            "meshes[]");
    }

    std::set<std::string> instanceIds;
    std::set<std::string> referencedMeshIdentities;
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
        if (instance.outlines.empty())
        {
            return Failure(
                "ViewData instance has no local-space outline",
                instance.instance_id);
        }
        for (const ViewOutline& outline : instance.outlines)
        {
            if (outline.points_mm.size() < 3U)
            {
                return Failure(
                    "ViewData outline has fewer than three points",
                    instance.instance_id);
            }
            for (const std::array<double, 2>& point : outline.points_mm)
            {
                if (!std::isfinite(point.at(0U))
                    || !std::isfinite(point.at(1U)))
                {
                    return Failure(
                        "ViewData outline contains non-finite coordinates",
                        instance.instance_id);
                }
            }
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
            const auto meshEntry = meshesByIdentity.find(
                instance.mesh_identity);
            if (instance.mesh_identity.empty()
                || meshEntry == meshesByIdentity.end()
                || (instance.mesh.has_value()
                    && instance.mesh->mesh_identity
                        != instance.mesh_identity))
            {
                return Failure(
                    "three_d ViewData mesh identity is not closed",
                    instance.instance_id);
            }
            const ViewMesh& mesh = *meshEntry->second;
            referencedMeshIdentities.emplace(instance.mesh_identity);
            if (mesh.mesh_transform == MeshTransform::World
                && !IsIdentity(instance.world_matrix))
            {
                return Failure(
                    "world ViewData mesh requires an identity worldMatrix",
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
                    || submesh.index_count == 0U)
                {
                    return Failure(
                        "three_d ViewData submesh binding is invalid",
                        submesh.material_id);
                }
            }
        }
    }
    if (viewData.view_mode == ViewMode::ThreeD
        && referencedMeshIdentities.size() != meshesByIdentity.size())
    {
        return Failure(
            "three_d ViewData contains an unreferenced mesh",
            std::to_string(
                meshesByIdentity.size() - referencedMeshIdentities.size()));
    }
    return ApiResult<void>::Success();
}

}  // namespace slicer_core::api::viewdata_detail
