#pragma once

#include "slicer_core/config/SlicePipelineConfig.h"
#include "slicer_core/diagnostics/TextureFillPartitionFullClosureAdapter.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackage.h"

#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Inputs required to promote exact Global Surface Shell evidence to writer-ready layers.
 */
struct GlobalSurfaceShellProductionLayerAdapterRequest
{
    const TextureFillPartitionRasterMappingResult* rasterMapping{nullptr};
    const TextureFillPartitionFullClosureAdapterResult* fullClosure{nullptr};
    const std::vector<TextureFillPartitionFullClosureLayerEvidence>*
        closureEvidence{nullptr};
};

/**
 * @brief Writer-ready RGBWSV bytes paired with their immutable material semantics.
 */
struct GlobalSurfaceShellProductionLayer
{
    RgbwsvProductionLayer output;
    MaterialClosureSemanticLayerInput semantic;
};

/**
 * @brief Result of adapting admitted Global Surface Shell evidence without writing files.
 */
struct GlobalSurfaceShellProductionLayerAdapterResult
{
    bool available{false};
    bool fullClosurePass{false};
    bool productionOutputWritten{false};
    std::string status{"blocked"};
    std::string productionAcceptance{"not_evaluated"};
    int widthPx{0};
    int heightPx{0};
    int layerCount{0};
    RgbwsvProtocol protocol;
    std::vector<GlobalSurfaceShellProductionLayer> layers;
    SlicePipelineErrorCode errorCode{SlicePipelineErrorCode::None};
    std::string detail;
};

/**
 * @brief Adapt exact Global Surface Shell raster and closure evidence to production layer DTOs.
 * @param request Validated raster mapping, passing full closure, and matching final RGBWSV evidence.
 * @return Writer-ready in-memory layers. No TIFF, manifest, preview, report, or package is written.
 */
GlobalSurfaceShellProductionLayerAdapterResult
AdaptGlobalSurfaceShellProductionLayers(
    const GlobalSurfaceShellProductionLayerAdapterRequest& request);

}  // namespace slicer_core
