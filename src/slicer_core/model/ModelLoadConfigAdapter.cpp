#include "slicer_core/model.h"

#include "slicer_core/config.h"

namespace slicer_core {

ModelLoadConfig MakeModelLoadConfig(const SliceConfig& config)
{
    ModelLoadConfig modelConfig;
    modelConfig.input = config.input;
    modelConfig.output_package_dir = config.output.package_dir;
    modelConfig.transform = config.transform;
    modelConfig.auto_orient = config.auto_orient;
    return modelConfig;
}

ModelReport load_model_report(
    const SliceConfig& config,
    const std::filesystem::path& configDir)
{
    return load_model_report(MakeModelLoadConfig(config), configDir);
}

}  // namespace slicer_core
