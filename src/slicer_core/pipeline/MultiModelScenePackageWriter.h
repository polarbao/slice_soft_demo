#pragma once

#include "slicer_core/layout/SceneCollisionService.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/pipeline/SceneRasterTypes.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <filesystem>
#include <vector>

namespace slicer_core
{

/**
 * @brief Publish one admitted composed scene through the shared RGBWSV writer.
 * @param request Package path, storage, preview, DPI, and pipeline metadata.
 * @param composition Complete 13B-05 writer-ready scene layers.
 * @param scene Canonical model and instance scene document.
 * @param admission Passing 13B-04 collision and build-volume evidence.
 * @param instanceRasters Authoritative per-instance production bytes.
 * @param profileConfigPath Effective Profile identity source.
 * @return Atomic package publication summary.
 * @throws std::invalid_argument when scene evidence is invalid or stale.
 */
RgbwsvProductionPackageWriteResult
WriteMultiModelSceneProductionPackage(
    RgbwsvProductionPackageWriteRequest request,
    SceneLayerComposeResult composition,
    const MultiModelScene& scene,
    const SceneCollisionResult& admission,
    const std::vector<SceneInstanceRaster>& instanceRasters = {},
    const std::filesystem::path& profileConfigPath = {});

}  // namespace slicer_core
