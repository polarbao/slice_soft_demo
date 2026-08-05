#include "slicer_core/api/viewdata/SceneViewAppearanceBudget.h"

#include "slicer_core/api/viewdata/SceneViewIdentity.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core::api::viewdata_detail
{
namespace
{

void ValidateTexture(const ViewTexture& texture)
{
    if (texture.width_px <= 0 || texture.height_px <= 0)
    {
        throw std::invalid_argument(
            "appearance budget received a texture with invalid dimensions");
    }

    const std::size_t width = static_cast<std::size_t>(texture.width_px);
    const std::size_t height = static_cast<std::size_t>(texture.height_px);
    const std::size_t expectedBytes = width * height * 4U;
    if (texture.rgba8.size() != expectedBytes)
    {
        throw std::invalid_argument(
            "appearance budget received an invalid RGBA8 texture payload");
    }
}

std::size_t ScaleDimension(
    const std::size_t sourceDimension,
    const std::size_t sourceMaxEdge,
    const std::size_t targetMaxEdge)
{
    const std::uint64_t numerator =
        static_cast<std::uint64_t>(sourceDimension)
            * static_cast<std::uint64_t>(targetMaxEdge)
        + static_cast<std::uint64_t>(sourceMaxEdge / 2U);
    return std::max<std::size_t>(
        1U,
        static_cast<std::size_t>(
            numerator / static_cast<std::uint64_t>(sourceMaxEdge)));
}

std::size_t NearestSourceCoordinate(
    const std::size_t targetCoordinate,
    const std::size_t sourceDimension,
    const std::size_t targetDimension)
{
    const std::uint64_t numerator =
        (2U * static_cast<std::uint64_t>(targetCoordinate) + 1U)
        * static_cast<std::uint64_t>(sourceDimension);
    const std::size_t coordinate = static_cast<std::size_t>(
        numerator / (2U * static_cast<std::uint64_t>(targetDimension)));
    return std::min(sourceDimension - 1U, coordinate);
}

ViewTexture DownsampleTexture(
    const ViewTexture& source,
    const std::size_t maxTextureEdgePx)
{
    ValidateTexture(source);

    ViewTexture result = source;
    const std::size_t sourceWidth =
        static_cast<std::size_t>(source.width_px);
    const std::size_t sourceHeight =
        static_cast<std::size_t>(source.height_px);
    const std::size_t sourceMaxEdge = std::max(sourceWidth, sourceHeight);
    if (sourceMaxEdge > maxTextureEdgePx)
    {
        const std::size_t targetWidth = ScaleDimension(
            sourceWidth,
            sourceMaxEdge,
            maxTextureEdgePx);
        const std::size_t targetHeight = ScaleDimension(
            sourceHeight,
            sourceMaxEdge,
            maxTextureEdgePx);

        std::vector<std::uint8_t> pixels(targetWidth * targetHeight * 4U);
        for (std::size_t targetY{0U}; targetY < targetHeight; ++targetY)
        {
            const std::size_t sourceY = NearestSourceCoordinate(
                targetY,
                sourceHeight,
                targetHeight);
            for (std::size_t targetX{0U}; targetX < targetWidth; ++targetX)
            {
                const std::size_t sourceX = NearestSourceCoordinate(
                    targetX,
                    sourceWidth,
                    targetWidth);
                const std::size_t sourceOffset =
                    (sourceY * sourceWidth + sourceX) * 4U;
                const std::size_t targetOffset =
                    (targetY * targetWidth + targetX) * 4U;
                std::copy_n(
                    source.rgba8.begin()
                        + static_cast<std::ptrdiff_t>(sourceOffset),
                    4U,
                    pixels.begin()
                        + static_cast<std::ptrdiff_t>(targetOffset));
            }
        }

        result.width_px = static_cast<int>(targetWidth);
        result.height_px = static_cast<int>(targetHeight);
        result.rgba8 = std::move(pixels);
    }

    result.texture_identity = ComputeTextureIdentity(result);
    result.texture_id = result.texture_identity;
    return result;
}

}  // namespace

ResolvedViewAppearance DownsampleAppearanceTextures(
    const ResolvedViewAppearance& appearance,
    const std::size_t maxTextureEdgePx)
{
    if (maxTextureEdgePx == 0U)
    {
        throw std::invalid_argument(
            "appearance texture edge budget must be greater than zero");
    }

    ResolvedViewAppearance result = appearance;
    std::vector<ViewTexture> textures;
    textures.reserve(appearance.appearance.textures.size());

    std::vector<std::size_t> textureRemap(
        appearance.appearance.textures.size(),
        0U);
    std::map<std::string, std::size_t> textureIndices;
    for (std::size_t sourceIndex{0U};
         sourceIndex < appearance.appearance.textures.size();
         ++sourceIndex)
    {
        ViewTexture texture = DownsampleTexture(
            appearance.appearance.textures.at(sourceIndex),
            maxTextureEdgePx);
        const auto existing = textureIndices.find(texture.texture_identity);
        if (existing != textureIndices.end())
        {
            textureRemap.at(sourceIndex) = existing->second;
            continue;
        }

        const std::size_t targetIndex = textures.size();
        textureRemap.at(sourceIndex) = targetIndex;
        textureIndices.emplace(texture.texture_identity, targetIndex);
        textures.push_back(std::move(texture));
    }

    std::map<std::string, std::string> materialTextureIds;
    for (ResolvedViewMaterial& material : result.materials)
    {
        if (!material.has_texture)
        {
            material.material.texture_id.clear();
            continue;
        }
        if (material.texture_index >= textureRemap.size())
        {
            throw std::invalid_argument(
                "appearance budget received an invalid texture index");
        }

        material.texture_index = textureRemap.at(material.texture_index);
        material.material.texture_id =
            textures.at(material.texture_index).texture_id;
        materialTextureIds.emplace(
            material.material.material_id,
            material.material.texture_id);
    }

    result.appearance.textures = std::move(textures);
    for (ViewMaterial& material : result.appearance.materials)
    {
        const auto textureId = materialTextureIds.find(material.material_id);
        if (textureId != materialTextureIds.end())
        {
            material.texture_id = textureId->second;
        }
        else if (!material.texture_id.empty())
        {
            throw std::invalid_argument(
                "appearance budget received an unresolved material binding");
        }
    }

    result.has_texture = !result.appearance.textures.empty();
    result.appearance.appearance_identity =
        ComputeAppearanceIdentity(result.appearance);
    return result;
}

}  // namespace slicer_core::api::viewdata_detail
