#pragma once

#include "slicer_core/layout/SceneCollisionService.h"
#include "slicer_core/output/rgbwsv/RgbwsvSceneExtension.h"
#include "slicer_core/pipeline/SceneLayerComposer.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <filesystem>
#include <string_view>

namespace slicer_core
{

/**
 * @brief Build a typed scene summary and report from admitted composed evidence.
 * @param scene Canonical scene sources, instances, layout, and transforms.
 * @param admission Passing build-volume and collision evidence.
 * @param composition Complete writer-ready scene layers and statistics.
 * @param requestedPipelineMode Requested stable pipeline mode.
 * @param packageDir Destination package identity recorded in the report.
 * @return Validated scene summary and full report.
 * @throws std::invalid_argument when identities or evidence do not match.
 */
MultiModelSceneReportDocument BuildMultiModelSceneReport(
    const MultiModelScene& scene,
    const SceneCollisionResult& admission,
    const SceneLayerComposeResult& composition,
    std::string_view requestedPipelineMode,
    const std::filesystem::path& packageDir);

/**
 * @brief Build a scene report from immutable composer validation evidence.
 * @param scene Canonical scene sources, instances, layout, and transforms.
 * @param admission Passing build-volume and collision evidence.
 * @param composition Composer-owned closed layer evidence.
 * @param requestedPipelineMode Requested stable pipeline mode.
 * @param packageDir Destination package identity recorded in the report.
 * @return Validated scene summary and full report without a duplicate byte scan.
 */
MultiModelSceneReportDocument BuildValidatedMultiModelSceneReport(
    const MultiModelScene& scene,
    const SceneCollisionResult& admission,
    const ValidatedSceneLayerComposeResult& composition,
    std::string_view requestedPipelineMode,
    const std::filesystem::path& packageDir);

}  // namespace slicer_core
