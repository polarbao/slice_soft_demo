#include "slicer_core/diagnostics/TextureFillPartitionClosureAdapter.h"

#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr std::array<const char*, 6> kChannelOrder{"R", "G", "B", "W", "S", "V"};

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

ValidationIssue MakeClosureIssue(
    const TextureFillPartitionErrorCode code,
    const ValidationSeverity severity,
    const std::string& message)
{
    return MakeValidationIssue(
        TextureFillPartitionErrorCodeName(code),
        severity,
        message);
}

void BlockClosure(
    TextureFillPartitionClosureAdapterResult& result,
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    result.available = false;
    result.status = "blocked";
    result.layers.clear();
    result.issues.push_back(MakeClosureIssue(
        code,
        ValidationSeverity::Error,
        message));
}

bool HasFixedChannelOrder(const std::array<std::string, 6>& order)
{
    for (std::size_t index{0U}; index < kChannelOrder.size(); ++index)
    {
        if (order.at(index) != kChannelOrder.at(index))
        {
            return false;
        }
    }
    return true;
}

bool IsEmptyPixel(
    const std::vector<std::uint8_t>& channels,
    const std::size_t pixelIndex)
{
    for (std::size_t channel{0U}; channel < kChannelOrder.size(); ++channel)
    {
        if (channels.at(pixelIndex * kChannelOrder.size() + channel) != 255U)
        {
            return false;
        }
    }
    return true;
}

bool ValidatePartition(
    const GlobalTextureFillPartitionResult& partition,
    const std::size_t voxelCount,
    TextureFillPartitionClosureAdapterResult& result)
{
    if (!partition.available || !partition.partitionPass)
    {
        BlockClosure(
            result,
            TextureFillPartitionErrorCode::ClosureAdapterInputInvalid,
            "closure adapter requires an available validated partition");
        return false;
    }
    if (partition.modelMask.values.size() != voxelCount
        || partition.textureSurfaceMask.values.size() != voxelCount
        || partition.modelFillMask.values.size() != voxelCount
        || !IsBinaryMask(partition.modelMask.values)
        || !IsBinaryMask(partition.textureSurfaceMask.values)
        || !IsBinaryMask(partition.modelFillMask.values))
    {
        BlockClosure(
            result,
            TextureFillPartitionErrorCode::ClosureMaskInvalid,
            "closure adapter partition masks must be binary and match the grid");
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
            BlockClosure(
                result,
                TextureFillPartitionErrorCode::ClosureMaskInvalid,
                "closure adapter requires texture XOR fill ownership for each model voxel");
            return false;
        }
    }
    return true;
}

bool ValidateComposerRoot(
    const GlobalTextureFillPartitionResult& partition,
    const TextureFillPartitionDiagnosticComposerResult& composer,
    TextureFillPartitionClosureAdapterResult& result)
{
    if (!composer.available || composer.status != "diagnostic")
    {
        BlockClosure(
            result,
            TextureFillPartitionErrorCode::ClosureAdapterInputInvalid,
            "closure adapter requires an available diagnostic composer result");
        return false;
    }
    if (composer.width != partition.grid.width
        || composer.height != partition.grid.height
        || composer.depth != partition.grid.depth
        || composer.layers.size() != static_cast<std::size_t>(partition.grid.depth))
    {
        BlockClosure(
            result,
            TextureFillPartitionErrorCode::ClosureAdapterInputInvalid,
            "closure adapter composer dimensions must match the partition grid");
        return false;
    }
    if (!HasFixedChannelOrder(composer.channelOrder))
    {
        BlockClosure(
            result,
            TextureFillPartitionErrorCode::ClosureChannelOrderInvalid,
            "closure adapter requires channel order R G B W S V");
        return false;
    }
    return true;
}

}  // namespace

TextureFillPartitionClosureAdapterResult AdaptTextureFillPartitionClosure(
    const TextureFillPartitionClosureAdapterRequest& request)
{
    TextureFillPartitionClosureAdapterResult result;
    if (request.partition == nullptr || request.composer == nullptr)
    {
        BlockClosure(
            result,
            TextureFillPartitionErrorCode::ClosureAdapterInputInvalid,
            "closure adapter requires partition and composer inputs");
        return result;
    }
    if ((request.connectivity != 4 && request.connectivity != 8)
        || request.maxGapPx <= 0)
    {
        BlockClosure(
            result,
            TextureFillPartitionErrorCode::ClosureAdapterInputInvalid,
            "closure adapter connectivity and gap radius are invalid");
        return result;
    }

    const GlobalTextureFillPartitionResult& partition = *request.partition;
    const TextureFillPartitionDiagnosticComposerResult& composer = *request.composer;
    const std::optional<std::size_t> voxelCount = VoxelCount(partition.grid);
    if (!voxelCount.has_value())
    {
        BlockClosure(
            result,
            TextureFillPartitionErrorCode::ClosureAdapterInputInvalid,
            "closure adapter partition grid dimensions are invalid");
        return result;
    }
    if (!ValidatePartition(partition, *voxelCount, result)
        || !ValidateComposerRoot(partition, composer, result))
    {
        return result;
    }

    const std::uint64_t modelVoxelCount = static_cast<std::uint64_t>(
        std::count(partition.modelMask.values.begin(), partition.modelMask.values.end(), 1U));
    const std::uint64_t textureVoxelCount = static_cast<std::uint64_t>(
        std::count(
            partition.textureSurfaceMask.values.begin(),
            partition.textureSurfaceMask.values.end(),
            1U));
    const std::uint64_t fillVoxelCount = static_cast<std::uint64_t>(
        std::count(
            partition.modelFillMask.values.begin(),
            partition.modelFillMask.values.end(),
            1U));
    result.allTexture = modelVoxelCount > 0U
        && textureVoxelCount == modelVoxelCount
        && fillVoxelCount == 0U;
    if (result.allTexture)
    {
        result.colorFillApplicability = "not_applicable";
        result.allTextureReason = "all_texture_partition";
    }

    const std::size_t layerArea = static_cast<std::size_t>(partition.grid.width)
        * static_cast<std::size_t>(partition.grid.height);
    result.layers.reserve(composer.layers.size());
    for (int layerIndex{0}; layerIndex < partition.grid.depth; ++layerIndex)
    {
        const TextureFillPartitionDiagnosticLayer& layer = composer.layers.at(
            static_cast<std::size_t>(layerIndex));
        const double expectedZ = partition.grid.originZMm
            + (static_cast<double>(layerIndex) + 0.5)
                * partition.grid.spacingZMm;
        const double zTolerance = std::max(
            1.0e-9,
            std::abs(partition.grid.spacingZMm) * 1.0e-9);
        if (layer.layerIndex != layerIndex
            || !std::isfinite(layer.zMm)
            || std::abs(layer.zMm - expectedZ) > zTolerance)
        {
            BlockClosure(
                result,
                TextureFillPartitionErrorCode::ClosureLayerOrderInvalid,
                "closure adapter layer index or Z coordinate is not the true grid order");
            return result;
        }
        if (layer.textureSurfaceMask.size() != layerArea
            || layer.modelFillMask.size() != layerArea
            || !IsBinaryMask(layer.textureSurfaceMask)
            || !IsBinaryMask(layer.modelFillMask))
        {
            BlockClosure(
                result,
                TextureFillPartitionErrorCode::ClosureMaskInvalid,
                "closure adapter layer masks must be binary and match the layer grid");
            return result;
        }
        if (layer.composed.width != partition.grid.width
            || layer.composed.height != partition.grid.height
            || layer.composed.channels.size() != layerArea * kChannelOrder.size())
        {
            BlockClosure(
                result,
                TextureFillPartitionErrorCode::ClosureAdapterInputInvalid,
                "closure adapter composed layer dimensions or channel data are invalid");
            return result;
        }
        if (!HasFixedChannelOrder(layer.composed.channel_order))
        {
            BlockClosure(
                result,
                TextureFillPartitionErrorCode::ClosureChannelOrderInvalid,
                "closure adapter composed layer channel order is invalid");
            return result;
        }

        MaterialClosureSemanticLayerInput semantic;
        semantic.layerIndex = layerIndex;
        semantic.zMm = layer.zMm;
        semantic.widthPx = partition.grid.width;
        semantic.heightPx = partition.grid.height;
        semantic.textureSurfaceMask = layer.textureSurfaceMask;
        semantic.modelFillMask = layer.modelFillMask;
        semantic.modelMaterialMask.assign(layerArea, 0U);
        semantic.supportFillMask.assign(layerArea, 0U);
        semantic.internalVoidSupportMask.assign(layerArea, 0U);
        semantic.surfaceVarnishMask.assign(layerArea, 0U);
        semantic.outerVarnishShellMask.assign(layerArea, 0U);
        semantic.modelEnvelopeMask.assign(layerArea, 0U);
        semantic.supportRequiredMask.assign(layerArea, 0U);
        semantic.expectedOccupiedDomainMask.assign(layerArea, 0U);
        semantic.layerEmptyMask.assign(layerArea, 0U);

        const std::size_t layerBegin = static_cast<std::size_t>(layerIndex)
            * layerArea;
        std::uint64_t modelDomainGapVoxels{0U};
        for (std::size_t pixelIndex{0U}; pixelIndex < layerArea; ++pixelIndex)
        {
            const std::size_t voxelIndex = layerBegin + pixelIndex;
            const std::uint8_t expectedTexture =
                partition.textureSurfaceMask.values.at(voxelIndex);
            const std::uint8_t expectedFill =
                partition.modelFillMask.values.at(voxelIndex);
            const std::uint8_t model = partition.modelMask.values.at(voxelIndex);
            if (layer.textureSurfaceMask.at(pixelIndex) != expectedTexture
                || layer.modelFillMask.at(pixelIndex) != expectedFill)
            {
                BlockClosure(
                    result,
                    TextureFillPartitionErrorCode::ClosureMaskInvalid,
                    "closure adapter composer masks do not match the exact partition");
                return result;
            }
            semantic.modelMaterialMask.at(pixelIndex) = model;
            semantic.modelEnvelopeMask.at(pixelIndex) = model;
            semantic.expectedOccupiedDomainMask.at(pixelIndex) = model;
            const bool empty = IsEmptyPixel(layer.composed.channels, pixelIndex);
            semantic.layerEmptyMask.at(pixelIndex) = empty ? 1U : 0U;
            if (model != 0U && empty)
            {
                ++modelDomainGapVoxels;
            }
        }

        MaterialClosureSemanticLayerAnalysis analysis;
        try
        {
            analysis = AnalyzeMaterialClosureSemanticLayer(
                semantic,
                request.connectivity,
                request.maxGapPx);
        }
        catch (const std::exception& error)
        {
            BlockClosure(
                result,
                TextureFillPartitionErrorCode::ClosureAdapterInputInvalid,
                std::string{"12D semantic detector rejected 12E evidence: "}
                    + error.what());
            return result;
        }

        TextureFillPartitionClosureLayerResult layerResult;
        layerResult.layerIndex = layerIndex;
        layerResult.zMm = layer.zMm;
        layerResult.colorFillGapVoxels = static_cast<std::uint64_t>(
            analysis.summary.colorFillGapPixels);
        layerResult.modelDomainGapVoxels = modelDomainGapVoxels;
        layerResult.semantic = analysis.summary;
        result.totalColorFillGapVoxels += layerResult.colorFillGapVoxels;
        result.totalModelDomainGapVoxels += layerResult.modelDomainGapVoxels;
        result.layers.push_back(std::move(layerResult));
    }

    result.available = true;
    result.source = "semantic_masks";
    result.confidence = "exact";
    result.status = "diagnostic";
    if (result.totalModelDomainGapVoxels > 0U)
    {
        result.status = "fail";
        result.issues.push_back(MakeClosureIssue(
            TextureFillPartitionErrorCode::ClosureModelDomainGap,
            ValidationSeverity::Error,
            "empty RGBWSV voxels exist inside the exact model domain: "
                + std::to_string(result.totalModelDomainGapVoxels)));
    }
    if (result.totalColorFillGapVoxels > 0U)
    {
        result.status = "fail";
        result.issues.push_back(MakeClosureIssue(
            TextureFillPartitionErrorCode::ClosureColorFillGap,
            ValidationSeverity::Error,
            "12D exact detector found texture/model-fill gaps: "
                + std::to_string(result.totalColorFillGapVoxels)));
    }
    return result;
}

}  // namespace slicer_core
