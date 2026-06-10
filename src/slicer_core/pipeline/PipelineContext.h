#pragma once

#include "slicer_core/config.h"
#include "slicer_core/pipeline/PipelineStepResult.h"
#include "slicer_core/scene/SceneModel.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace slicer_core
{

/**
 * @brief Shared state passed between R1 pipeline wrapper steps.
 */
struct PipelineContext
{
    std::filesystem::path config_path;
    std::filesystem::path config_dir;
    std::optional<SliceConfig> config;
    std::optional<SceneModel> scene;
    std::vector<PipelineStepResult> steps;
};

}  // namespace slicer_core
