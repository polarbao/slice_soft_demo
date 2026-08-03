#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.h"
#include "slicer_core/raster/TextureFillPartitionRasterMapper.h"

#include <array>
#include <cstdint>
#include <string>

namespace slicer_core
{

struct BoundingBox;
struct TriangleMeshData;

/**
 * @brief Input contract for one diagnostic Stage 12E-08C Release measurement.
 */
struct TextureFillPartitionReleaseBenchmarkRequest
{
    const TriangleMeshData* mesh{nullptr};
    const AdaptedTriangleMesh* adaptedMesh{nullptr};
    std::string caseName{"unnamed"};
    std::string configPath;
    std::string modelPath;
    std::string buildType{"unknown"};
    double voxelMm{0.10};
    double widthMm{0.20};
    bool forceAllTexture{false};
    int paddingVoxels{1};
    double configLoadMs{0.0};
    double modelLoadMs{0.0};
    double meshAdaptMs{0.0};
    std::uint64_t sourceTriangles{0U};
    std::uint64_t acceptedTriangles{0U};
    std::uint64_t degenerateTriangles{0U};
    std::uint64_t boundaryEdges{0U};
    std::uint64_t nonManifoldEdges{0U};
    TextureSampleOptions textureSample;
    std::array<std::uint8_t, 3> fallbackRgb{0, 0, 0};
    std::string missingTexturePolicy{"warn_and_fallback"};
};

/**
 * @brief Diagnostic Release evidence without production admission or package output.
 */
struct TextureFillPartitionReleaseBenchmarkResult
{
    bool evidenceCollected{false};
    bool productionAdmitted{false};
    GlobalTextureFillPartitionResult partition;
    TextureFillPartitionTextureTransferResult textureTransfer;
    TextureFillPartitionRasterMappingResult rasterMapping;
    TextureFillPartitionFullClosureAdapterResult fullClosure;
    Json report;
};

/**
 * @brief Build a cell-centered classification grid around one model bounding box.
 * @param bbox Model bounding box in millimeters.
 * @param voxelMm Isotropic classification voxel size in millimeters.
 * @param paddingVoxels Number of empty cells retained on every side.
 * @return Finite classification grid covering the complete bounding box.
 */
TextureFillPartitionGridSpec BuildTextureFillPartitionBenchmarkGrid(
    const BoundingBox& bbox,
    double voxelMm,
    int paddingVoxels);

/**
 * @brief Measure the default-OFF CPU partition candidate without writing production output.
 * @param request Mesh, grid resolution, width, and already-measured import timings.
 * @return Machine-readable Release evidence and the validated partition result.
 */
TextureFillPartitionReleaseBenchmarkResult RunTextureFillPartitionReleaseBenchmark(
    const TextureFillPartitionReleaseBenchmarkRequest& request);

}  // namespace slicer_core
