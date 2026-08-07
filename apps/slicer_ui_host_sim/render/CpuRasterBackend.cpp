#include "CpuRasterBackend.h"

#include "CpuRasterResources.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iterator>
#include <unordered_set>

class CpuRasterBackend::Implementation final
{
public:
    cpu_raster_detail::Resources resources;
};

CpuRasterBackend::CpuRasterBackend()
    : m_implementation(std::make_unique<Implementation>())
{
}

CpuRasterBackend::~CpuRasterBackend() = default;

slicer::render::BackendCaps CpuRasterBackend::Caps() const
{
    return {
        "cpu_raster",
        true,
        false,
        false,
        16384U,
        0U};
}

bool CpuRasterBackend::UploadMesh(const slicer::render::MeshDesc& mesh)
{
    auto& meshes = m_implementation->resources.meshes;
    if (meshes.contains(mesh.meshIdentity))
    {
        return true;
    }
    if (mesh.meshIdentity.empty() || mesh.vertexCount == 0U
        || mesh.triangleCount == 0U || mesh.position == nullptr
        || mesh.normal == nullptr || mesh.texcoord0 == nullptr
        || mesh.index == nullptr || mesh.submeshes.empty())
    {
        return false;
    }

    cpu_raster_detail::MeshResource resource;
    const auto* positions = static_cast<const float*>(mesh.position);
    const auto* normals = static_cast<const float*>(mesh.normal);
    const auto* texcoords = static_cast<const float*>(mesh.texcoord0);
    resource.positions.assign(positions, positions + mesh.vertexCount * 3U);
    resource.normals.assign(normals, normals + mesh.vertexCount * 3U);
    resource.texcoord0.assign(texcoords, texcoords + mesh.vertexCount * 2U);
    const std::size_t indexCount = mesh.triangleCount * 3U;
    resource.indices.resize(indexCount);
    if (mesh.indexIs32Bit)
    {
        const auto* indices = static_cast<const std::uint32_t*>(mesh.index);
        std::copy(indices, indices + indexCount, resource.indices.begin());
    }
    else
    {
        const auto* indices = static_cast<const std::uint16_t*>(mesh.index);
        std::transform(
            indices,
            indices + indexCount,
            resource.indices.begin(),
            [](const std::uint16_t value)
            {
                return static_cast<std::uint32_t>(value);
            });
    }
    if (std::any_of(
            resource.indices.begin(),
            resource.indices.end(),
            [mesh](const std::uint32_t value)
            {
                return value >= mesh.vertexCount;
            }))
    {
        return false;
    }
    resource.submeshes = mesh.submeshes;
    meshes.emplace(mesh.meshIdentity, std::move(resource));
    return true;
}

bool CpuRasterBackend::UploadTexture(
    const slicer::render::TextureDesc& texture)
{
    auto& textures = m_implementation->resources.textures;
    if (textures.contains(texture.textureIdentity))
    {
        return true;
    }
    if (texture.textureIdentity.empty() || texture.widthPx == 0U
        || texture.heightPx == 0U || texture.rgba8 == nullptr
        || texture.widthPx > Caps().maxTextureSizePx
        || texture.heightPx > Caps().maxTextureSizePx)
    {
        return false;
    }
    const std::uint64_t byteCount =
        static_cast<std::uint64_t>(texture.widthPx)
        * static_cast<std::uint64_t>(texture.heightPx) * 4U;
    cpu_raster_detail::TextureResource resource;
    resource.widthPx = texture.widthPx;
    resource.heightPx = texture.heightPx;
    resource.rowOriginTopLeft = texture.rowOriginTopLeft;
    const auto* pixels = static_cast<const std::uint8_t*>(texture.rgba8);
    resource.rgba8.assign(
        pixels,
        pixels + static_cast<std::size_t>(byteCount));
    textures.emplace(texture.textureIdentity, std::move(resource));
    return true;
}

bool CpuRasterBackend::UploadMaterial(
    const slicer::render::MaterialDesc& material)
{
    const std::string key = cpu_raster_detail::MaterialKey(
        material.appearanceIdentity,
        material.materialId);
    auto& materials = m_implementation->resources.materials;
    if (materials.contains(key))
    {
        return true;
    }
    if (key.empty())
    {
        return false;
    }
    cpu_raster_detail::MaterialResource resource;
    std::copy(
        std::begin(material.baseColorFactor),
        std::end(material.baseColorFactor),
        resource.baseColor.begin());
    std::copy(
        std::begin(material.uvTransform),
        std::end(material.uvTransform),
        resource.uvTransform.begin());
    resource.textureIdentity = material.baseColorTextureIdentity;
    resource.alphaMode = material.alphaMode;
    resource.alphaCutoff = material.alphaCutoff;
    resource.doubleSided = material.doubleSided;
    materials.emplace(key, std::move(resource));
    return true;
}

void CpuRasterBackend::ReleaseUnused(
    const std::vector<std::string>& liveIdentities)
{
    const std::unordered_set<std::string> live(
        liveIdentities.begin(),
        liveIdentities.end());
    auto& resources = m_implementation->resources;
    std::erase_if(resources.meshes, [&live](const auto& item)
    {
        return !live.contains(item.first);
    });
    std::erase_if(resources.textures, [&live](const auto& item)
    {
        return !live.contains(item.first);
    });
    std::erase_if(resources.materials, [&live](const auto& item)
    {
        const std::size_t separator = item.first.find('\n');
        return separator == std::string::npos
            || !live.contains(item.first.substr(0U, separator));
    });
}

slicer::render::FrameResult CpuRasterBackend::RenderFrame(
    const slicer::render::FrameDesc& frame)
{
    const auto start = std::chrono::steady_clock::now();
    slicer::render::ImageOut output;
    std::string errorCode;
    std::uint32_t drawCallCount{0U};
    const bool success = cpu_raster_detail::Rasterize(
        m_implementation->resources,
        frame,
        &output,
        &drawCallCount,
        &errorCode);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return {
        success,
        errorCode,
        std::chrono::duration<double, std::milli>(elapsed).count(),
        0.0,
        drawCallCount};
}

bool CpuRasterBackend::RenderToImage(
    const slicer::render::FrameDesc& frame,
    slicer::render::ImageOut& output)
{
    std::string errorCode;
    std::uint32_t drawCallCount{0U};
    return cpu_raster_detail::Rasterize(
        m_implementation->resources,
        frame,
        &output,
        &drawCallCount,
        &errorCode);
}

slicer::render::PickResult CpuRasterBackend::Pick(
    const slicer::render::FrameDesc& frame,
    const int xPx,
    const int yPx)
{
    return cpu_raster_detail::Pick(
        m_implementation->resources,
        frame,
        xPx,
        yPx);
}
