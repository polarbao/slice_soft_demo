#include "slicer_core/pipeline/PipelineStepResult.h"

#include <utility>

namespace slicer_core
{

PipelineStepResult MakeSuccessfulStepResult(std::string stepName)
{
    PipelineStepResult result;
    result.step_name = std::move(stepName);
    return result;
}

}  // namespace slicer_core
