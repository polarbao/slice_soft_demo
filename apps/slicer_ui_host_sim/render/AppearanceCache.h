#pragma once

#include "IRenderBackend.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * @brief Applies ViewData identity caching before resources reach a backend.
 */
class AppearanceCache final
{
public:
    /** @brief Creates an empty cache over one renderer backend. */
    explicit AppearanceCache(slicer::render::IRenderBackend& backend);

    /** @brief Uploads a mesh only when its identity is not cached. */
    bool UploadMesh(const slicer::render::MeshDesc& mesh);

    /** @brief Uploads a texture only when its identity is not cached. */
    bool UploadTexture(const slicer::render::TextureDesc& texture);

    /** @brief Uploads a material only when its appearance binding is new. */
    bool UploadMaterial(const slicer::render::MaterialDesc& material);

    /** @brief Reports whether a mesh identity is already resident. */
    [[nodiscard]] bool HasMesh(const std::string& identity) const;

    /** @brief Reports whether a texture identity is already resident. */
    [[nodiscard]] bool HasTexture(const std::string& identity) const;

    /** @brief Releases resources outside the current live identity set. */
    void ReleaseUnused(const std::vector<std::string>& liveIdentities);

    /** @brief Returns the number of real mesh backend uploads. */
    [[nodiscard]] std::uint64_t MeshUploadCount() const;

    /** @brief Returns the number of real texture backend uploads. */
    [[nodiscard]] std::uint64_t TextureUploadCount() const;

private:
    [[nodiscard]] static std::string MaterialKey(
        const slicer::render::MaterialDesc& material);

    slicer::render::IRenderBackend& m_backend;
    std::unordered_set<std::string> m_meshIdentities;
    std::unordered_set<std::string> m_textureIdentities;
    std::unordered_set<std::string> m_materialKeys;
    std::uint64_t m_meshUploadCount{0U};
    std::uint64_t m_textureUploadCount{0U};
};
