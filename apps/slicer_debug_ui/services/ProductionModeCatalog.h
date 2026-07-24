#pragma once

#include "slicer_core/config/SlicePipelineConfig.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

/**
 * @brief Capability state exposed by one production mode or Profile.
 */
enum class ProductionFeatureState
{
    ConfiguredByProfile,
    Enabled,
    Disabled,
};

/**
 * @brief Admitted support scope for one production Profile.
 */
enum class ProductionSupportScope
{
    None,
    ConfiguredByProfile,
    LowerAndInternalVoid,
};

/**
 * @brief Admitted varnish scope for one production Profile.
 */
enum class ProductionVarnishScope
{
    None,
    ConfiguredByProfile,
    SurfaceAndOuter,
};

/**
 * @brief Product-facing resource cost classification.
 */
enum class ProductionResourceCostLevel
{
    NotEvaluated,
    Normal,
    High,
};

/**
 * @brief Product-facing admission state for the current model and config.
 */
enum class ProductionAdmissionState
{
    Pending,
    Stale,
    Running,
    Blocked,
    Admitted,
};

/**
 * @brief Stable capability metadata for one end-to-end production mode.
 */
struct ProductionModeCapability
{
    slicer_core::SlicePipelineMode mode{
        slicer_core::SlicePipelineMode::Legacy};
    std::string stablevalue;
    std::string displaynamezh;
    bool defaultmode{false};
    bool explicitselectionrequired{false};
    bool profilepassthrough{false};
    ProductionResourceCostLevel resourcecost{
        ProductionResourceCostLevel::NotEvaluated};
};

/**
 * @brief Stable capability metadata for one admitted production Profile.
 */
struct ProductionProfileCapability
{
    std::string profileid;
    std::string displaynamezh;
    slicer_core::SlicePipelineMode mode{
        slicer_core::SlicePipelineMode::Legacy};
    ProductionFeatureState rgb{ProductionFeatureState::Disabled};
    ProductionFeatureState white{ProductionFeatureState::Disabled};
    ProductionFeatureState support{ProductionFeatureState::Disabled};
    ProductionFeatureState varnish{ProductionFeatureState::Disabled};
    ProductionSupportScope supportscope{ProductionSupportScope::None};
    ProductionVarnishScope varnishscope{ProductionVarnishScope::None};
};

/**
 * @brief Fail-closed UI state for one requested production slice.
 */
struct ProductionModeUiDto
{
    slicer_core::SlicePipelineMode requestedmode{
        slicer_core::SlicePipelineMode::Legacy};
    std::optional<slicer_core::SlicePipelineMode> effectivemode;
    std::string requestedprofileid;
    std::optional<std::string> effectiveprofileid;
    ProductionAdmissionState admissionstate{
        ProductionAdmissionState::Pending};
    bool productionoutputwritten{false};
    bool fallbackapplied{false};
    ProductionResourceCostLevel resourcecost{
        ProductionResourceCostLevel::NotEvaluated};
    std::optional<double> measuredtotalms;
    std::optional<std::uint64_t> measuredpeakworkingsetbytes;
    std::string sessionid;
    std::string configpath;
    std::string packagepath;
    std::string blockingcode;
    std::string blockingmessage;
};

/**
 * @brief Read-only catalog for product modes and admitted Global Profiles.
 */
class ProductionModeCatalog final
{
public:
    /**
     * @brief Return the stable capability-lock schema version.
     * @return Stable version stored in session audit data.
     */
    static std::string_view CapabilityLockVersion();

    /**
     * @brief Return all product modes in stable display order.
     * @return Read-only view containing Legacy followed by Global Surface Shell.
     */
    static std::span<const ProductionModeCapability> Modes();

    /**
     * @brief Return all admitted Global production Profiles.
     * @return Read-only view containing restricted and material-parity Profiles.
     */
    static std::span<const ProductionProfileCapability> Profiles();

    /**
     * @brief Return the default product mode.
     * @return Legacy mode capability.
     */
    static const ProductionModeCapability& DefaultMode();

    /**
     * @brief Find capability metadata for one product mode.
     * @param mode Stable core pipeline mode.
     * @return Capability pointer, or nullptr when the mode is not cataloged.
     */
    static const ProductionModeCapability* FindMode(
        slicer_core::SlicePipelineMode mode);

    /**
     * @brief Find one admitted Global production Profile.
     * @param profileId Stable material process Profile target.
     * @return Capability pointer, or nullptr for an unknown Profile.
     */
    static const ProductionProfileCapability* FindProfile(
        std::string_view profileId);
};
