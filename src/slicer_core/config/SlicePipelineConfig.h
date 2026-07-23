#pragma once

#include <stdexcept>
#include <string>

namespace slicer_core
{

/**
 * @brief End-to-end production slice pipeline selected by configuration.
 */
enum class SlicePipelineMode
{
    Legacy,
    GlobalSurfaceShell,
};

/**
 * @brief Stable configuration for selecting one end-to-end slice pipeline.
 */
struct SlicePipelineConfig
{
    SlicePipelineMode mode{SlicePipelineMode::Legacy};
    bool explicitly_configured{false};
};

/**
 * @brief Stable error codes for dual-pipeline configuration and routing.
 */
enum class SlicePipelineErrorCode
{
    None,
    ModeUnsupported,
    ConfigMismatch,
    GlobalNotAdmitted,
    GlobalTopologyBlocked,
    ProductionTiffRequired,
    SilentFallbackForbidden,
    GlobalAdapterInputInvalid,
    GlobalAdapterClosureRequired,
    GlobalAdapterLayerMismatch,
    GlobalAdapterProtocolMismatch,
};

/**
 * @brief Exception carrying one stable dual-pipeline error code.
 */
class SlicePipelineError final : public std::runtime_error
{
public:
    /**
     * @brief Construct a pipeline error with a stable code and detail.
     * @param code Stable pipeline error code.
     * @param detail Human-readable diagnostic detail.
     */
    SlicePipelineError(SlicePipelineErrorCode code, const std::string& detail);

    /**
     * @brief Return the stable error code.
     * @return Error code associated with this exception.
     */
    [[nodiscard]] SlicePipelineErrorCode Code() const noexcept;

private:
    SlicePipelineErrorCode m_code;
};

/**
 * @brief Convert a pipeline mode to its stable configuration value.
 * @param mode Pipeline mode.
 * @return Stable snake_case mode name.
 */
std::string SlicePipelineModeName(SlicePipelineMode mode);

/**
 * @brief Parse a stable pipeline mode value.
 * @param value Configuration value.
 * @return Parsed pipeline mode.
 * @throws SlicePipelineError when the value is unsupported.
 */
SlicePipelineMode ParseSlicePipelineMode(const std::string& value);

/**
 * @brief Convert a pipeline error code to its stable machine-readable value.
 * @param code Pipeline error code.
 * @return Stable E_12E_PIPELINE error name.
 */
std::string SlicePipelineErrorCodeName(SlicePipelineErrorCode code);

}  // namespace slicer_core
