#include "ProductionModeCatalog.h"

#include <iostream>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool TestLegacyIsTheDefaultMode()
{
    const ProductionModeCapability& capability =
        ProductionModeCatalog::DefaultMode();
    return ExpectTrue(
               capability.mode == slicer_core::SlicePipelineMode::Legacy,
               "legacy is default")
        && ExpectTrue(capability.defaultmode, "legacy default flag")
        && ExpectTrue(
            capability.stablevalue == "legacy",
            "legacy stable value")
        && ExpectTrue(
            !capability.explicitselectionrequired,
            "legacy does not require explicit selection")
        && ExpectTrue(
            capability.profilepassthrough,
            "legacy preserves the selected existing Profile")
        && ExpectTrue(
            capability.displaynamezh == "传统切片",
            "legacy Chinese display name")
        && ExpectTrue(
            capability.resourcecost == ProductionResourceCostLevel::Normal,
            "legacy resource cost");
}

bool TestGlobalModeIsExplicitAndHighCost()
{
    const ProductionModeCapability* capability =
        ProductionModeCatalog::FindMode(
            slicer_core::SlicePipelineMode::GlobalSurfaceShell);
    return ExpectTrue(capability != nullptr, "global mode exists")
        && ExpectTrue(!capability->defaultmode, "global is not default")
        && ExpectTrue(
            capability->stablevalue == "global_surface_shell",
            "global stable value")
        && ExpectTrue(
            capability->explicitselectionrequired,
            "global requires explicit selection")
        && ExpectTrue(
            !capability->profilepassthrough,
            "global requires an admitted catalog Profile")
        && ExpectTrue(
            capability->displaynamezh == "全局纹理壳层",
            "global Chinese display name")
        && ExpectTrue(
            capability->resourcecost == ProductionResourceCostLevel::High,
            "global high resource cost");
}

bool TestRestrictedProfileCapability()
{
    const ProductionProfileCapability* capability =
        ProductionModeCatalog::FindProfile(
            "global_surface_shell_restricted_candidate");
    return ExpectTrue(capability != nullptr, "restricted Profile exists")
        && ExpectTrue(
            capability->displaynamezh == "全局纹理壳层（受限材料）",
            "restricted Profile Chinese display name")
        && ExpectTrue(
            capability->mode
                == slicer_core::SlicePipelineMode::GlobalSurfaceShell,
            "restricted Profile mode")
        && ExpectTrue(
            capability->rgb == ProductionFeatureState::Enabled,
            "restricted RGB enabled")
        && ExpectTrue(
            capability->white == ProductionFeatureState::Enabled,
            "restricted white enabled")
        && ExpectTrue(
            capability->support == ProductionFeatureState::Disabled,
            "restricted support disabled")
        && ExpectTrue(
            capability->varnish == ProductionFeatureState::Disabled,
            "restricted varnish disabled")
        && ExpectTrue(
            capability->supportscope == ProductionSupportScope::None,
            "restricted support scope")
        && ExpectTrue(
            capability->varnishscope == ProductionVarnishScope::None,
            "restricted varnish scope");
}

bool TestMaterialParityProfileCapability()
{
    const ProductionProfileCapability* capability =
        ProductionModeCatalog::FindProfile(
            "global_surface_shell_material_parity_candidate");
    return ExpectTrue(capability != nullptr, "material parity Profile exists")
        && ExpectTrue(
            capability->displaynamezh == "全局纹理壳层（材料一致）",
            "material parity Profile Chinese display name")
        && ExpectTrue(
            capability->rgb == ProductionFeatureState::Enabled,
            "material parity RGB enabled")
        && ExpectTrue(
            capability->white == ProductionFeatureState::Enabled,
            "material parity white enabled")
        && ExpectTrue(
            capability->support == ProductionFeatureState::Enabled,
            "material parity support enabled")
        && ExpectTrue(
            capability->varnish == ProductionFeatureState::Enabled,
            "material parity varnish enabled")
        && ExpectTrue(
            capability->supportscope
                == ProductionSupportScope::LowerAndInternalVoid,
            "material parity support scope")
        && ExpectTrue(
            capability->varnishscope
                == ProductionVarnishScope::SurfaceAndOuter,
            "material parity varnish scope");
}

bool TestUnknownProfileFailsClosed()
{
    return ExpectTrue(
               ProductionModeCatalog::FindProfile("unknown_profile") == nullptr,
               "unknown Profile is not mapped to legacy")
        && ExpectTrue(
            ProductionModeCatalog::FindMode(
                static_cast<slicer_core::SlicePipelineMode>(255))
                == nullptr,
            "unknown mode is not mapped to legacy");
}

bool TestCapabilityLockVersionIsStable()
{
    return ExpectTrue(
        ProductionModeCatalog::CapabilityLockVersion()
            == "slicesoft.ui.production_capability.12e_09b.1",
        "capability lock version");
}

bool TestUiDtoDefaultsFailClosed()
{
    const ProductionModeUiDto dto;
    return ExpectTrue(
               dto.requestedmode == slicer_core::SlicePipelineMode::Legacy,
               "DTO requests legacy by default")
        && ExpectTrue(!dto.effectivemode.has_value(), "effective mode unevaluated")
        && ExpectTrue(
            dto.admissionstate == ProductionAdmissionState::Pending,
            "admission pending")
        && ExpectTrue(
            !dto.productionoutputwritten,
            "production output not written")
        && ExpectTrue(!dto.fallbackapplied, "fallback disabled")
        && ExpectTrue(
            dto.resourcecost == ProductionResourceCostLevel::NotEvaluated,
            "resource cost unevaluated")
        && ExpectTrue(!dto.measuredtotalms.has_value(), "timing unevaluated")
        && ExpectTrue(
            !dto.measuredpeakworkingsetbytes.has_value(),
            "memory unevaluated");
}

}  // namespace

int main()
{
    const bool passed = TestLegacyIsTheDefaultMode()
        && TestGlobalModeIsExplicitAndHighCost()
        && TestRestrictedProfileCapability()
        && TestMaterialParityProfileCapability()
        && TestUnknownProfileFailsClosed()
        && TestCapabilityLockVersionIsStable()
        && TestUiDtoDefaultsFailClosed();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS production_mode_catalog_unit_tests\n";
    return 0;
}
