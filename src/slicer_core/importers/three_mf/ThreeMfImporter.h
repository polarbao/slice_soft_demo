#pragma once

#include "slicer_core/config.h"
#include "slicer_core/scene/SceneModel.h"

#include <filesystem>

namespace slicer_core
{

/**
 * @brief Request object for the legacy 3MF importer facade.
 */
struct ThreeMfImportRequest
{
    SliceConfig config;
    std::filesystem::path config_dir;
};

/**
 * @brief Load a 3MF scene through the current legacy model loader.
 * @param request Import request containing config and config directory.
 * @return Loaded scene model.
 */
SceneModel LoadThreeMfSceneLegacy(const ThreeMfImportRequest& request);

}  // namespace slicer_core
