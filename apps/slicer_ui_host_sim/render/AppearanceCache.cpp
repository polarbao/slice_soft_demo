#include "AppearanceCache.h"

#include <algorithm>

AppearanceCache::AppearanceCache(slicer::render::IRenderBackend& backend)
    : m_backend(backend)
{
}

bool AppearanceCache::UploadMesh(const slicer::render::MeshDesc& mesh)
{
    if (mesh.meshIdentity.empty())
    {
        return false;
    }
    if (m_meshIdentities.contains(mesh.meshIdentity))
    {
        return true;
    }
    if (!m_backend.UploadMesh(mesh))
    {
        return false;
    }
    m_meshIdentities.insert(mesh.meshIdentity);
    ++m_meshUploadCount;
    return true;
}

bool AppearanceCache::UploadTexture(
    const slicer::render::TextureDesc& texture)
{
    if (texture.textureIdentity.empty())
    {
        return false;
    }
    if (m_textureIdentities.contains(texture.textureIdentity))
    {
        return true;
    }
    if (!m_backend.UploadTexture(texture))
    {
        return false;
    }
    m_textureIdentities.insert(texture.textureIdentity);
    ++m_textureUploadCount;
    return true;
}

bool AppearanceCache::UploadMaterial(
    const slicer::render::MaterialDesc& material)
{
    const std::string key = MaterialKey(material);
    if (key.empty())
    {
        return false;
    }
    if (m_materialKeys.contains(key))
    {
        return true;
    }
    if (!m_backend.UploadMaterial(material))
    {
        return false;
    }
    m_materialKeys.insert(key);
    return true;
}

bool AppearanceCache::HasMesh(const std::string& identity) const
{
    return m_meshIdentities.contains(identity);
}

bool AppearanceCache::HasTexture(const std::string& identity) const
{
    return m_textureIdentities.contains(identity);
}

void AppearanceCache::ReleaseUnused(
    const std::vector<std::string>& liveIdentities)
{
    const std::unordered_set<std::string> live(
        liveIdentities.begin(),
        liveIdentities.end());
    std::erase_if(
        m_meshIdentities,
        [&live](const std::string& value)
        {
            return !live.contains(value);
        });
    std::erase_if(
        m_textureIdentities,
        [&live](const std::string& value)
        {
            return !live.contains(value);
        });
    std::erase_if(
        m_materialKeys,
        [&live](const std::string& value)
        {
            const std::size_t separator = value.find('\n');
            return separator == std::string::npos
                || !live.contains(value.substr(0U, separator));
        });
    m_backend.ReleaseUnused(liveIdentities);
}

std::uint64_t AppearanceCache::MeshUploadCount() const
{
    return m_meshUploadCount;
}

std::uint64_t AppearanceCache::TextureUploadCount() const
{
    return m_textureUploadCount;
}

std::string AppearanceCache::MaterialKey(
    const slicer::render::MaterialDesc& material)
{
    if (material.appearanceIdentity.empty() || material.materialId.empty())
    {
        return {};
    }
    return material.appearanceIdentity + '\n' + material.materialId;
}
