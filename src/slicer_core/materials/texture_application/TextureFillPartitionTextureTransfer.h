#pragma once

#include "slicer_core/diagnostics/ValidationIssue.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"
#include "slicer_core/texture_image.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Stable color source for one Stage 12E texture-surface voxel.
 */
enum class TextureFillColorSource
{
    NotColored,
    Texture,
    MaterialDiffuse,
    Fallback,
};

/**
 * @brief Convert a texture-transfer color source to stable report text.
 * @param source Color source.
 * @return Stable source name.
 */
std::string TextureFillColorSourceName(TextureFillColorSource source);

/**
 * @brief Backend-neutral Stage 12E texture-transfer request.
 */
struct TextureFillPartitionTextureTransferRequest
{
    const AdaptedTriangleMesh* adaptedMesh{nullptr};
    const GlobalTextureFillPartitionResult* partition{nullptr};
    TextureSampleOptions textureSample;
    std::array<std::uint8_t, 3> fallbackRgb{0, 0, 0};
    std::string missingTexturePolicy{"warn_and_fallback"};
};

/**
 * @brief Diagnostic texture-transfer measurements without production admission.
 */
struct TextureFillPartitionTextureTransferStats
{
    std::uint64_t textureSurfaceVoxels{0U};
    std::uint64_t sampledTextureCount{0U};
    std::uint64_t materialDiffuseCount{0U};
    std::uint64_t fallbackCount{0U};
    std::uint64_t missingUvCount{0U};
    std::uint64_t missingTextureCount{0U};
    std::uint64_t textureSampleFailureCount{0U};
    std::uint64_t uvOutOfRangeCount{0U};
    std::uint64_t outsideColoredCount{0U};
    std::uint64_t reusedReferenceCount{0U};
    std::uint64_t medialAxisTieCount{0U};
    std::uint64_t loadedTextureCount{0U};
    std::uint64_t textureCacheHits{0U};
    std::uint64_t textureCacheMisses{0U};
    std::uint64_t textureCacheBytes{0U};
    std::uint64_t nearestQueryCount{0U};
    double maxTransferDistanceMm{0.0};
    double transferMs{0.0};
};

/**
 * @brief Backend-neutral RGB attributes for the exact texture-surface mask.
 */
struct TextureFillPartitionTextureTransferResult
{
    bool available{false};
    std::string status{"blocked"};
    std::string productionAcceptance{"not_evaluated"};
    std::vector<std::array<std::uint8_t, 3>> voxelRgb;
    std::vector<TextureFillColorSource> colorSources;
    TextureFillPartitionTextureTransferStats stats;
    std::vector<ValidationIssue> issues;
};

/**
 * @brief Transfer OBJ/3MF surface attributes using stored closest-surface references.
 * @param request Adapted source attributes and validated Stage 12E partition.
 * @return Diagnostic RGB attributes; no nearest query or production output is performed.
 */
TextureFillPartitionTextureTransferResult TransferTextureFillPartition(
    const TextureFillPartitionTextureTransferRequest& request);

}  // namespace slicer_core
