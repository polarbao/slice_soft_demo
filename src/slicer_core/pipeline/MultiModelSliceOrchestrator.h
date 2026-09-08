#pragma once

#include "slicer_core/api/Cancellation.h"
#include "slicer_core/layout/SceneCollisionService.h"
#include "slicer_core/pipeline/SceneLayerComposer.h"

#include <cstdint>
#include <vector>

namespace slicer_core
{

/**
 * @brief Immutable request for composing admitted per-instance rasters.
 */
struct MultiModelLayerComposeRequest
{
    SceneCollisionResult admission;
    std::uint64_t currentscenerevision{0U};
    SlicePipelineMode effectivepipelinemode{
        SlicePipelineMode::Legacy};
    std::vector<SceneInstanceRaster> instances;
    double quantizationtolerance{1.0e-6};
    bool padtooriginx{false}; // Preserve raster phase, extend only the left empty canvas.

    /**
     * @brief MF-05：逐层出入口，原样透传给 SceneLayerComposeRequest。
     *
     * 两者同时设置即进入流式合成：`instances` 里的 layers 允许为空、由 provider
     * 现取，合成结果也不再累积 layers、每层经 sink 交出。语义见
     * `SceneRasterTypes.h` 中同名字段的注释。
     */
    std::function<const SceneInstanceRasterLayer*(const SceneInstanceRaster&, int)>
        layerprovider;
    std::function<void(int, RgbwsvProductionLayer&&, const RgbwsvProductionLayerStatistics&)>
        layersink;

    /** @brief Synchronous, non-owning cancellation source for composition. */
    const api::ICancelToken* canceltoken{nullptr};
};

/**
 * @brief Build one shared grid and compose 13B-04-admitted rasters.
 * @param request Scene admission evidence, revision, mode, and rasters.
 * @return Writer-ready in-memory scene layers or a fail-closed error.
 */
SceneLayerComposeResult ComposeAdmittedSceneRasters(
    const MultiModelLayerComposeRequest& request);

/**
 * @brief Compose an owned request and consume exact single-instance layer buffers.
 * @param request Scene admission evidence and owned rasters.
 * @return Writer-ready layers or a fail-closed error.
 */
SceneLayerComposeResult ComposeAdmittedSceneRasters(
    MultiModelLayerComposeRequest&& request);

/**
 * @brief Compose admitted rasters and preserve in-loop closure evidence.
 * @param request Scene admission evidence, revision, mode, and rasters.
 * @return Immutable validated composition, or a structured blocked result.
 */
ValidatedSceneLayerComposeResult ComposeAdmittedSceneRastersValidated(
    const MultiModelLayerComposeRequest& request);

/**
 * @brief Compose an owned request while retaining immutable validation evidence.
 * @param request Scene admission evidence and owned rasters.
 * @return Immutable validated composition or a structured blocked result.
 */
ValidatedSceneLayerComposeResult ComposeAdmittedSceneRastersValidated(
    MultiModelLayerComposeRequest&& request);

}  // namespace slicer_core
