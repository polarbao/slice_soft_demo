#include "slicer_core/importers/three_mf/ThreeMfImporter.h"

#include "slicer_core/model.h"

namespace slicer_core
{

SceneModel LoadThreeMfSceneLegacy(const ThreeMfImportRequest& request)
{
    ModelLoadConfig config = request.config;
    config.input.format = "3mf";
    return load_model_report(config, request.config_dir);
}

}  // namespace slicer_core
