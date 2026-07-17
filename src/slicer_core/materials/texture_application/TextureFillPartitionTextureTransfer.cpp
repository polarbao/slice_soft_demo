#include "slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.h"

#include "slicer_core/materials/texture_application/SurfaceAttributeMap.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;

std::optional<std::size_t> VoxelCount(const TextureFillPartitionGridSpec& grid)
{
    if (grid.width <= 0 || grid.height <= 0 || grid.depth <= 0)
    {
        return std::nullopt;
    }
    const std::size_t width = static_cast<std::size_t>(grid.width);
    const std::size_t height = static_cast<std::size_t>(grid.height);
    const std::size_t depth = static_cast<std::size_t>(grid.depth);
    if (width > std::numeric_limits<std::size_t>::max() / height)
    {
        return std::nullopt;
    }
    const std::size_t area = width * height;
    if (area > std::numeric_limits<std::size_t>::max() / depth)
    {
        return std::nullopt;
    }
    return area * depth;
}

ValidationIssue MakeTransferIssue(
    const TextureFillPartitionErrorCode code,
    const ValidationSeverity severity,
    const std::string& message)
{
    return MakeValidationIssue(
        TextureFillPartitionErrorCodeName(code),
        severity,
        message);
}

void BlockTransfer(
    TextureFillPartitionTextureTransferResult& result,
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    result.available = false;
    result.status = "blocked";
    result.voxelRgb.clear();
    result.colorSources.clear();
    result.issues.push_back(MakeTransferIssue(
        code,
        ValidationSeverity::Error,
        message));
}

bool IsBinaryMask(const std::vector<std::uint8_t>& mask)
{
    return std::all_of(
        mask.begin(),
        mask.end(),
        [](const std::uint8_t value)
        {
            return value == 0U || value == 1U;
        });
}

bool ValidatePartition(
    const GlobalTextureFillPartitionResult& partition,
    const std::size_t voxelCount,
    TextureFillPartitionTextureTransferResult& result)
{
    if (!partition.available || !partition.partitionPass)
    {
        BlockTransfer(
            result,
            TextureFillPartitionErrorCode::TextureTransferInputInvalid,
            "texture transfer requires an available validated partition");
        return false;
    }
    if (partition.modelMask.values.size() != voxelCount
        || partition.textureSurfaceMask.values.size() != voxelCount
        || partition.modelFillMask.values.size() != voxelCount
        || partition.closestSurfaceReferences.size() != voxelCount)
    {
        BlockTransfer(
            result,
            TextureFillPartitionErrorCode::TextureTransferInputInvalid,
            "texture transfer masks and closest references must match the partition grid");
        return false;
    }
    if (!IsBinaryMask(partition.modelMask.values)
        || !IsBinaryMask(partition.textureSurfaceMask.values)
        || !IsBinaryMask(partition.modelFillMask.values))
    {
        BlockTransfer(
            result,
            TextureFillPartitionErrorCode::TextureTransferInputInvalid,
            "texture transfer requires binary model, texture, and fill masks");
        return false;
    }
    for (std::size_t index{0U}; index < voxelCount; ++index)
    {
        const bool model = partition.modelMask.values.at(index) != 0U;
        const bool texture = partition.textureSurfaceMask.values.at(index) != 0U;
        const bool fill = partition.modelFillMask.values.at(index) != 0U;
        if (texture && fill)
        {
            BlockTransfer(
                result,
                TextureFillPartitionErrorCode::TextureTransferInputInvalid,
                "texture and model-fill masks overlap");
            return false;
        }
        if (texture && !model)
        {
            BlockTransfer(
                result,
                TextureFillPartitionErrorCode::TextureOutsideModel,
                "texture-surface mask contains a voxel outside the model");
            return false;
        }
        if ((model && !texture && !fill) || (!model && fill))
        {
            BlockTransfer(
                result,
                TextureFillPartitionErrorCode::TextureTransferInputInvalid,
                "texture transfer requires an exact texture/fill partition");
            return false;
        }
    }
    return true;
}

std::uint64_t EstimateTextureBytes(const TextureImage& image)
{
    return static_cast<std::uint64_t>(image.width)
        * static_cast<std::uint64_t>(image.height)
        * 4ULL;
}

TexCoord InterpolateReferenceUv(
    const std::array<TexCoord, 3>& uv,
    const std::array<double, 3>& barycentric)
{
    TexCoord result;
    for (std::size_t index{0U}; index < 3U; ++index)
    {
        result.u += uv.at(index).u * barycentric.at(index);
        result.v += uv.at(index).v * barycentric.at(index);
    }
    return result;
}

std::array<std::uint8_t, 3> SelectNonTextureColor(
    const MaterialInfo* material,
    const TextureFillPartitionTextureTransferRequest& request,
    TextureFillColorSource& source,
    TextureFillPartitionTextureTransferStats& stats)
{
    if (material != nullptr && material->has_diffuse)
    {
        source = TextureFillColorSource::MaterialDiffuse;
        ++stats.materialDiffuseCount;
        return material->diffuse_rgb;
    }
    source = TextureFillColorSource::Fallback;
    ++stats.fallbackCount;
    return request.fallbackRgb;
}

bool IsFailFast(const TextureFillPartitionTextureTransferRequest& request)
{
    return request.missingTexturePolicy == "fail_fast";
}

void AppendWarningIfNeeded(
    TextureFillPartitionTextureTransferResult& result,
    const std::uint64_t count,
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    if (count == 0U)
    {
        return;
    }
    result.issues.push_back(MakeTransferIssue(
        code,
        ValidationSeverity::Warning,
        message + ": " + std::to_string(count)));
}

}  // namespace

std::string TextureFillColorSourceName(const TextureFillColorSource source)
{
    switch (source)
    {
    case TextureFillColorSource::NotColored:
        return "not_colored";
    case TextureFillColorSource::Texture:
        return "texture";
    case TextureFillColorSource::MaterialDiffuse:
        return "material_diffuse";
    case TextureFillColorSource::Fallback:
        return "fallback";
    }
    return "not_colored";
}

TextureFillPartitionTextureTransferResult TransferTextureFillPartition(
    const TextureFillPartitionTextureTransferRequest& request)
{
    TextureFillPartitionTextureTransferResult result;
    if (request.adaptedMesh == nullptr || request.partition == nullptr)
    {
        BlockTransfer(
            result,
            TextureFillPartitionErrorCode::TextureTransferInputInvalid,
            "texture transfer requires adapted mesh and partition inputs");
        return result;
    }
    if (request.missingTexturePolicy != "warn_and_fallback"
        && request.missingTexturePolicy != "fail_fast")
    {
        BlockTransfer(
            result,
            TextureFillPartitionErrorCode::TextureTransferInputInvalid,
            "unsupported missing texture policy: " + request.missingTexturePolicy);
        return result;
    }

    const AdaptedTriangleMesh& adapted = *request.adaptedMesh;
    const GlobalTextureFillPartitionResult& partition = *request.partition;
    const std::optional<std::size_t> voxelCount = VoxelCount(partition.grid);
    if (!voxelCount.has_value())
    {
        BlockTransfer(
            result,
            TextureFillPartitionErrorCode::TextureTransferInputInvalid,
            "texture transfer partition grid dimensions are invalid");
        return result;
    }
    if (adapted.mesh.triangles.size() != adapted.triangle_attributes.size())
    {
        BlockTransfer(
            result,
            TextureFillPartitionErrorCode::TextureTransferInputInvalid,
            "adapted mesh triangle attributes do not match accepted triangles");
        return result;
    }
    if (!ValidatePartition(partition, *voxelCount, result))
    {
        return result;
    }

    const auto transferStart = Clock::now();
    result.voxelRgb.assign(*voxelCount, {255, 255, 255});
    result.colorSources.assign(*voxelCount, TextureFillColorSource::NotColored);
    SurfaceAttributeMap attributes(adapted);
    std::map<std::string, TextureImage> textureCache;
    std::set<std::string> failedTextureCache;

    for (std::size_t index{0U}; index < *voxelCount; ++index)
    {
        if (partition.textureSurfaceMask.values.at(index) == 0U)
        {
            continue;
        }
        ++result.stats.textureSurfaceVoxels;
        const TextureFillClosestSurfaceReference& reference =
            partition.closestSurfaceReferences.at(index);
        if (!reference.valid || !std::isfinite(reference.distanceMm))
        {
            BlockTransfer(
                result,
                TextureFillPartitionErrorCode::TextureReferenceMissing,
                "texture-surface voxel has no finite closest-surface reference");
            return result;
        }
        if (reference.triangleIndex >= adapted.mesh.triangles.size()
            || reference.triangleIndex >= adapted.triangle_attributes.size())
        {
            BlockTransfer(
                result,
                TextureFillPartitionErrorCode::TextureTriangleOutOfRange,
                "closest-surface reference triangle is outside the adapted mesh");
            return result;
        }
        ++result.stats.reusedReferenceCount;
        result.stats.maxTransferDistanceMm = std::max(
            result.stats.maxTransferDistanceMm,
            reference.distanceMm);
        if (reference.tieCandidateCount > 0U)
        {
            ++result.stats.medialAxisTieCount;
        }

        const SurfaceTriangleAttributes& triangleAttributes =
            attributes.TriangleAttributes(reference.triangleIndex);
        const MaterialInfo* material = attributes.FindMaterial(
            triangleAttributes.material_name);
        TextureFillColorSource source = TextureFillColorSource::Fallback;
        std::array<std::uint8_t, 3> rgb{255, 255, 255};

        if (!triangleAttributes.has_uv)
        {
            ++result.stats.missingUvCount;
            if (IsFailFast(request))
            {
                BlockTransfer(
                    result,
                    TextureFillPartitionErrorCode::TextureMissingUv,
                    "texture-surface triangle has no UV coordinates");
                return result;
            }
            rgb = SelectNonTextureColor(material, request, source, result.stats);
        }
        else if (material != nullptr && material->has_texture)
        {
            const std::string textureKey =
                material->diffuse_texture_path.lexically_normal().generic_string();
            if (!material->texture_exists || textureKey.empty())
            {
                ++result.stats.missingTextureCount;
                if (IsFailFast(request))
                {
                    BlockTransfer(
                        result,
                        TextureFillPartitionErrorCode::TextureMissingResource,
                        "texture resource is missing for a texture-surface triangle");
                    return result;
                }
                rgb = SelectNonTextureColor(material, request, source, result.stats);
            }
            else if (failedTextureCache.contains(textureKey))
            {
                ++result.stats.missingTextureCount;
                ++result.stats.textureSampleFailureCount;
                rgb = SelectNonTextureColor(material, request, source, result.stats);
            }
            else
            {
                try
                {
                    auto found = textureCache.find(textureKey);
                    if (found == textureCache.end())
                    {
                        const TextureImage image = load_texture_image(
                            material->diffuse_texture_path);
                        result.stats.textureCacheBytes += EstimateTextureBytes(image);
                        found = textureCache.emplace(textureKey, image).first;
                        ++result.stats.textureCacheMisses;
                    }
                    else
                    {
                        ++result.stats.textureCacheHits;
                    }
                    const TexCoord uv = InterpolateReferenceUv(
                        triangleAttributes.uv,
                        reference.barycentric);
                    bool uvOutOfRange{false};
                    rgb = sample_texture_rgb(
                        found->second,
                        uv.u,
                        uv.v,
                        request.textureSample,
                        uvOutOfRange);
                    if (uvOutOfRange)
                    {
                        ++result.stats.uvOutOfRangeCount;
                    }
                    source = TextureFillColorSource::Texture;
                    ++result.stats.sampledTextureCount;
                }
                catch (const std::exception& error)
                {
                    failedTextureCache.insert(textureKey);
                    ++result.stats.textureCacheMisses;
                    ++result.stats.missingTextureCount;
                    ++result.stats.textureSampleFailureCount;
                    if (IsFailFast(request))
                    {
                        BlockTransfer(
                            result,
                            TextureFillPartitionErrorCode::TextureSampleFailed,
                            std::string{"texture load or sample failed: "} + error.what());
                        return result;
                    }
                    rgb = SelectNonTextureColor(material, request, source, result.stats);
                }
            }
        }
        else if (material != nullptr && material->has_diffuse)
        {
            rgb = SelectNonTextureColor(material, request, source, result.stats);
        }
        else
        {
            ++result.stats.missingTextureCount;
            if (IsFailFast(request))
            {
                BlockTransfer(
                    result,
                    TextureFillPartitionErrorCode::TextureMissingResource,
                    "texture-surface triangle has no usable material resource");
                return result;
            }
            rgb = SelectNonTextureColor(material, request, source, result.stats);
        }

        result.voxelRgb.at(index) = rgb;
        result.colorSources.at(index) = source;
    }

    for (std::size_t index{0U}; index < *voxelCount; ++index)
    {
        if (partition.textureSurfaceMask.values.at(index) == 0U
            && result.colorSources.at(index) != TextureFillColorSource::NotColored)
        {
            ++result.stats.outsideColoredCount;
        }
    }
    if (result.stats.outsideColoredCount > 0U)
    {
        BlockTransfer(
            result,
            TextureFillPartitionErrorCode::TextureOutsideModel,
            "texture transfer colored voxels outside the exact texture-surface mask");
        return result;
    }

    result.stats.loadedTextureCount = static_cast<std::uint64_t>(
        textureCache.size());
    AppendWarningIfNeeded(
        result,
        result.stats.missingUvCount,
        TextureFillPartitionErrorCode::TextureMissingUv,
        "texture-surface voxels used fallback because UV coordinates were missing");
    AppendWarningIfNeeded(
        result,
        result.stats.missingTextureCount - result.stats.textureSampleFailureCount,
        TextureFillPartitionErrorCode::TextureMissingResource,
        "texture-surface voxels used fallback because texture resources were missing");
    AppendWarningIfNeeded(
        result,
        result.stats.textureSampleFailureCount,
        TextureFillPartitionErrorCode::TextureSampleFailed,
        "texture-surface voxels used fallback because texture loading or sampling failed");
    result.stats.transferMs = std::chrono::duration<double, std::milli>(
        Clock::now() - transferStart).count();
    result.available = true;
    result.status = "diagnostic";
    return result;
}

}  // namespace slicer_core
