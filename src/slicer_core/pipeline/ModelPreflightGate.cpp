#include "slicer_core/pipeline/ModelPreflightGate.h"

#include <sstream>

namespace slicer_core
{
namespace
{

ModeAdmissionResult SelectAdmission(
    const ModelPreflightResult& result,
    const ModelPreflightPipelineMode mode)
{
    return mode == ModelPreflightPipelineMode::Legacy
        ? result.legacyAdmission
        : result.globalAdmission;
}

bool IsFreshCompleteResult(const ModelPreflightExecutionResult& execution)
{
    return execution.fastComplete
        && execution.fullComplete
        && !execution.cancelled
        && !execution.stale
        && (execution.result.status == ModelPreflightStatus::Passed
            || execution.result.status == ModelPreflightStatus::Warning);
}

}  // namespace

ModelPreflightGateResult ExecuteModelPreflightPipelineGate(
    const ModelPreflightExecutionResult& execution,
    const ModelPreflightPipelineMode selectedMode,
    const ModelPreflightAdmissionContext& context,
    const ModelPreflightPipelineAction& action)
{
    ModelPreflightGateResult gate;
    gate.preflight = execution;
    gate.preflight.result = EvaluateModelPreflightAdmissions(
        execution.result,
        context);
    gate.selected_admission = SelectAdmission(
        gate.preflight.result,
        selectedMode);
    gate.pipeline_allowed = IsFreshCompleteResult(gate.preflight)
        && gate.selected_admission.status != ModelPreflightAdmissionStatus::Blocked;

    if (gate.pipeline_allowed && action)
    {
        gate.action_invoked = true;
        action(gate);
    }
    return gate;
}

ModelPreflightGateResult RunModelPreflightPipelineGate(
    ModelPreflightService& service,
    const ModelPreflightGateRequest& request,
    const ModelPreflightPipelineAction& action)
{
    const ModelPreflightExecutionResult execution = service.Run(
        request.preflight_request);
    return ExecuteModelPreflightPipelineGate(
        execution,
        request.selected_mode,
        request.admission_context,
        action);
}

std::string FormatModelPreflightGateFailure(
    const ModelPreflightGateResult& result)
{
    std::ostringstream stream;
    stream << "model preflight blocked "
           << ModelPreflightPipelineModeName(result.selected_admission.mode)
           << " pipeline";
    if (!result.selected_admission.blockerCodes.empty())
    {
        stream << ": ";
        for (std::size_t index{0U};
             index < result.selected_admission.blockerCodes.size();
             ++index)
        {
            if (index > 0U)
            {
                stream << ',';
            }
            stream << result.selected_admission.blockerCodes.at(index);
        }
    }
    for (const ModelPreflightIssue& issue : result.preflight.result.issues)
    {
        if (issue.context.is_object()
            && issue.context.contains("detail")
            && issue.context.at("detail").is_string())
        {
            stream << "; detail=" << issue.context.at("detail").as_string();
            break;
        }
    }
    return stream.str();
}

}  // namespace slicer_core
