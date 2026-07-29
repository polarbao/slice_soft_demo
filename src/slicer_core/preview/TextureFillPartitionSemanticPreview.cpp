#include "slicer_core/preview/TextureFillPartitionSemanticPreview.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace slicer_core
{
namespace
{

constexpr std::size_t kChannelCount{6U};
constexpr std::size_t kSupportChannel{4U};
constexpr std::size_t kVarnishChannel{5U};

TextureFillPartitionSemanticPreviewResult FailResult(
    const std::string& code,
    const std::string& message)
{
    TextureFillPartitionSemanticPreviewResult result;
    result.errorcode = code;
    result.message = message;
    return result;
}

bool IsPositiveFinite(const double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool CheckedPixelCount(
    const std::uint32_t width,
    const std::uint32_t height,
    std::size_t* count)
{
    if (count == nullptr || width == 0U || height == 0U)
    {
        return false;
    }
    const std::size_t safeWidth =
        static_cast<std::size_t>(width);
    const std::size_t safeHeight =
        static_cast<std::size_t>(height);
    if (safeWidth
        > std::numeric_limits<std::size_t>::max()
            / safeHeight)
    {
        return false;
    }
    *count = safeWidth * safeHeight;
    return true;
}

bool CheckedVoxelCount(
    const TextureFillPartitionGridSpec& grid,
    std::size_t* count)
{
    if (count == nullptr
        || grid.width <= 0
        || grid.height <= 0
        || grid.depth <= 0)
    {
        return false;
    }
    const std::size_t width =
        static_cast<std::size_t>(grid.width);
    const std::size_t height =
        static_cast<std::size_t>(grid.height);
    const std::size_t depth =
        static_cast<std::size_t>(grid.depth);
    if (width
        > std::numeric_limits<std::size_t>::max()
            / height)
    {
        return false;
    }
    const std::size_t area = width * height;
    if (area
        > std::numeric_limits<std::size_t>::max()
            / depth)
    {
        return false;
    }
    *count = area * depth;
    return true;
}

bool IsBinaryMask(
    const std::vector<std::uint8_t>& mask)
{
    return std::all_of(
        mask.begin(),
        mask.end(),
        [](const std::uint8_t value)
        {
            return value == 0U || value == 1U;
        });
}

int CellIndex(
    const double coordinate,
    const double origin,
    const double spacing)
{
    return static_cast<int>(
        std::floor((coordinate - origin) / spacing));
}

}  // namespace

TextureFillPartitionSemanticPreviewResult
BuildTextureFillPartitionSemanticPreview(
    const TextureFillPartitionSemanticPreviewRequest& request)
{
    if (request.partition == nullptr
        || request.productionlayer == nullptr)
    {
        return FailResult(
            "SEMANTIC_PREVIEW_EVIDENCE_MISSING",
            "diagnostic partition and production TIFF layer are required");
    }

    const GlobalTextureFillPartitionResult& partition =
        *request.partition;
    const RgbwsvLayerBuffer& production =
        *request.productionlayer;
    if (!partition.available || !partition.partitionPass)
    {
        return FailResult(
            "SEMANTIC_PREVIEW_PARTITION_INVALID",
            "diagnostic partition is unavailable or blocked");
    }
    if (!IsPositiveFinite(partition.grid.spacingXMm)
        || !IsPositiveFinite(partition.grid.spacingYMm)
        || !IsPositiveFinite(partition.grid.spacingZMm)
        || !std::isfinite(partition.grid.originXMm)
        || !std::isfinite(partition.grid.originYMm)
        || !std::isfinite(partition.grid.originZMm))
    {
        return FailResult(
            "SEMANTIC_PREVIEW_DIAGNOSTIC_GRID_INVALID",
            "diagnostic partition grid is invalid");
    }
    if (!IsPositiveFinite(production.pixelsizexmm)
        || !IsPositiveFinite(production.pixelsizeymm)
        || !IsPositiveFinite(production.layerthicknessmm)
        || !std::isfinite(production.originxmm)
        || !std::isfinite(production.originymm)
        || !std::isfinite(production.originzmm)
        || !std::isfinite(production.zMm))
    {
        return FailResult(
            "SEMANTIC_PREVIEW_PRODUCTION_GRID_INVALID",
            "production TIFF physical grid is invalid");
    }

    std::size_t pixelCount{0U};
    std::size_t voxelCount{0U};
    if (!CheckedPixelCount(
            production.width,
            production.height,
            &pixelCount)
        || pixelCount
            > std::numeric_limits<std::size_t>::max()
                / kChannelCount
        || production.pixels.size()
            != pixelCount * kChannelCount
        || !CheckedVoxelCount(partition.grid, &voxelCount)
        || partition.modelMask.values.size()
            != voxelCount
        || partition.textureSurfaceMask.values.size()
            != voxelCount
        || partition.modelFillMask.values.size()
            != voxelCount
        || !IsBinaryMask(partition.modelMask.values)
        || !IsBinaryMask(
            partition.textureSurfaceMask.values)
        || !IsBinaryMask(partition.modelFillMask.values))
    {
        return FailResult(
            "SEMANTIC_PREVIEW_MASK_INVALID",
            "semantic preview masks or production buffer have invalid dimensions");
    }

    TextureFillPartitionSemanticPreviewResult result;
    result.available = true;
    result.status = "diagnostic";
    result.message =
        "same-layer diagnostic semantics are available";
    result.productionsourceidentity =
        production.sourceIdentity;
    result.layerindex = production.layerIndex;
    result.zmm = production.zMm;
    result.width = production.width;
    result.height = production.height;
    result.alltexture =
        partition.widthMetrics.allTexture;
    result.modelmask.assign(pixelCount, 0U);
    result.texturesurfacemask.assign(pixelCount, 0U);
    result.modelfillmask.assign(pixelCount, 0U);
    result.supportmask.assign(pixelCount, 0U);
    result.varnishmask.assign(pixelCount, 0U);

    for (std::size_t pixelIndex{0U};
         pixelIndex < pixelCount;
         ++pixelIndex)
    {
        const std::size_t channelBase =
            pixelIndex * kChannelCount;
        if (production.pixels.at(
                channelBase + kSupportChannel)
            != 255U)
        {
            result.supportmask.at(pixelIndex) = 1U;
            ++result.supportpixels;
        }
        if (production.pixels.at(
                channelBase + kVarnishChannel)
            != 255U)
        {
            result.varnishmask.at(pixelIndex) = 1U;
            ++result.varnishpixels;
        }
    }

    const int voxelZ = CellIndex(
        production.zMm,
        partition.grid.originZMm,
        partition.grid.spacingZMm);
    if (voxelZ < 0 || voxelZ >= partition.grid.depth)
    {
        result.status = "diagnostic_empty";
        result.message =
            "production layer is outside the diagnostic Z grid";
        return result;
    }

    const std::size_t diagnosticLayerArea =
        static_cast<std::size_t>(partition.grid.width)
        * static_cast<std::size_t>(partition.grid.height);
    for (std::uint32_t pixelY{0U};
         pixelY < production.height;
         ++pixelY)
    {
        const double worldY = production.originymm
            + (static_cast<double>(pixelY) + 0.5)
                * production.pixelsizeymm;
        const int voxelY = CellIndex(
            worldY,
            partition.grid.originYMm,
            partition.grid.spacingYMm);
        if (voxelY < 0 || voxelY >= partition.grid.height)
        {
            continue;
        }
        for (std::uint32_t pixelX{0U};
             pixelX < production.width;
             ++pixelX)
        {
            const double worldX = production.originxmm
                + (static_cast<double>(pixelX) + 0.5)
                    * production.pixelsizexmm;
            const int voxelX = CellIndex(
                worldX,
                partition.grid.originXMm,
                partition.grid.spacingXMm);
            if (voxelX < 0
                || voxelX >= partition.grid.width)
            {
                continue;
            }

            const std::size_t pixelIndex =
                static_cast<std::size_t>(pixelY)
                    * static_cast<std::size_t>(
                        production.width)
                + static_cast<std::size_t>(pixelX);
            const std::size_t voxelIndex =
                static_cast<std::size_t>(voxelZ)
                    * diagnosticLayerArea
                + static_cast<std::size_t>(voxelY)
                    * static_cast<std::size_t>(
                        partition.grid.width)
                + static_cast<std::size_t>(voxelX);
            const bool model =
                partition.modelMask.values.at(voxelIndex)
                != 0U;
            const bool texture =
                partition.textureSurfaceMask.values.at(
                    voxelIndex)
                != 0U;
            const bool fill =
                partition.modelFillMask.values.at(
                    voxelIndex)
                != 0U;
            if ((texture && fill)
                || (texture && !model)
                || (fill && !model)
                || (model && !texture && !fill))
            {
                return FailResult(
                    "SEMANTIC_PREVIEW_PARTITION_INVALID",
                    "diagnostic partition violates exact texture/fill ownership");
            }
            if (model)
            {
                result.modelmask.at(pixelIndex) = 1U;
                ++result.modelpixels;
            }
            if (texture)
            {
                result.texturesurfacemask.at(
                    pixelIndex) = 1U;
                ++result.texturesurfacepixels;
            }
            if (fill)
            {
                result.modelfillmask.at(pixelIndex) = 1U;
                ++result.modelfillpixels;
            }
        }
    }

    if (result.modelpixels > 0U)
    {
        result.texturecoverage =
            static_cast<double>(
                result.texturesurfacepixels)
            / static_cast<double>(result.modelpixels);
        result.modelfillcoverage =
            static_cast<double>(
                result.modelfillpixels)
            / static_cast<double>(result.modelpixels);
    }
    return result;
}

}  // namespace slicer_core
