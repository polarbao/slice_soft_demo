#include "slicer_core/pipeline/SlicePipeline.h"

#include "slicer_core/pipeline/ModelPreflightGate.h"
#include "slicer_core/pipeline/SlicePipelineRouter.h"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>

namespace slicer_core
{
namespace
{

bool ContainsBlocker(
    const ModelPreflightGateResult& gate,
    const ModelPreflightErrorCode code)
{
    const std::string stableCode = ModelPreflightErrorCodeName(code);
    return std::find(
               gate.selected_admission.blockerCodes.begin(),
               gate.selected_admission.blockerCodes.end(),
               stableCode)
        != gate.selected_admission.blockerCodes.end();
}

}  // namespace

std::vector<std::string> DefaultSlicePipelineSteps()
{
    return {
        "LoadConfig",
        "ValidateConfig",
        "LoadInputScene",
        "NormalizeScene",
        "ResolveMaterials",
        "PrepareTextureSources",
        "ApplyTextureApplicationPolicy",
        "PrepareVarnishGeometryPolicy",
        "SliceGeometry",
        "GenerateSupport",
        "ComposeMaterialChannels",
        "WriteRGBWSVPackage",
        "WriteReports",
        "ValidatePackage",
    };
}

SliceRunResult RunSlicePipeline(
    const std::filesystem::path& configPath,
    const SliceRunOptions& options)
{
    const SliceConfig config = load_slice_config(configPath);
    if (config.slice_pipeline.mode == SlicePipelineMode::Legacy)
    {
        return RunSlicePipelineLegacy(configPath, options);
    }

    ModelPreflightService service;
    ModelPreflightGateRequest request;
    request.preflight_request.configPath = configPath;
    request.selected_mode = ModelPreflightPipelineMode::GlobalSurfaceShell;
    request.admission_context.global_backend_available = true;

    const ModelPreflightGateResult gate = RunModelPreflightPipelineGate(
        service,
        request,
        {});

    SlicePipelineRouteContext routeContext;
    routeContext.global_preflight_admitted = gate.pipeline_allowed;
    routeContext.global_topology_blocked = ContainsBlocker(
        gate,
        ModelPreflightErrorCode::GlobalTopologyBlocked);

    // The shared writer is verified by 08D-03, but an explicit admitted Global
    // production profile remains gated until 08D-04.
    routeContext.global_production_available = false;

    SlicePipelineRouteDecision decision = ResolveSlicePipelineRoute(
        config.slice_pipeline,
        routeContext);
    if (!gate.pipeline_allowed)
    {
        decision.detail += "; " + FormatModelPreflightGateFailure(gate);
    }
    RequireSlicePipelineRoute(decision);

    throw SlicePipelineError(
        SlicePipelineErrorCode::ProductionTiffRequired,
        "global_surface_shell route completed without a production TIFF result");
}

SliceRunResult RunSlicePipelineLegacy(const std::filesystem::path& configPath, const SliceRunOptions& options)
{
    ModelPreflightService service;
    ModelPreflightGateRequest request;
    request.preflight_request.configPath = configPath;
    request.selected_mode = ModelPreflightPipelineMode::Legacy;

    std::optional<SliceRunResult> result;
    const ModelPreflightGateResult gate = RunModelPreflightPipelineGate(
        service,
        request,
        [&](const ModelPreflightGateResult&)
        {
            result = run_slicer(configPath, options);
        });
    if (!result.has_value())
    {
        throw std::runtime_error(FormatModelPreflightGateFailure(gate));
    }
    return std::move(result.value());
}

}  // namespace slicer_core
