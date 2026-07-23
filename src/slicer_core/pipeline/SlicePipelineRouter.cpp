#include "slicer_core/pipeline/SlicePipelineRouter.h"

namespace slicer_core
{

SlicePipelineRouteDecision ResolveSlicePipelineRoute(
    const SlicePipelineConfig& config,
    const SlicePipelineRouteContext& context)
{
    SlicePipelineRouteDecision decision;
    decision.requested_mode = config.mode;
    decision.effective_mode = config.mode;
    decision.fallback_applied = false;

    if (config.mode == SlicePipelineMode::Legacy)
    {
        decision.allowed = true;
        return decision;
    }

    if (context.global_topology_blocked)
    {
        decision.error_code = SlicePipelineErrorCode::GlobalTopologyBlocked;
        decision.detail = "global_surface_shell topology admission failed";
        return decision;
    }
    if (!context.global_preflight_admitted)
    {
        decision.error_code = SlicePipelineErrorCode::GlobalNotAdmitted;
        decision.detail = "global_surface_shell preflight admission failed";
        return decision;
    }
    if (!context.global_production_available)
    {
        decision.error_code = SlicePipelineErrorCode::GlobalNotAdmitted;
        decision.detail =
            "global_surface_shell production profile is unavailable until 12E-08D-04 admission";
        return decision;
    }

    decision.allowed = true;
    return decision;
}

void RequireSlicePipelineRoute(const SlicePipelineRouteDecision& decision)
{
    if (decision.allowed)
    {
        return;
    }
    const SlicePipelineErrorCode code =
        decision.error_code == SlicePipelineErrorCode::None
        ? SlicePipelineErrorCode::GlobalNotAdmitted
        : decision.error_code;
    throw SlicePipelineError(code, decision.detail);
}

}  // namespace slicer_core
