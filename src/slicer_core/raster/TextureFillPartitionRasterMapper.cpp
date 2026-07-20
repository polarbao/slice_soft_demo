#include "slicer_core/raster/TextureFillPartitionRasterMapper.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>

namespace slicer_core
{
namespace
{

using Clock = std::chrono::steady_clock;

std::optional<std::size_t> CheckedVoxelCount(const int width, const int height, const int depth)
{
    if (width <= 0 || height <= 0 || depth <= 0)
    {
        return std::nullopt;
    }
    const std::size_t checkedWidth = static_cast<std::size_t>(width);
    const std::size_t checkedHeight = static_cast<std::size_t>(height);
    const std::size_t checkedDepth = static_cast<std::size_t>(depth);
    if (checkedWidth > std::numeric_limits<std::size_t>::max() / checkedHeight)
    {
        return std::nullopt;
    }
    const std::size_t area = checkedWidth * checkedHeight;
    if (area > std::numeric_limits<std::size_t>::max() / checkedDepth)
    {
        return std::nullopt;
    }
    return area * checkedDepth;
}

bool IsFinitePositive(const double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool IsFiniteOrigin(const double value)
{
    return std::isfinite(value);
}

bool IsValidClassificationGrid(const TextureFillPartitionGridSpec& grid)
{
    return CheckedVoxelCount(grid.width, grid.height, grid.depth).has_value()
        && IsFiniteOrigin(grid.originXMm)
        && IsFiniteOrigin(grid.originYMm)
        && IsFiniteOrigin(grid.originZMm)
        && IsFinitePositive(grid.spacingXMm)
        && IsFinitePositive(grid.spacingYMm)
        && IsFinitePositive(grid.spacingZMm);
}

bool IsValidRasterGrid(const TextureFillPartitionRasterGridSpec& grid)
{
    return CheckedVoxelCount(grid.width, grid.height, grid.depth).has_value()
        && IsFiniteOrigin(grid.originXMm)
        && IsFiniteOrigin(grid.originYMm)
        && IsFiniteOrigin(grid.originZMm)
        && IsFinitePositive(grid.pixelPitchXMm)
        && IsFinitePositive(grid.pixelPitchYMm)
        && IsFinitePositive(grid.layerThicknessMm);
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

bool SameClassificationGrid(
    const TextureFillPartitionGridSpec& left,
    const TextureFillPartitionGridSpec& right)
{
    return left.width == right.width
        && left.height == right.height
        && left.depth == right.depth
        && left.originXMm == right.originXMm
        && left.originYMm == right.originYMm
        && left.originZMm == right.originZMm
        && left.spacingXMm == right.spacingXMm
        && left.spacingYMm == right.spacingYMm
        && left.spacingZMm == right.spacingZMm;
}

ValidationIssue MakeMappingIssue(
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    return MakeValidationIssue(
        TextureFillPartitionErrorCodeName(code),
        ValidationSeverity::Error,
        message);
}

void BlockMapping(
    TextureFillPartitionRasterMappingResult& result,
    const TextureFillPartitionErrorCode code,
    const std::string& message)
{
    result.available = false;
    result.status = "blocked";
    result.layers.clear();
    result.issues.push_back(MakeMappingIssue(code, message));
}

bool ValidatePartition(
    const GlobalTextureFillPartitionResult& partition,
    const std::size_t sourceVoxelCount,
    TextureFillPartitionRasterMappingResult& result)
{
    if (!partition.available || !partition.partitionPass)
    {
        BlockMapping(
            result,
            TextureFillPartitionErrorCode::RasterMappingPartitionInvalid,
            "raster mapping requires an available validated partition");
        return false;
    }
    if (!IsValidClassificationGrid(partition.grid)
        || !SameClassificationGrid(partition.grid, partition.modelMask.grid)
        || !SameClassificationGrid(partition.grid, partition.textureSurfaceMask.grid)
        || !SameClassificationGrid(partition.grid, partition.modelFillMask.grid))
    {
        BlockMapping(
            result,
            TextureFillPartitionErrorCode::RasterMappingPartitionInvalid,
            "partition masks must use one finite positive classification grid");
        return false;
    }
    if (partition.modelMask.values.size() != sourceVoxelCount
        || partition.textureSurfaceMask.values.size() != sourceVoxelCount
        || partition.modelFillMask.values.size() != sourceVoxelCount
        || !IsBinaryMask(partition.modelMask.values)
        || !IsBinaryMask(partition.textureSurfaceMask.values)
        || !IsBinaryMask(partition.modelFillMask.values))
    {
        BlockMapping(
            result,
            TextureFillPartitionErrorCode::RasterMappingPartitionInvalid,
            "partition masks must be binary and match the classification grid");
        return false;
    }
    for (std::size_t index{0U}; index < sourceVoxelCount; ++index)
    {
        const bool model = partition.modelMask.values.at(index) != 0U;
        const bool texture = partition.textureSurfaceMask.values.at(index) != 0U;
        const bool fill = partition.modelFillMask.values.at(index) != 0U;
        if ((texture && fill) || (texture && !model) || (fill && !model)
            || (model && !texture && !fill))
        {
            BlockMapping(
                result,
                TextureFillPartitionErrorCode::RasterMappingPartitionInvalid,
                "source model must be an exact texture/fill complement");
            return false;
        }
    }
    if (partition.widthMetrics.allTexture
        && std::any_of(
            partition.modelFillMask.values.begin(),
            partition.modelFillMask.values.end(),
            [](const std::uint8_t value)
            {
                return value != 0U;
            }))
    {
        BlockMapping(
            result,
            TextureFillPartitionErrorCode::RasterMappingPartitionInvalid,
            "all-texture partition must have an empty model-fill mask");
        return false;
    }
    return true;
}

bool ValidateTransfer(
    const GlobalTextureFillPartitionResult& partition,
    const TextureFillPartitionTextureTransferResult& transfer,
    const std::size_t sourceVoxelCount,
    TextureFillPartitionRasterMappingResult& result)
{
    if (!transfer.available
        || transfer.voxelRgb.size() != sourceVoxelCount
        || transfer.colorSources.size() != sourceVoxelCount)
    {
        BlockMapping(
            result,
            TextureFillPartitionErrorCode::RasterMappingTransferInvalid,
            "texture transfer must be available and aligned to the classification grid");
        return false;
    }
    for (std::size_t index{0U}; index < sourceVoxelCount; ++index)
    {
        const bool texture = partition.textureSurfaceMask.values.at(index) != 0U;
        const bool colored = transfer.colorSources.at(index) != TextureFillColorSource::NotColored;
        if (texture != colored)
        {
            BlockMapping(
                result,
                TextureFillPartitionErrorCode::RasterMappingTransferInvalid,
                "texture colors must exist on and only on the texture-surface mask");
            return false;
        }
    }
    return true;
}

std::size_t FlattenIndex(
    const int width,
    const int height,
    const int x,
    const int y,
    const int z)
{
    return (static_cast<std::size_t>(z) * static_cast<std::size_t>(height)
        + static_cast<std::size_t>(y)) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

std::optional<int> ContainingCell(
    const double worldCoordinate,
    const double sourceOrigin,
    const double sourceSpacing,
    const int sourceSize)
{
    const double localCoordinate = (worldCoordinate - sourceOrigin) / sourceSpacing;
    if (!std::isfinite(localCoordinate)
        || localCoordinate < 0.0
        || localCoordinate >= static_cast<double>(sourceSize))
    {
        return std::nullopt;
    }
    return static_cast<int>(std::floor(localCoordinate));
}

double SourceCenter(
    const double origin,
    const double spacing,
    const int cellIndex)
{
    return origin + (static_cast<double>(cellIndex) + 0.5) * spacing;
}

}  // namespace

TextureFillPartitionRasterMappingResult MapTextureFillPartitionToRaster(
    const TextureFillPartitionRasterMappingRequest& request)
{
    TextureFillPartitionRasterMappingResult result;
    result.grid = request.rasterGrid;
    if (request.partition == nullptr || request.transfer == nullptr)
    {
        BlockMapping(
            result,
            TextureFillPartitionErrorCode::RasterMappingInputInvalid,
            "raster mapping requires partition and texture-transfer inputs");
        return result;
    }
    if (!IsValidRasterGrid(request.rasterGrid))
    {
        BlockMapping(
            result,
            TextureFillPartitionErrorCode::RasterMappingGridInvalid,
            "raster grid dimensions, origins, pitches, and layer thickness must be finite and valid");
        return result;
    }

    const GlobalTextureFillPartitionResult& partition = *request.partition;
    const TextureFillPartitionTextureTransferResult& transfer = *request.transfer;
    const std::optional<std::size_t> sourceVoxelCount = CheckedVoxelCount(
        partition.grid.width,
        partition.grid.height,
        partition.grid.depth);
    if (!sourceVoxelCount.has_value()
        || !ValidatePartition(partition, *sourceVoxelCount, result)
        || !ValidateTransfer(partition, transfer, *sourceVoxelCount, result))
    {
        return result;
    }

    const std::optional<std::size_t> rasterVoxelCount = CheckedVoxelCount(
        request.rasterGrid.width,
        request.rasterGrid.height,
        request.rasterGrid.depth);
    if (!rasterVoxelCount.has_value())
    {
        BlockMapping(
            result,
            TextureFillPartitionErrorCode::RasterMappingGridInvalid,
            "raster voxel count overflows addressable memory");
        return result;
    }

    const auto mappingStart = Clock::now();
    const std::size_t rasterLayerArea = static_cast<std::size_t>(request.rasterGrid.width)
        * static_cast<std::size_t>(request.rasterGrid.height);
    const std::array<std::uint8_t, 3> emptyRgb{255U, 255U, 255U};
    std::vector<std::uint8_t> visitedSourceVoxels(*sourceVoxelCount, 0U);
    result.layers.reserve(static_cast<std::size_t>(request.rasterGrid.depth));
    result.stats.rasterVoxelCount = static_cast<std::uint64_t>(*rasterVoxelCount);
    result.allTexture = partition.widthMetrics.allTexture;

    for (int rasterZ{0}; rasterZ < request.rasterGrid.depth; ++rasterZ)
    {
        TextureFillPartitionRasterLayer layer;
        layer.layerIndex = rasterZ;
        layer.zMm = request.rasterGrid.originZMm
            + (static_cast<double>(rasterZ) + 0.5) * request.rasterGrid.layerThicknessMm;
        layer.modelMask.assign(rasterLayerArea, 0U);
        layer.textureSurfaceMask.assign(rasterLayerArea, 0U);
        layer.modelFillMask.assign(rasterLayerArea, 0U);
        layer.textureRgb.assign(rasterLayerArea, emptyRgb);

        const std::optional<int> sourceZ = ContainingCell(
            layer.zMm,
            partition.grid.originZMm,
            partition.grid.spacingZMm,
            partition.grid.depth);
        for (int rasterY{0}; rasterY < request.rasterGrid.height; ++rasterY)
        {
            const double worldY = request.rasterGrid.originYMm
                + (static_cast<double>(rasterY) + 0.5) * request.rasterGrid.pixelPitchYMm;
            const std::optional<int> sourceY = ContainingCell(
                worldY,
                partition.grid.originYMm,
                partition.grid.spacingYMm,
                partition.grid.height);
            for (int rasterX{0}; rasterX < request.rasterGrid.width; ++rasterX)
            {
                const std::size_t rasterIndex = static_cast<std::size_t>(rasterY)
                    * static_cast<std::size_t>(request.rasterGrid.width)
                    + static_cast<std::size_t>(rasterX);
                const double worldX = request.rasterGrid.originXMm
                    + (static_cast<double>(rasterX) + 0.5) * request.rasterGrid.pixelPitchXMm;
                const std::optional<int> sourceX = ContainingCell(
                    worldX,
                    partition.grid.originXMm,
                    partition.grid.spacingXMm,
                    partition.grid.width);
                if (!sourceX.has_value() || !sourceY.has_value() || !sourceZ.has_value())
                {
                    ++result.stats.outsideSourceGridVoxels;
                    continue;
                }

                const std::size_t sourceIndex = FlattenIndex(
                    partition.grid.width,
                    partition.grid.height,
                    *sourceX,
                    *sourceY,
                    *sourceZ);
                if (visitedSourceVoxels.at(sourceIndex) == 0U)
                {
                    visitedSourceVoxels.at(sourceIndex) = 1U;
                    ++result.stats.uniqueSourceVoxelCount;
                }
                ++result.stats.mappedSourceGridVoxels;

                const std::uint8_t model = partition.modelMask.values.at(sourceIndex);
                const std::uint8_t texture = partition.textureSurfaceMask.values.at(sourceIndex);
                const std::uint8_t fill = partition.modelFillMask.values.at(sourceIndex);
                layer.modelMask.at(rasterIndex) = model;
                layer.textureSurfaceMask.at(rasterIndex) = texture;
                layer.modelFillMask.at(rasterIndex) = fill;
                if (texture != 0U)
                {
                    layer.textureRgb.at(rasterIndex) = transfer.voxelRgb.at(sourceIndex);
                    ++result.stats.textureRgbRasterVoxels;
                }

                result.stats.modelRasterVoxels += model;
                result.stats.textureSurfaceRasterVoxels += texture;
                result.stats.modelFillRasterVoxels += fill;
                if (texture != 0U && fill != 0U)
                {
                    ++result.stats.overlapRasterVoxels;
                }
                if (model != 0U && texture == 0U && fill == 0U)
                {
                    ++result.stats.unassignedModelRasterVoxels;
                }

                const double deltaX = worldX - SourceCenter(
                    partition.grid.originXMm,
                    partition.grid.spacingXMm,
                    *sourceX);
                const double deltaY = worldY - SourceCenter(
                    partition.grid.originYMm,
                    partition.grid.spacingYMm,
                    *sourceY);
                const double deltaZ = layer.zMm - SourceCenter(
                    partition.grid.originZMm,
                    partition.grid.spacingZMm,
                    *sourceZ);
                result.stats.maxCenterQuantizationErrorMm = std::max(
                    result.stats.maxCenterQuantizationErrorMm,
                    std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ));
            }
        }
        result.layers.push_back(std::move(layer));
    }

    result.stats.reusedSourceVoxelCount = result.stats.mappedSourceGridVoxels
        - result.stats.uniqueSourceVoxelCount;
    const std::uint64_t sourceModelVoxels = static_cast<std::uint64_t>(std::count(
        partition.modelMask.values.begin(),
        partition.modelMask.values.end(),
        static_cast<std::uint8_t>(1U)));
    result.stats.sourceModelCoverage = static_cast<double>(sourceModelVoxels)
        / static_cast<double>(*sourceVoxelCount);
    result.stats.rasterModelCoverage = static_cast<double>(result.stats.modelRasterVoxels)
        / static_cast<double>(*rasterVoxelCount);
    result.stats.modelCoverageDelta = result.stats.rasterModelCoverage
        - result.stats.sourceModelCoverage;
    result.stats.partitionPass = result.stats.overlapRasterVoxels == 0U
        && result.stats.unassignedModelRasterVoxels == 0U
        && result.stats.modelRasterVoxels
            == result.stats.textureSurfaceRasterVoxels + result.stats.modelFillRasterVoxels;
    result.stats.mappingMs = std::chrono::duration<double, std::milli>(
        Clock::now() - mappingStart).count();

    if (!result.stats.partitionPass)
    {
        BlockMapping(
            result,
            TextureFillPartitionErrorCode::RasterMappingInvariantFailed,
            "raster mapping violated the exact model texture/fill complement");
        return result;
    }
    result.available = true;
    result.status = "diagnostic";
    return result;
}

}  // namespace slicer_core
