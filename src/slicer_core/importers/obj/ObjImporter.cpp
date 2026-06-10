#include "slicer_core/importers/obj/ObjImporter.h"

#include "slicer_core/model.h"

namespace slicer_core
{

SceneModel LoadObjSceneLegacy(const ObjImportRequest& request)
{
    SliceConfig config = request.config;
    config.input.format = "obj";
    return load_model_report(config, request.config_dir);
}

}  // namespace slicer_core
