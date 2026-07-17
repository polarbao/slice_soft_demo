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
    case TextureFillPartitionErrorCode::CpuMeshMissing:
        return "E_12E_CPU_MESH_MISSING";
    case TextureFillPartitionErrorCode::CpuGridInvalid:
        return "E_12E_CPU_GRID_INVALID";
    case TextureFillPartitionErrorCode::CpuTopologyBlocked:
        return "E_12E_CPU_TOPOLOGY_BLOCKED";
    case TextureFillPartitionErrorCode::CpuOccupancyFailed:
        return "E_12E_CPU_OCCUPANCY_FAILED";
    case TextureFillPartitionErrorCode::CpuNearestSurfaceFailed:
        return "E_12E_CPU_NEAREST_SURFACE_FAILED";
    case TextureFillPartitionErrorCode::OpenVdbBackendUnavailable:
        return "E_12E_OPENVDB_BACKEND_UNAVAILABLE";
    case TextureFillPartitionErrorCode::OpenVdbTopologyBlocked:
        return "E_12E_OPENVDB_TOPOLOGY_BLOCKED";
    case TextureFillPartitionErrorCode::OpenVdbLevelSetFailed:
        return "E_12E_OPENVDB_LEVEL_SET_FAILED";
    case TextureFillPartitionErrorCode::OpenVdbGridSampleFailed:
        return "E_12E_OPENVDB_GRID_SAMPLE_FAILED";
    case TextureFillPartitionErrorCode::OpenVdbDistanceIncomplete:
        return "E_12E_OPENVDB_DISTANCE_INCOMPLETE";
    case TextureFillPartitionErrorCode::BackendConformanceFailed:
        return "E_12E_BACKEND_CONFORMANCE_FAILED";
    case TextureFillPartitionErrorCode::WidthSweepEmpty:
        return "E_12E_WIDTH_SWEEP_EMPTY";
    case TextureFillPartitionErrorCode::WidthSweepSampleFailed:
        return "E_12E_WIDTH_SWEEP_SAMPLE_FAILED";
    case TextureFillPartitionErrorCode::WidthSweepModelChanged:
        return "E_12E_WIDTH_SWEEP_MODEL_CHANGED";
    case TextureFillPartitionErrorCode::WidthSweepTextureNonMonotonic:
        return "E_12E_WIDTH_SWEEP_TEXTURE_NON_MONOTONIC";
    case TextureFillPartitionErrorCode::WidthSweepFillNonMonotonic:
        return "E_12E_WIDTH_SWEEP_FILL_NON_MONOTONIC";
    case TextureFillPartitionErrorCode::WidthSweepEndpointInvalid:
        return "E_12E_WIDTH_SWEEP_ENDPOINT_INVALID";
    case TextureFillPartitionErrorCode::SurfaceShellWidthBelowEffectiveMinimum:
        return "E_12E_SURFACE_SHELL_WIDTH_BELOW_EFFECTIVE_MINIMUM";
    case TextureFillPartitionErrorCode::AllTextureThresholdUnavailable:
        return "E_12E_ALL_TEXTURE_THRESHOLD_UNAVAILABLE";
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
