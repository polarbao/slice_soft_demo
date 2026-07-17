#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"

namespace slicer_core
{

std::string TextureFillPartitionErrorCodeName(const TextureFillPartitionErrorCode code)
{
    switch (code)
    {
    case TextureFillPartitionErrorCode::SurfaceShellWidthInvalid:
        return "E_12E_SURFACE_SHELL_WIDTH_INVALID";
    case TextureFillPartitionErrorCode::SurfaceShellStepUnsupported:
        return "E_12E_SURFACE_SHELL_STEP_UNSUPPORTED";
    case TextureFillPartitionErrorCode::SurfaceShellGeometryModeUnsupported:
        return "E_12E_SURFACE_SHELL_GEOMETRY_MODE_UNSUPPORTED";
    case TextureFillPartitionErrorCode::SurfaceShellMinimumPolicyUnsupported:
        return "E_12E_SURFACE_SHELL_MINIMUM_POLICY_UNSUPPORTED";
    case TextureFillPartitionErrorCode::SurfaceScopeUnsupported:
        return "E_12E_SURFACE_SCOPE_UNSUPPORTED";
    case TextureFillPartitionErrorCode::FullTextureAtModelLimitRequired:
        return "E_12E_FULL_TEXTURE_AT_MODEL_LIMIT_REQUIRED";
    case TextureFillPartitionErrorCode::TextureFillScopeMismatch:
        return "E_12E_TEXTURE_FILL_SCOPE_MISMATCH";
    case TextureFillPartitionErrorCode::ModelFillRequired:
        return "E_12E_MODEL_FILL_REQUIRED";
    case TextureFillPartitionErrorCode::PartitionBackendUnavailable:
        return "E_12E_PARTITION_BACKEND_UNAVAILABLE";
    case TextureFillPartitionErrorCode::PartitionBackendFailed:
        return "E_12E_PARTITION_BACKEND_FAILED";
    case TextureFillPartitionErrorCode::PartitionGridInvalid:
        return "E_12E_PARTITION_GRID_INVALID";
    case TextureFillPartitionErrorCode::PartitionMaskSizeMismatch:
        return "E_12E_PARTITION_MASK_SIZE_MISMATCH";
    case TextureFillPartitionErrorCode::PartitionMaskNonBinary:
        return "E_12E_PARTITION_MASK_NON_BINARY";
    case TextureFillPartitionErrorCode::TextureOutsideModel:
        return "E_12E_TEXTURE_OUTSIDE_MODEL";
    case TextureFillPartitionErrorCode::ModelFillOutsideModel:
        return "E_12E_MODEL_FILL_OUTSIDE_MODEL";
    case TextureFillPartitionErrorCode::TextureFillOverlap:
        return "E_12E_TEXTURE_FILL_OVERLAP";
    case TextureFillPartitionErrorCode::ModelVoxelUnassigned:
        return "E_12E_MODEL_VOXEL_UNASSIGNED";
    }
    return "E_12E_UNKNOWN";
}

TextureFillPartitionError::TextureFillPartitionError(
    const TextureFillPartitionErrorCode code,
    const std::string& message)
    : std::runtime_error(TextureFillPartitionErrorCodeName(code) + ": " + message),
      m_code(code)
{
}

TextureFillPartitionErrorCode TextureFillPartitionError::Code() const noexcept
{
    return m_code;
}

}  // namespace slicer_core
