#include "slicer_core/materials/volume/MaterialVolumeError.h"

namespace slicer_core
{

std::string MaterialVolumeErrorCodeName(const MaterialVolumeErrorCode code)
{
    switch (code)
    {
        case MaterialVolumeErrorCode::UnsupportedPipeline:
            return "E_MATVOL_UNSUPPORTED_PIPELINE";
        case MaterialVolumeErrorCode::MaterialMissing:
            return "E_MATVOL_MATERIAL_MISSING";
        case MaterialVolumeErrorCode::OpenSurfaceRequiresPolicy:
            return "E_MATVOL_OPEN_SURFACE_REQUIRES_POLICY";
        case MaterialVolumeErrorCode::TopologyInvalid:
            return "E_MATVOL_TOPOLOGY_INVALID";
        case MaterialVolumeErrorCode::IntersectionUnpaired:
            return "E_MATVOL_INTERSECTION_UNPAIRED";
        case MaterialVolumeErrorCode::OverlapUnresolved:
            return "E_MATVOL_OVERLAP_UNRESOLVED";
        case MaterialVolumeErrorCode::ModelPixelUnowned:
            return "E_MATVOL_MODEL_PIXEL_UNOWNED";
        case MaterialVolumeErrorCode::ReplayMismatch:
            return "E_MATVOL_REPLAY_MISMATCH";
        case MaterialVolumeErrorCode::BudgetExceeded:
            return "E_MATVOL_BUDGET_EXCEEDED";
    }
    return "E_MATVOL_UNKNOWN";
}

MaterialVolumeError::MaterialVolumeError(
    const MaterialVolumeErrorCode code,
    const std::string& message)
    : std::runtime_error(MaterialVolumeErrorCodeName(code) + ": " + message),
      code_(code)
{
}

MaterialVolumeErrorCode MaterialVolumeError::Code() const noexcept
{
    return code_;
}

}  // namespace slicer_core
