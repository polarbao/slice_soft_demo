#include "ProductionModeCatalog.h"

#include <array>

namespace
{

const std::array<ProductionModeCapability, 2> kModes{{
    {
        slicer_core::SlicePipelineMode::Legacy,
        "legacy",
        "传统切片",
        true,
        false,
        true,
        ProductionResourceCostLevel::Normal,
    },
    {
        slicer_core::SlicePipelineMode::GlobalSurfaceShell,
        "global_surface_shell",
        "全局纹理壳层",
        false,
        true,
        false,
        ProductionResourceCostLevel::High,
    },
}};

const std::array<ProductionProfileCapability, 2> kProfiles{{
    {
        "global_surface_shell_restricted_candidate",
        "全局纹理壳层（受限材料）",
        slicer_core::SlicePipelineMode::GlobalSurfaceShell,
        ProductionFeatureState::Enabled,
        ProductionFeatureState::Enabled,
        ProductionFeatureState::Disabled,
        ProductionFeatureState::Disabled,
        ProductionSupportScope::None,
        ProductionVarnishScope::None,
    },
    {
        "global_surface_shell_material_parity_candidate",
        "全局纹理壳层（材料一致）",
        slicer_core::SlicePipelineMode::GlobalSurfaceShell,
        ProductionFeatureState::Enabled,
        ProductionFeatureState::Enabled,
        ProductionFeatureState::Enabled,
        ProductionFeatureState::Enabled,
        ProductionSupportScope::LowerAndInternalVoid,
        ProductionVarnishScope::SurfaceAndOuter,
    },
}};

}  // namespace

std::string_view ProductionModeCatalog::CapabilityLockVersion()
{
    return "slicesoft.ui.production_capability.12e_09b.1";
}

std::span<const ProductionModeCapability> ProductionModeCatalog::Modes()
{
    return kModes;
}

std::span<const ProductionProfileCapability> ProductionModeCatalog::Profiles()
{
    return kProfiles;
}

const ProductionModeCapability& ProductionModeCatalog::DefaultMode()
{
    return kModes.front();
}

const ProductionModeCapability* ProductionModeCatalog::FindMode(
    const slicer_core::SlicePipelineMode mode)
{
    for (const ProductionModeCapability& capability : kModes)
    {
        if (capability.mode == mode)
        {
            return &capability;
        }
    }
    return nullptr;
}

const ProductionProfileCapability* ProductionModeCatalog::FindProfile(
    const std::string_view profileId)
{
    for (const ProductionProfileCapability& capability : kProfiles)
    {
        if (capability.profileid == profileId)
        {
            return &capability;
        }
    }
    return nullptr;
}
