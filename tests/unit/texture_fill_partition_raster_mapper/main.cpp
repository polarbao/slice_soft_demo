#include "slicer_core/raster/TextureFillPartitionRasterMapper.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool HasIssueCode(
    const std::vector<slicer_core::ValidationIssue>& issues,
    const std::string& code)
{
    for (const slicer_core::ValidationIssue& issue : issues)
    {
        if (issue.code == code)
        {
            return true;
        }
    }
    return false;
}

std::size_t VoxelIndex(const int width, const int height, const int x, const int y, const int z)
{
    return (static_cast<std::size_t>(z) * static_cast<std::size_t>(height)
        + static_cast<std::size_t>(y)) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

slicer_core::GlobalTextureFillPartitionResult MakePartition(const bool allTexture = false)
{
    slicer_core::GlobalTextureFillPartitionResult partition;
    partition.available = true;
    partition.partitionPass = true;
    partition.status = "diagnostic";
    partition.backend = "generated_raster_fixture";
    partition.backendRole = "diagnostic";
    partition.grid.width = 2;
    partition.grid.height = 2;
    partition.grid.depth = 2;
    partition.grid.originXMm = 0.0;
    partition.grid.originYMm = 0.0;
    partition.grid.originZMm = 0.0;
    partition.grid.spacingXMm = 1.0;
    partition.grid.spacingYMm = 1.0;
    partition.grid.spacingZMm = 1.0;
    partition.modelMask.grid = partition.grid;
    partition.textureSurfaceMask.grid = partition.grid;
    partition.modelFillMask.grid = partition.grid;
    partition.modelMask.values.assign(8U, 1U);
    partition.textureSurfaceMask.values.assign(8U, 0U);
    partition.modelFillMask.values.assign(8U, 0U);
    for (int z{0}; z < 2; ++z)
    {
        for (int y{0}; y < 2; ++y)
        {
            for (int x{0}; x < 2; ++x)
            {
                const std::size_t index = VoxelIndex(2, 2, x, y, z);
                const bool texture = allTexture || x == 0;
                partition.textureSurfaceMask.values.at(index) = texture ? 1U : 0U;
                partition.modelFillMask.values.at(index) = texture ? 0U : 1U;
            }
        }
    }
    partition.widthMetrics.allTexture = allTexture;
    partition.stats.modelVoxels = 8U;
    partition.stats.textureSurfaceVoxels = allTexture ? 8U : 4U;
    partition.stats.modelFillVoxels = allTexture ? 0U : 4U;
    return partition;
}

slicer_core::TextureFillPartitionTextureTransferResult MakeTransfer(
    const slicer_core::GlobalTextureFillPartitionResult& partition)
{
    slicer_core::TextureFillPartitionTextureTransferResult transfer;
    transfer.available = true;
    transfer.status = "diagnostic";
    transfer.voxelRgb.assign(8U, {255U, 255U, 255U});
    transfer.colorSources.assign(8U, slicer_core::TextureFillColorSource::NotColored);
    for (std::size_t index{0U}; index < 8U; ++index)
    {
        if (partition.textureSurfaceMask.values.at(index) == 0U)
        {
            continue;
        }
        transfer.voxelRgb.at(index) = {
            static_cast<std::uint8_t>(10U + index),
            static_cast<std::uint8_t>(20U + index),
            static_cast<std::uint8_t>(30U + index)};
        transfer.colorSources.at(index) = slicer_core::TextureFillColorSource::Texture;
        ++transfer.stats.textureSurfaceVoxels;
    }
    return transfer;
}

slicer_core::TextureFillPartitionRasterGridSpec MakeRasterGrid(
    const int width = 2,
    const int height = 2,
    const int depth = 2,
    const double pitch = 1.0)
{
    slicer_core::TextureFillPartitionRasterGridSpec grid;
    grid.width = width;
    grid.height = height;
    grid.depth = depth;
    grid.originXMm = 0.0;
    grid.originYMm = 0.0;
    grid.originZMm = 0.0;
    grid.pixelPitchXMm = pitch;
    grid.pixelPitchYMm = pitch;
    grid.layerThicknessMm = pitch;
    return grid;
}

slicer_core::TextureFillPartitionRasterMappingRequest MakeRequest(
    const slicer_core::GlobalTextureFillPartitionResult& partition,
    const slicer_core::TextureFillPartitionTextureTransferResult& transfer,
    const slicer_core::TextureFillPartitionRasterGridSpec& grid)
{
    slicer_core::TextureFillPartitionRasterMappingRequest request;
    request.partition = &partition;
    request.transfer = &transfer;
    request.rasterGrid = grid;
    return request;
}

bool IdentityGridPreservesExactPartitionAndRgb()
{
    const auto partition = MakePartition();
    const auto transfer = MakeTransfer(partition);
    const auto result = slicer_core::MapTextureFillPartitionToRaster(
        MakeRequest(partition, transfer, MakeRasterGrid()));
    return ExpectTrue(result.available, "identity mapping is available")
        && ExpectTrue(result.status == "diagnostic", "identity mapping is diagnostic")
        && ExpectTrue(result.mappingMethod == "world_space_cell_containment", "method is stable")
        && ExpectTrue(result.layers.size() == 2U, "identity mapping keeps two true-Z layers")
        && ExpectTrue(result.stats.modelRasterVoxels == 8U, "all model voxels are preserved")
        && ExpectTrue(result.stats.textureSurfaceRasterVoxels == 4U, "texture count is preserved")
        && ExpectTrue(result.stats.modelFillRasterVoxels == 4U, "fill count is preserved")
        && ExpectTrue(result.stats.overlapRasterVoxels == 0U, "identity has no overlap")
        && ExpectTrue(result.stats.unassignedModelRasterVoxels == 0U, "identity has no model gap")
        && ExpectTrue(result.layers.at(1).layerIndex == 1, "true layer index is retained")
        && ExpectTrue(std::abs(result.layers.at(1).zMm - 1.5) < 1.0e-12, "true Z is retained")
        && ExpectTrue(
            result.layers.at(0).textureRgb.at(0) == std::array<std::uint8_t, 3>{10U, 20U, 30U},
            "texture RGB is copied from the owning classification voxel")
        && ExpectTrue(!result.productionOutputWritten, "mapping writes no production output")
        && ExpectTrue(result.productionAcceptance == "not_evaluated", "mapping is not admitted");
}

bool FinerRasterUsesDeterministicCellOwnership()
{
    const auto partition = MakePartition();
    const auto transfer = MakeTransfer(partition);
    const auto result = slicer_core::MapTextureFillPartitionToRaster(
        MakeRequest(partition, transfer, MakeRasterGrid(4, 4, 4, 0.5)));
    return ExpectTrue(result.available, "finer mapping is available")
        && ExpectTrue(result.stats.rasterVoxelCount == 64U, "finer raster has 64 voxels")
        && ExpectTrue(result.stats.uniqueSourceVoxelCount == 8U, "all source cells are sampled")
        && ExpectTrue(result.stats.reusedSourceVoxelCount == 56U, "source reuse is reported")
        && ExpectTrue(result.stats.textureSurfaceRasterVoxels == 32U, "texture expands exactly")
        && ExpectTrue(result.stats.modelFillRasterVoxels == 32U, "fill expands exactly")
        && ExpectTrue(result.stats.partitionPass, "finer raster preserves complement")
        && ExpectTrue(result.stats.maxCenterQuantizationErrorMm > 0.0, "quantization is measured");
}

bool CoarserRasterIsDeterministic()
{
    const auto partition = MakePartition();
    const auto transfer = MakeTransfer(partition);
    const auto request = MakeRequest(partition, transfer, MakeRasterGrid(1, 1, 1, 2.0));
    const auto first = slicer_core::MapTextureFillPartitionToRaster(request);
    const auto second = slicer_core::MapTextureFillPartitionToRaster(request);
    return ExpectTrue(first.available && second.available, "coarse mappings are available")
        && ExpectTrue(first.layers.at(0).modelMask == second.layers.at(0).modelMask, "model is stable")
        && ExpectTrue(first.layers.at(0).textureSurfaceMask == second.layers.at(0).textureSurfaceMask, "texture is stable")
        && ExpectTrue(first.layers.at(0).modelFillMask == second.layers.at(0).modelFillMask, "fill is stable")
        && ExpectTrue(first.layers.at(0).textureRgb == second.layers.at(0).textureRgb, "RGB ownership is stable")
        && ExpectTrue(first.stats.modelFillRasterVoxels == 1U, "cell-boundary tie selects positive cell");
}

bool OutsideClassificationExtentStaysEmpty()
{
    const auto partition = MakePartition();
    const auto transfer = MakeTransfer(partition);
    auto grid = MakeRasterGrid(4, 2, 2, 1.0);
    grid.originXMm = -1.0;
    const auto result = slicer_core::MapTextureFillPartitionToRaster(
        MakeRequest(partition, transfer, grid));
    return ExpectTrue(result.available, "partly outside mapping remains inspectable")
        && ExpectTrue(result.stats.outsideSourceGridVoxels == 8U, "outside voxels are counted")
        && ExpectTrue(result.stats.modelRasterVoxels == 8U, "inside extent remains mapped")
        && ExpectTrue(result.layers.at(0).modelMask.at(0) == 0U, "outside source cell stays empty")
        && ExpectTrue(
            result.layers.at(0).textureRgb.at(0)
                == std::array<std::uint8_t, 3>{255U, 255U, 255U},
            "outside source cell keeps empty RGB evidence");
}

bool AllTextureMapsWithZeroFill()
{
    const auto partition = MakePartition(true);
    const auto transfer = MakeTransfer(partition);
    const auto result = slicer_core::MapTextureFillPartitionToRaster(
        MakeRequest(partition, transfer, MakeRasterGrid()));
    return ExpectTrue(result.available, "all-texture mapping is available")
        && ExpectTrue(result.allTexture, "all-texture state is retained")
        && ExpectTrue(result.stats.textureSurfaceRasterVoxels == 8U, "all model is texture")
        && ExpectTrue(result.stats.modelFillRasterVoxels == 0U, "all-texture fill stays zero")
        && ExpectTrue(result.stats.partitionPass, "all-texture complement passes");
}

bool TextureRgbNeverLeaksIntoFill()
{
    const auto partition = MakePartition();
    const auto transfer = MakeTransfer(partition);
    const auto result = slicer_core::MapTextureFillPartitionToRaster(
        MakeRequest(partition, transfer, MakeRasterGrid()));
    const auto& layer = result.layers.at(0);
    return ExpectTrue(
        layer.textureRgb.at(1) == std::array<std::uint8_t, 3>{255U, 255U, 255U},
        "fill voxel keeps empty RGB evidence")
        && ExpectTrue(result.stats.textureRgbRasterVoxels == 4U, "only texture voxels receive RGB");
}

bool InvalidRasterGridBlocks()
{
    const auto partition = MakePartition();
    const auto transfer = MakeTransfer(partition);
    auto grid = MakeRasterGrid();
    grid.pixelPitchXMm = 0.0;
    const auto result = slicer_core::MapTextureFillPartitionToRaster(
        MakeRequest(partition, transfer, grid));
    return ExpectTrue(!result.available, "invalid raster grid blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_RASTER_MAPPING_GRID_INVALID"),
            "invalid raster grid uses stable code");
}

bool InvalidPartitionBlocks()
{
    auto partition = MakePartition();
    partition.textureSurfaceMask.values.at(1) = 1U;
    const auto transfer = MakeTransfer(partition);
    const auto result = slicer_core::MapTextureFillPartitionToRaster(
        MakeRequest(partition, transfer, MakeRasterGrid()));
    return ExpectTrue(!result.available, "overlapping source partition blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_RASTER_MAPPING_PARTITION_INVALID"),
            "invalid source partition uses stable code");
}

bool InvalidTextureTransferBlocks()
{
    const auto partition = MakePartition();
    auto transfer = MakeTransfer(partition);
    transfer.voxelRgb.pop_back();
    const auto result = slicer_core::MapTextureFillPartitionToRaster(
        MakeRequest(partition, transfer, MakeRasterGrid()));
    return ExpectTrue(!result.available, "invalid texture transfer blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_RASTER_MAPPING_TRANSFER_INVALID"),
            "invalid transfer uses stable code");
}

bool InconsistentAllTexturePartitionBlocks()
{
    auto partition = MakePartition();
    partition.widthMetrics.allTexture = true;
    const auto transfer = MakeTransfer(partition);
    const auto result = slicer_core::MapTextureFillPartitionToRaster(
        MakeRequest(partition, transfer, MakeRasterGrid()));
    return ExpectTrue(!result.available, "all-texture partition with fill blocks")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_RASTER_MAPPING_PARTITION_INVALID"),
            "inconsistent all-texture state uses stable code");
}

bool NullInputsBlockWithoutWriting()
{
    slicer_core::TextureFillPartitionRasterMappingRequest request;
    request.rasterGrid = MakeRasterGrid();
    const auto result = slicer_core::MapTextureFillPartitionToRaster(request);
    return ExpectTrue(!result.available, "null inputs block")
        && ExpectTrue(!result.productionOutputWritten, "blocked mapping writes nothing")
        && ExpectTrue(
            HasIssueCode(result.issues, "E_12E_RASTER_MAPPING_INPUT_INVALID"),
            "null inputs use stable code");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"identity_grid_preserves_exact_partition_and_rgb", IdentityGridPreservesExactPartitionAndRgb},
        {"finer_raster_uses_deterministic_cell_ownership", FinerRasterUsesDeterministicCellOwnership},
        {"coarser_raster_is_deterministic", CoarserRasterIsDeterministic},
        {"outside_classification_extent_stays_empty", OutsideClassificationExtentStaysEmpty},
        {"all_texture_maps_with_zero_fill", AllTextureMapsWithZeroFill},
        {"texture_rgb_never_leaks_into_fill", TextureRgbNeverLeaksIntoFill},
        {"invalid_raster_grid_blocks", InvalidRasterGridBlocks},
        {"invalid_partition_blocks", InvalidPartitionBlocks},
        {"invalid_texture_transfer_blocks", InvalidTextureTransferBlocks},
        {"inconsistent_all_texture_partition_blocks", InconsistentAllTexturePartitionBlocks},
        {"null_inputs_block_without_writing", NullInputsBlockWithoutWriting},
    };
    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        if (!test.second())
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }
    std::cout << "Texture/fill partition raster mapper unit tests complete.\n";
    return 0;
}
