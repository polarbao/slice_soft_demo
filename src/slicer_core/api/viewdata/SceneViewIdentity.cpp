#include "slicer_core/api/viewdata/SceneViewIdentity.h"

#include "slicer_core/system/Sha256.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace slicer_core::api::viewdata_detail
{
namespace
{

class CanonicalWriter
{
public:
    void AppendString(const std::string_view value)
    {
        AppendUnsigned(static_cast<std::uint64_t>(value.size()));
        m_payload.append(value.data(), value.size());
    }

    template <class T>
    void AppendUnsigned(const T value)
    {
        static_assert(std::is_unsigned_v<T>);
        for (std::size_t index{0U}; index < sizeof(T); ++index)
        {
            m_payload.push_back(static_cast<char>(
                (value >> (index * 8U)) & static_cast<T>(0xFFU)));
        }
    }

    void AppendBool(const bool value)
    {
        AppendUnsigned<std::uint8_t>(value ? 1U : 0U);
    }

    void AppendFloat(float value)
    {
        if (value == 0.0F)
        {
            value = 0.0F;
        }
        AppendUnsigned(std::bit_cast<std::uint32_t>(value));
    }

    void AppendDouble(double value)
    {
        if (value == 0.0)
        {
            value = 0.0;
        }
        AppendUnsigned(std::bit_cast<std::uint64_t>(value));
    }

    void AppendBytes(const std::vector<std::uint8_t>& bytes)
    {
        AppendUnsigned(static_cast<std::uint64_t>(bytes.size()));
        if (!bytes.empty())
        {
            m_payload.append(
                reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
    }

    [[nodiscard]] std::string Digest(const std::string_view prefix) const
    {
        return std::string(prefix) + ComputeSha256(m_payload);
    }

private:
    std::string m_payload;
};

void AppendBounds(CanonicalWriter& writer, const Bounds3d& bounds)
{
    for (const double value : bounds.min_mm)
    {
        writer.AppendDouble(value);
    }
    for (const double value : bounds.max_mm)
    {
        writer.AppendDouble(value);
    }
}

void AppendMatrix(CanonicalWriter& writer, const Matrix4d& matrix)
{
    for (const double value : matrix.values)
    {
        writer.AppendDouble(value);
    }
}

void AppendAppearanceIdentity(
    CanonicalWriter& writer,
    const ViewAppearance& appearance)
{
    std::map<std::string, std::string> textureIdentities;
    for (const ViewTexture& texture : appearance.textures)
    {
        textureIdentities.emplace(
            texture.texture_id,
            texture.texture_identity);
    }

    writer.AppendString("slicesoft.view.appearance.1");
    writer.AppendUnsigned(
        static_cast<std::uint64_t>(appearance.materials.size()));
    for (const ViewMaterial& material : appearance.materials)
    {
        writer.AppendString(material.material_id);
        for (const float value : material.base_color)
        {
            writer.AppendFloat(value);
        }
        const auto texture = textureIdentities.find(material.texture_id);
        writer.AppendString(
            texture == textureIdentities.end() ? "" : texture->second);
        for (const float value : material.uv_transform)
        {
            writer.AppendFloat(value);
        }
        writer.AppendUnsigned(
            static_cast<std::uint32_t>(material.uv_set));
        writer.AppendBool(material.double_sided);
    }
}

}  // namespace

std::string ComputeTextureIdentity(const ViewTexture& texture)
{
    CanonicalWriter writer;
    writer.AppendString("slicesoft.view.texture.1");
    writer.AppendUnsigned(static_cast<std::uint32_t>(texture.width_px));
    writer.AppendUnsigned(static_cast<std::uint32_t>(texture.height_px));
    writer.AppendString("rgba8_unorm");
    writer.AppendString("srgb");
    writer.AppendString("straight");
    writer.AppendString("top_left");
    writer.AppendBytes(texture.rgba8);
    return writer.Digest("tex:");
}

std::string ComputeAppearanceIdentity(const ViewAppearance& appearance)
{
    CanonicalWriter writer;
    AppendAppearanceIdentity(writer, appearance);
    return writer.Digest("app:");
}

std::string ComputeMeshIdentity(const ViewMesh& mesh)
{
    CanonicalWriter writer;
    writer.AppendString("slicesoft.view.mesh.1");
    writer.AppendUnsigned(static_cast<std::uint32_t>(mesh.lod));
    writer.AppendUnsigned(static_cast<std::uint32_t>(mesh.mesh_transform));
    writer.AppendUnsigned(static_cast<std::uint64_t>(mesh.positions.size()));
    for (const float value : mesh.positions)
    {
        writer.AppendFloat(value);
    }
    writer.AppendUnsigned(static_cast<std::uint64_t>(mesh.normals.size()));
    for (const float value : mesh.normals)
    {
        writer.AppendFloat(value);
    }
    writer.AppendUnsigned(static_cast<std::uint64_t>(mesh.texcoord0.size()));
    for (const float value : mesh.texcoord0)
    {
        writer.AppendFloat(value);
    }
    writer.AppendUnsigned(static_cast<std::uint64_t>(mesh.indices.size()));
    for (const std::uint32_t value : mesh.indices)
    {
        writer.AppendUnsigned(value);
    }
    writer.AppendUnsigned(static_cast<std::uint64_t>(mesh.submeshes.size()));
    for (const ViewSubmesh& submesh : mesh.submeshes)
    {
        writer.AppendUnsigned(submesh.first_index);
        writer.AppendUnsigned(submesh.index_count);
        writer.AppendString(submesh.material_id);
    }
    return writer.Digest("mesh:");
}

std::string ComputePreviewIdentity(const SurfacePreview& preview)
{
    CanonicalWriter writer;
    writer.AppendString("slicesoft.view.top_preview.1");
    writer.AppendString(preview.appearance_identity);
    writer.AppendUnsigned(static_cast<std::uint32_t>(preview.width_px));
    writer.AppendUnsigned(static_cast<std::uint32_t>(preview.height_px));
    AppendBounds(writer, preview.local_bounds_mm);
    writer.AppendBytes(preview.rgba8);
    return writer.Digest("preview:");
}

std::string ComputeViewDataIdentity(const SceneViewData& viewData)
{
    CanonicalWriter writer;
    writer.AppendString("slicesoft.scene_viewdata.1");
    writer.AppendUnsigned(viewData.scene_revision);
    writer.AppendUnsigned(static_cast<std::uint32_t>(viewData.view_mode));
    writer.AppendUnsigned(static_cast<std::uint64_t>(viewData.instances.size()));
    for (const ViewInstance& instance : viewData.instances)
    {
        writer.AppendString(instance.instance_id);
        writer.AppendUnsigned(instance.model_id);
        AppendMatrix(writer, instance.world_matrix);
        AppendBounds(writer, instance.local_bounds_mm);
        writer.AppendString(instance.mesh_identity);
        writer.AppendString(instance.appearance_identity);
        writer.AppendString(instance.preview_identity);
    }
    writer.AppendUnsigned(static_cast<std::uint64_t>(viewData.appearances.size()));
    for (const ViewAppearance& appearance : viewData.appearances)
    {
        writer.AppendString(appearance.appearance_identity);
    }
    return writer.Digest("vd:");
}

}  // namespace slicer_core::api::viewdata_detail
