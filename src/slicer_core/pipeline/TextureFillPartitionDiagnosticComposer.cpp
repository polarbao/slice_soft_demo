#include "slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.h"

#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>

namespace slicer_core
{
namespace
{

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

ValidationIssue MakeComposerIssue(
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    return MakeValidationIssue(
        TextureFillPartitionErrorCodeName(code),
        ValidationSeverity::Error,
        message);
}

void BlockComposer(
    TextureFillPartitionDiagnosticComposerResult& result,
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    result.available = false;
    result.status = "blocked";
    result.layers.clear();
    result.issues.push_back(MakeComposerIssue(code, message));
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

bool ValidateExactPartition(
    const GlobalTextureFillPartitionResult& partition,
    const std::size_t voxelCount,
    TextureFillPartitionDiagnosticComposerResult& result)
{
    if (!partition.available || !partition.partitionPass)
    {
        BlockComposer(
            result,
            TextureFillPartitionErrorCode::DiagnosticComposerPartitionInvalid,
            "diagnostic composer requires an available validated partition");
        return false;
    }
    if (partition.modelMask.values.size() != voxelCount
        || partition.textureSurfaceMask.values.size() != voxelCount
        || partition.modelFillMask.values.size() != voxelCount
        || !IsBinaryMask(partition.modelMask.values)
        || !IsBinaryMask(partition.textureSurfaceMask.values)
        || !IsBinaryMask(partition.modelFillMask.values))
    {
        BlockComposer(
            result,
            TextureFillPartitionErrorCode::DiagnosticComposerPartitionInvalid,
            "diagnostic composer masks must be binary and match the partition grid");
        return false;
    }
    for (std::size_t index{0U}; index < voxelCount; ++index)
    {
        const bool model = partition.modelMask.values.at(index) != 0U;
        const bool texture = partition.textureSurfaceMask.values.at(index) != 0U;
        const bool fill = partition.modelFillMask.values.at(index) != 0U;
        if ((texture && fill)
            || (texture && !model)
            || (fill && !model)
            || (model && !texture && !fill))
        {
            BlockComposer(
                result,
                TextureFillPartitionErrorCode::DiagnosticComposerPartitionInvalid,
                "diagnostic composer requires exact texture XOR fill ownership for every model voxel");
            return false;
        }
    }
    return true;
}

bool ValidateTransfer(
    const GlobalTextureFillPartitionResult& partition,
    const TextureFillPartitionTextureTransferResult& transfer,
    const std::size_t voxelCount,
    TextureFillPartitionDiagnosticComposerResult& result)
{
    if (!transfer.available || transfer.status != "diagnostic"
        || transfer.voxelRgb.size() != voxelCount
        || transfer.colorSources.size() != voxelCount
        || transfer.stats.outsideColoredCount != 0U)
    {
        BlockComposer(
            result,
            TextureFillPartitionErrorCode::DiagnosticComposerInputInvalid,
            "diagnostic composer requires a complete texture transfer with no outside colors");
        return false;
    }
    for (std::size_t index{0U}; index < voxelCount; ++index)
    {
        const bool texture = partition.textureSurfaceMask.values.at(index) != 0U;
        const bool colored = transfer.colorSources.at(index)
            != TextureFillColorSource::NotColored;
        if (texture != colored)
        {
            BlockComposer(
                result,
                TextureFillPartitionErrorCode::DiagnosticComposerInputInvalid,
                "texture transfer color ownership does not match the exact texture mask");
            return false;
        }
    }
    return true;
}

bool IsSupportedFillMaterial(const std::string& material)
{
    return material == "white" || material == "varnish" || material == "rgb";
}

std::size_t ChannelIndex(
    const std::size_t pixelIndex,
    const MaterialChannelOffset channel)
{
    return pixelIndex * static_cast<std::size_t>(MaterialChannelCount())
        + static_cast<std::size_t>(channel);
}

}  // namespace

TextureFillPartitionDiagnosticComposerResult ComposeTextureFillPartitionDiagnostic(
    const TextureFillPartitionDiagnosticComposerRequest& request)
{
    TextureFillPartitionDiagnosticComposerResult result;
    if (request.partition == nullptr || request.transfer == nullptr)
    {
        BlockComposer(
            result,
            TextureFillPartitionErrorCode::DiagnosticComposerInputInvalid,
            "diagnostic composer requires partition and texture-transfer inputs");
        return result;
    }
    if (!IsSupportedFillMaterial(request.modelFillMaterial))
    {
        BlockComposer(
            result,
            TextureFillPartitionErrorCode::DiagnosticComposerInputInvalid,
            "unsupported model-fill material: " + request.modelFillMaterial);
        return result;
    }

    const GlobalTextureFillPartitionResult& partition = *request.partition;
    const TextureFillPartitionTextureTransferResult& transfer = *request.transfer;
    const std::optional<std::size_t> voxelCount = VoxelCount(partition.grid);
    if (!voxelCount.has_value())
    {
        BlockComposer(
            result,
            TextureFillPartitionErrorCode::DiagnosticComposerInputInvalid,
            "diagnostic composer partition grid dimensions are invalid");
        return result;
    }
    if (!ValidateExactPartition(partition, *voxelCount, result)
        || !ValidateTransfer(partition, transfer, *voxelCount, result))
    {
        return result;
    }

    result.width = partition.grid.width;
    result.height = partition.grid.height;
    result.depth = partition.grid.depth;
    const std::size_t layerArea = static_cast<std::size_t>(result.width)
        * static_cast<std::size_t>(result.height);
    result.layers.reserve(static_cast<std::size_t>(result.depth));

    for (int layerIndex{0}; layerIndex < result.depth; ++layerIndex)
    {
        TextureFillPartitionDiagnosticLayer layer;
        layer.layerIndex = layerIndex;
        layer.zMm = partition.grid.originZMm
            + (static_cast<double>(layerIndex) + 0.5)
                * partition.grid.spacingZMm;
        layer.textureSurfaceMask.assign(layerArea, 0U);
        layer.modelFillMask.assign(layerArea, 0U);

        MaterialChannelComposerInput input;
        input.width = result.width;
        input.height = result.height;
        input.surface_shell_mask.assign(layerArea, 0U);
        input.surface_rgb.assign(layerArea, {255, 255, 255});
        input.model_rgb = request.modelFillRgb;
        input.white_value = request.modelFillValue;
        input.varnish_value = request.modelFillValue;
        if (request.modelFillMaterial == "white")
        {
            input.white_mask.assign(layerArea, 0U);
        }
        else if (request.modelFillMaterial == "varnish")
        {
            input.varnish_mask.assign(layerArea, 0U);
        }
        else
        {
            input.model_mask.assign(layerArea, 0U);
        }

        const std::size_t layerBegin = static_cast<std::size_t>(layerIndex)
            * layerArea;
        for (std::size_t pixelIndex{0U}; pixelIndex < layerArea; ++pixelIndex)
        {
            const std::size_t voxelIndex = layerBegin + pixelIndex;
            const bool texture = partition.textureSurfaceMask.values.at(voxelIndex)
                != 0U;
            const bool fill = partition.modelFillMask.values.at(voxelIndex)
                != 0U;
            if (texture)
            {
                layer.textureSurfaceMask.at(pixelIndex) = 1U;
                input.surface_shell_mask.at(pixelIndex) = 1U;
                input.surface_rgb.at(pixelIndex) = transfer.voxelRgb.at(voxelIndex);
                ++result.stats.textureSurfaceVoxels;
            }
            if (fill)
            {
                layer.modelFillMask.at(pixelIndex) = 1U;
                ++result.stats.modelFillVoxels;
                if (request.modelFillMaterial == "white")
                {
                    input.white_mask.at(pixelIndex) = 1U;
                    ++result.stats.modelFillWhiteVoxels;
                }
                else if (request.modelFillMaterial == "varnish")
                {
                    input.varnish_mask.at(pixelIndex) = 1U;
                    ++result.stats.modelFillVarnishVoxels;
                }
                else
                {
                    input.model_mask.at(pixelIndex) = 1U;
                    ++result.stats.modelFillRgbVoxels;
                }
            }
        }

        layer.composed = ComposeMaterialChannels(input);
        if (!layer.composed.error.empty())
        {
            BlockComposer(
                result,
                TextureFillPartitionErrorCode::DiagnosticComposerInputInvalid,
                "material channel composer rejected a diagnostic layer: "
                    + layer.composed.error);
            return result;
        }
        result.stats.emptyVoxels += static_cast<std::uint64_t>(
            layer.composed.stats.empty_pixels);
        for (std::size_t pixelIndex{0U}; pixelIndex < layerArea; ++pixelIndex)
        {
            if (layer.composed.channels.at(ChannelIndex(
                    pixelIndex,
                    MaterialChannelOffset::S)) != 255U)
            {
                ++result.stats.supportPrintVoxels;
            }
        }
        result.layers.push_back(std::move(layer));
    }

    if (result.stats.supportPrintVoxels != 0U)
    {
        BlockComposer(
            result,
            TextureFillPartitionErrorCode::DiagnosticComposerInputInvalid,
            "Stage 12E diagnostic composer must leave the support channel empty");
        return result;
    }
    result.available = true;
    result.status = "diagnostic";
    return result;
}

}  // namespace slicer_core
