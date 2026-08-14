#pragma once

#include "IRenderBackend.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * @brief 在资源到达后端前应用 ViewData 标识缓存。
 */
class AppearanceCache final
{
public:
/** @brief 为指定渲染后端创建空缓存。 */
    explicit AppearanceCache(slicer::render::IRenderBackend& backend);

    /** @brief 仅在标识未缓存时上传网格。 */
    bool UploadMesh(const slicer::render::MeshDesc& mesh);

    /** @brief 仅在标识未缓存时上传纹理。 */
    bool UploadTexture(const slicer::render::TextureDesc& texture);

    /** @brief 仅在外观绑定为新值时上传材料。 */
    bool UploadMaterial(const slicer::render::MaterialDesc& material);

    /** @brief 报告网格标识是否已驻留。 */
    [[nodiscard]] bool HasMesh(const std::string& identity) const;

    /** @brief 报告纹理标识是否已驻留。 */
    [[nodiscard]] bool HasTexture(const std::string& identity) const;

    /** @brief 释放当前活动标识集合之外的资源。 */
    void ReleaseUnused(const std::vector<std::string>& liveIdentities);

    /** @brief 返回实际网格后端上传次数。 */
    [[nodiscard]] std::uint64_t MeshUploadCount() const;

    /** @brief 返回实际纹理后端上传次数。 */
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
