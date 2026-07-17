#pragma once

#include "slicer_core/diagnostics/ValidationIssue.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Stable errors for the Stage 12E texture/fill partition contract.
 */
enum class TextureFillPartitionErrorCode
{
    SurfaceShellWidthInvalid,
    SurfaceShellStepUnsupported,
    SurfaceShellGeometryModeUnsupported,
    SurfaceShellMinimumPolicyUnsupported,
    SurfaceScopeUnsupported,
    FullTextureAtModelLimitRequired,
    TextureFillScopeMismatch,
    ModelFillRequired,
    PartitionBackendUnavailable,
};

/**
 * @brief Convert a Stage 12E error code to its stable machine-readable name.
 * @param code Error code.
 * @return Stable error name.
 */
std::string TextureFillPartitionErrorCodeName(TextureFillPartitionErrorCode code);

/**
 * @brief Exception carrying a stable Stage 12E error code.
 */
class TextureFillPartitionError : public std::runtime_error
{
public:
    /**
     * @brief Construct a Stage 12E contract error.
     * @param code Stable error code.
     * @param message Human-readable diagnostic message.
     */
    TextureFillPartitionError(TextureFillPartitionErrorCode code, const std::string& message);

    /**
     * @brief Return the stable Stage 12E error code.
     * @return Error code associated with this exception.
     */
    TextureFillPartitionErrorCode Code() const noexcept;

private:
    TextureFillPartitionErrorCode m_code;
};

/**
 * @brief Backend-neutral options for a future global 3D texture/fill partition.
 */
struct GlobalTextureFillPartitionOptions
{
    double requestedWidthMm{0.10};
    double widthStepMm{0.01};
    double baseMinimumWidthMm{0.10};
    std::string surfaceScope{"all_closed_surfaces"};
};

/**
 * @brief Backend-neutral report state used before partition evidence exists.
 */
struct TextureFillPartitionReportData
{
    bool enabled{false};
    std::string strategy{"global_surface_shell"};
    std::string availability{"unavailable"};
    std::string status{"blocked"};
    std::string productionAcceptance{"not_evaluated"};
    std::string backend{"none"};
    std::string backendRole{"unavailable"};
    GlobalTextureFillPartitionOptions options;
    std::vector<ValidationIssue> issues;
};

}  // namespace slicer_core
