#pragma once

#include "slicer_core/config.h"
#include "slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h"
#include "slicer_core/raster/TextureFillPartitionRasterMapper.h"

#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Material evidence generated around an exact Global model partition.
 */
struct GlobalSurfaceShellMaterialEvidenceResult
{
    bool available{false};
    std::string status{"blocked"};
    std::string detail;
    std::uint64_t supportPixels{0U};
    std::uint64_t internalVoidSupportPixels{0U};
    std::uint64_t surfaceVarnishPixels{0U};
    std::uint64_t outerVarnishPixels{0U};
    std::vector<TextureFillPartitionFullClosureLayerEvidence> layers;
};

/**
 * @brief Compose RGB/W model material plus optional lower support and varnish evidence.
 * @param mapping Exact Global partition mapped to the production raster.
 * @param config Validated slice configuration.
 * @return Full-material evidence aligned to every true-Z raster layer.
 */
GlobalSurfaceShellMaterialEvidenceResult
ComposeGlobalSurfaceShellMaterialEvidence(
    const TextureFillPartitionRasterMappingResult& mapping,
    const SliceConfig& config);

}  // namespace slicer_core
