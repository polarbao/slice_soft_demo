#include "slicer_core/pipeline/SlicePipeline.h"

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
    return run_slicer(configPath, options);
}

}  // namespace slicer_core
