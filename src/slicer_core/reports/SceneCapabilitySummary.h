#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/pipeline/SceneRasterTypes.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace slicer_core
{

/** @brief Capability-v1.2 metadata persisted beside one scene package. */
struct SceneCapabilitySummaryDocument
{
    Json perinstance;
    Json profileecho;

    /**
     * @brief Validate the package-summary extension shape.
     * @return True when per-instance and Profile evidence are complete.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Build authoritative package-summary evidence from instance rasters.
 * @param scene Committed scene containing instance transforms.
 * @param rasters Writer-ready per-instance production rasters.
 * @param profileDocument Effective Profile JSON carrying version and hash.
 * @return Summary evidence, or no value for legacy Profiles without identity.
 * @throws std::invalid_argument When scene/raster evidence is inconsistent.
 */
std::optional<SceneCapabilitySummaryDocument> BuildSceneCapabilitySummary(
    const MultiModelScene& scene,
    const std::vector<SceneInstanceRaster>& rasters,
    const Json& profileDocument);

/**
 * @brief Build package-summary evidence from one effective Profile file.
 * @param scene Committed scene containing instance transforms.
 * @param rasters Writer-ready per-instance production rasters.
 * @param profileConfigPath Effective Profile JSON path.
 * @return Summary evidence, or no value for a legacy path/Profile.
 * @throws std::invalid_argument When the file or evidence is inconsistent.
 */
std::optional<SceneCapabilitySummaryDocument> BuildSceneCapabilitySummary(
    const MultiModelScene& scene,
    const std::vector<SceneInstanceRaster>& rasters,
    const std::filesystem::path& profileConfigPath);

}  // namespace slicer_core
