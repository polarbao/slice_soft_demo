#include "slicer_core/config/SlicePipelineConfig.h"

namespace slicer_core
{

SlicePipelineError::SlicePipelineError(
    const SlicePipelineErrorCode code,
    const std::string& detail)
    : std::runtime_error(SlicePipelineErrorCodeName(code) + ": " + detail)
    , m_code(code)
{
}

SlicePipelineErrorCode SlicePipelineError::Code() const noexcept
{
    return m_code;
}

std::string SlicePipelineModeName(const SlicePipelineMode mode)
{
    switch (mode)
    {
    case SlicePipelineMode::Legacy:
        return "legacy";
    case SlicePipelineMode::GlobalSurfaceShell:
        return "global_surface_shell";
    }
    return "legacy";
}

SlicePipelineMode ParseSlicePipelineMode(const std::string& value)
{
    if (value == "legacy")
    {
        return SlicePipelineMode::Legacy;
    }
    if (value == "global_surface_shell")
    {
        return SlicePipelineMode::GlobalSurfaceShell;
    }
    throw SlicePipelineError(
        SlicePipelineErrorCode::ModeUnsupported,
        "slicePipeline.mode must be legacy or global_surface_shell");
}

std::string SlicePipelineErrorCodeName(const SlicePipelineErrorCode code)
{
    switch (code)
    {
    case SlicePipelineErrorCode::None:
        return "";
    case SlicePipelineErrorCode::ModeUnsupported:
        return "E_12E_PIPELINE_MODE_UNSUPPORTED";
    case SlicePipelineErrorCode::ConfigMismatch:
        return "E_12E_PIPELINE_MODE_CONFIG_MISMATCH";
    case SlicePipelineErrorCode::GlobalNotAdmitted:
        return "E_12E_PIPELINE_GLOBAL_NOT_ADMITTED";
    case SlicePipelineErrorCode::GlobalTopologyBlocked:
        return "E_12E_PIPELINE_GLOBAL_TOPOLOGY_BLOCKED";
    case SlicePipelineErrorCode::ProductionTiffRequired:
        return "E_12E_PIPELINE_PRODUCTION_TIFF_REQUIRED";
    case SlicePipelineErrorCode::SilentFallbackForbidden:
        return "E_12E_PIPELINE_SILENT_FALLBACK_FORBIDDEN";
    }
    return "E_12E_PIPELINE_MODE_UNSUPPORTED";
}

}  // namespace slicer_core
