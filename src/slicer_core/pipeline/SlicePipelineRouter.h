#pragma once

#include "slicer_core/config/SlicePipelineConfig.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Runtime facts used to resolve one configured slice pipeline.
 */
struct SlicePipelineRouteContext
{
    bool global_preflight_admitted{false};
    bool global_topology_blocked{false};
    bool global_production_available{false};
};

/**
 * @brief Auditable route decision for one end-to-end slice request.
 */
struct SlicePipelineRouteDecision
{
    SlicePipelineMode requested_mode{SlicePipelineMode::Legacy};
    SlicePipelineMode effective_mode{SlicePipelineMode::Legacy};
    bool allowed{false};
    bool fallback_applied{false};
    SlicePipelineErrorCode error_code{SlicePipelineErrorCode::None};
    std::string detail;
};

/**
 * @brief Resolve a configured pipeline without applying a silent fallback.
 * @param config Pipeline configuration.
 * @param context Current admission and production capabilities.
 * @return Route decision containing requested/effective mode and blocker.
 */
SlicePipelineRouteDecision ResolveSlicePipelineRoute(
    const SlicePipelineConfig& config,
    const SlicePipelineRouteContext& context);

/**
 * @brief Throw the stable route error when a decision is blocked.
 * @param decision Route decision to enforce.
 * @throws SlicePipelineError when decision.allowed is false.
 */
void RequireSlicePipelineRoute(const SlicePipelineRouteDecision& decision);

}  // namespace slicer_core
