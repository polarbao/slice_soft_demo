#pragma once

#include "slicer_core/preflight/ModelPreflightAdmissionPolicy.h"
#include "slicer_core/preflight/ModelPreflightService.h"

#include <functional>
#include <string>

namespace slicer_core
{

/**
 * @brief Request for one mode-aware model preflight pipeline gate.
 */
struct ModelPreflightGateRequest
{
    ModelPreflightRequest preflight_request;
    ModelPreflightPipelineMode selected_mode{ModelPreflightPipelineMode::Legacy};
    ModelPreflightAdmissionContext admission_context;
};

/**
 * @brief Result of evaluating one selected pipeline mode.
 */
struct ModelPreflightGateResult
{
    ModelPreflightExecutionResult preflight;
    ModeAdmissionResult selected_admission;
    bool pipeline_allowed{false};
    bool action_invoked{false};
};

/**
 * @brief Pipeline action invoked only after the selected mode is admitted.
 */
using ModelPreflightPipelineAction =
    std::function<void(const ModelPreflightGateResult&)>;

/**
 * @brief Apply mode admission and conditionally invoke one pipeline action.
 * @param execution Completed shared preflight execution.
 * @param selectedMode Explicitly selected pipeline mode.
 * @param context Explicit mode capability context.
 * @param action Pipeline callback. It is never invoked for blocked results.
 * @return Gate result including selected admission and invocation state.
 */
ModelPreflightGateResult ExecuteModelPreflightPipelineGate(
    const ModelPreflightExecutionResult& execution,
    ModelPreflightPipelineMode selectedMode,
    const ModelPreflightAdmissionContext& context,
    const ModelPreflightPipelineAction& action);

/**
 * @brief Run shared preflight, evaluate the selected mode, and invoke one admitted action.
 * @param service Reusable two-stage preflight service and cache.
 * @param request Config, mode, capability, generation and cancellation request.
 * @param action Pipeline callback. It is never invoked for blocked results.
 * @return Gate result including immutable diagnostics and selected admission.
 */
ModelPreflightGateResult RunModelPreflightPipelineGate(
    ModelPreflightService& service,
    const ModelPreflightGateRequest& request,
    const ModelPreflightPipelineAction& action);

/**
 * @brief Build a deterministic human-readable failure message for CLI/facades.
 * @param result Blocked gate result.
 * @return Message containing selected mode and stable blocker codes.
 */
std::string FormatModelPreflightGateFailure(
    const ModelPreflightGateResult& result);

}  // namespace slicer_core
