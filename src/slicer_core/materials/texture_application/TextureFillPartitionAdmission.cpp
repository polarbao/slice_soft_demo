#include "slicer_core/materials/texture_application/TextureFillPartitionAdmission.h"

#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"

namespace slicer_core
{

bool IsGlobalTextureFillPartitionRequested(const SliceConfig& config)
{
    return config.texture.enabled && config.texture.apply_mode == "global_surface_shell";
}

void EnsureGlobalTextureFillPartitionBackendAvailable(const SliceConfig& config)
{
    if (!IsGlobalTextureFillPartitionRequested(config))
    {
        return;
    }

    throw TextureFillPartitionError(
        TextureFillPartitionErrorCode::PartitionBackendUnavailable,
        "global_surface_shell is a contract-only mode in 12E-01; no global 3D partition backend is available");
}

}  // namespace slicer_core
