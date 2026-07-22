#include "slicer_core/pipeline/SlicePipeline.h"

#include "slicer_core/pipeline/ModelPreflightGate.h"

#include <optional>
#include <stdexcept>
#include <utility>

namespace slicer_core
{

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
