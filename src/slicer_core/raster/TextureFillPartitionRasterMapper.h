#pragma once

#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Final-grid geometry used by the diagnostic Stage 12E raster mapper.
 */
struct TextureFillPartitionRasterGridSpec
{
    int width{0};
    int height{0};
    int depth{0};
    double originXMm{0.0};
    double originYMm{0.0};
    double originZMm{0.0};
    double pixelPitchXMm{0.0};
    double pixelPitchYMm{0.0};
    double layerThicknessMm{0.0};
};

/**
 * @brief One true-Z raster layer mapped from the validated classification grid.
 */
struct TextureFillPartitionRasterLayer
{
    int layerIndex{0};
    double zMm{0.0};
    std::vector<std::uint8_t> modelMask;
    std::vector<std::uint8_t> textureSurfaceMask;
    std::vector<std::uint8_t> modelFillMask;
    std::vector<std::array<std::uint8_t, 3>> textureRgb;
};

/**
 * @brief Deterministic mapping evidence without production package admission.
 */
struct TextureFillPartitionRasterMappingStats
{
    std::uint64_t rasterVoxelCount{0U};
    std::uint64_t mappedSourceGridVoxels{0U};
    std::uint64_t outsideSourceGridVoxels{0U};
    std::uint64_t uniqueSourceVoxelCount{0U};
    std::uint64_t reusedSourceVoxelCount{0U};
    std::uint64_t modelRasterVoxels{0U};
    std::uint64_t textureSurfaceRasterVoxels{0U};
    std::uint64_t modelFillRasterVoxels{0U};
    std::uint64_t textureRgbRasterVoxels{0U};
    std::uint64_t overlapRasterVoxels{0U};
    std::uint64_t unassignedModelRasterVoxels{0U};
    double sourceModelCoverage{0.0};
    double rasterModelCoverage{0.0};
    double modelCoverageDelta{0.0};
    double maxCenterQuantizationErrorMm{0.0};
    double mappingMs{0.0};
    bool partitionPass{false};
};

/**
 * @brief Inputs for deterministic classification-to-raster mapping.
 */
struct TextureFillPartitionRasterMappingRequest
{
    const GlobalTextureFillPartitionResult* partition{nullptr};
    const TextureFillPartitionTextureTransferResult* transfer{nullptr};
    TextureFillPartitionRasterGridSpec rasterGrid;
};

/**
 * @brief Diagnostic classification-to-raster result that never writes production output.
 */
struct TextureFillPartitionRasterMappingResult
{
    bool available{false};
    bool allTexture{false};
    bool productionOutputWritten{false};
    std::string status{"blocked"};
    std::string productionAcceptance{"not_evaluated"};
    std::string mappingMethod{"world_space_cell_containment"};
    TextureFillPartitionRasterGridSpec grid;
    std::vector<TextureFillPartitionRasterLayer> layers;
    TextureFillPartitionRasterMappingStats stats;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Map validated Stage 12E ownership and texture RGB to true print-grid centers.
 * @param request Validated partition, texture transfer, and destination raster geometry.
 * @return Diagnostic raster layers and quantization evidence; no TIFF is written.
 */
TextureFillPartitionRasterMappingResult MapTextureFillPartitionToRaster(
    const TextureFillPartitionRasterMappingRequest& request);

}  // namespace slicer_core
