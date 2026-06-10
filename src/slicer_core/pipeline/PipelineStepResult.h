#pragma once

#include "slicer_core/diagnostics/Diagnostics.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Result for a single pipeline step boundary.
 */
struct PipelineStepResult
{
    std::string step_name;
    bool ok{true};
    Diagnostics diagnostics;
    double elapsed_ms{0.0};
};

/**
 * @brief Create a successful pipeline step result.
 * @param stepName Name of the pipeline step.
 * @return Successful step result with the supplied name.
 */
PipelineStepResult MakeSuccessfulStepResult(std::string stepName);

}  // namespace slicer_core
