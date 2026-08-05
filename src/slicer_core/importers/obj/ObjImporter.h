#pragma once

#include "slicer_core/model/ModelLoadConfig.h"
#include "slicer_core/scene/SceneModel.h"

#include <filesystem>

namespace slicer_core
{

/**
 * @brief Request object for the legacy OBJ importer facade.
 */
struct ObjImportRequest
{
    ModelLoadConfig config;
    std::filesystem::path config_dir;
};

/**
 * @brief Load an OBJ scene through the current legacy model loader.
 * @param request Import request containing config and config directory.
 * @return Loaded scene model.
 */
SceneModel LoadObjSceneLegacy(const ObjImportRequest& request);

}  // namespace slicer_core
