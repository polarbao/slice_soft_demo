#include "slicer_core/materials/process_profile/ModelFillMaterialResolver.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

bool ExpectChannels(
    const slicer_core::ModelFillMaterialResolution& resolution,
    const std::array<std::uint8_t, 6>& expected,
    const std::string& message)
{
    return ExpectTrue(resolution.resolvedChannels == expected, message)
        && ExpectTrue(
            resolution.resolvedChannels.at(4U) == 255U,
            message + " must not occupy support");
}

bool BuiltinMaterialsResolveToExistingChannels()
{
    slicer_core::ModelFillMaterialResolveRequest request;
    request.requestedMaterial = "white";
    request.value = 0U;
    const slicer_core::ModelFillMaterialResolution white =
        slicer_core::ResolveModelFillMaterial(request);

    request.requestedMaterial = "varnish";
    request.value = 17U;
    const slicer_core::ModelFillMaterialResolution varnish =
        slicer_core::ResolveModelFillMaterial(request);

    request.requestedMaterial = "rgb";
    request.customRgb = {11U, 22U, 33U};
    const slicer_core::ModelFillMaterialResolution rgb =
        slicer_core::ResolveModelFillMaterial(request);

    return ExpectTrue(white.available, "white is available")
        && ExpectChannels(
            white,
            {255U, 255U, 255U, 0U, 255U, 255U},
            "white channel mapping")
        && ExpectTrue(varnish.available, "varnish is available")
        && ExpectChannels(
            varnish,
            {255U, 255U, 255U, 255U, 255U, 17U},
            "varnish channel mapping")
        && ExpectTrue(rgb.available, "rgb is available")
        && ExpectChannels(
            rgb,
            {11U, 22U, 33U, 255U, 255U, 255U},
            "rgb channel mapping");
}

bool ProfileDefaultIsExplicitAndDeterministic()
{
    slicer_core::ModelFillMaterialResolveRequest request;
    request.requestedMaterial = "profile_default";
    const slicer_core::ModelFillMaterialResolution disabled =
        slicer_core::ResolveModelFillMaterial(request);

    request.profile.enabled = true;
    request.profile.name = "fixture-profile";
    request.profile.rgb.enabled = true;
    request.profile.white.enabled = true;
    request.profile.white.value = 9U;
    request.profile.varnish.enabled = true;
    request.profile.varnish.value = 13U;
    const slicer_core::ModelFillMaterialResolution enabled =
        slicer_core::ResolveModelFillMaterial(request);

    return ExpectTrue(!disabled.available, "disabled profile is unavailable")
        && ExpectTrue(
            disabled.reasonCode == "E_12E_MODEL_FILL_PROFILE_UNAVAILABLE",
            "disabled profile reason is stable")
        && ExpectTrue(enabled.available, "enabled profile resolves")
        && ExpectTrue(
            enabled.resolvedMaterial == "white",
            "profile default uses frozen white-varnish-rgb priority")
        && ExpectTrue(
            enabled.profileId == "fixture-profile",
            "profile identity is retained")
        && ExpectChannels(
            enabled,
            {255U, 255U, 255U, 9U, 255U, 255U},
            "profile default channel mapping");
}

bool MaterialRolesRequireAnExplicitRegistration()
{
    slicer_core::ModelFillMaterialResolveRequest request;
    request.requestedMaterial = "material_role";
    request.requestedRole = "c";
    const slicer_core::ModelFillMaterialResolution unavailable =
        slicer_core::ResolveModelFillMaterial(request);

    slicer_core::ModelFillMaterialRoleRegistration registration;
    registration.roleId = "C";
    registration.resolvedMaterial = "rgb";
    registration.rgb = {31U, 47U, 63U};
    registration.profileId = "calibrated-c";
    request.roleRegistry.push_back(registration);
    const slicer_core::ModelFillMaterialResolution registered =
        slicer_core::ResolveModelFillMaterial(request);

    request.requestedRole = "white";
    request.value = 21U;
    const slicer_core::ModelFillMaterialResolution builtinRole =
        slicer_core::ResolveModelFillMaterial(request);

    return ExpectTrue(!unavailable.available, "unregistered C is unavailable")
        && ExpectTrue(
            unavailable.reasonCode == "E_12E_MODEL_FILL_ROLE_UNREGISTERED",
            "unregistered C has a stable reason")
        && ExpectTrue(registered.available, "registered C is available")
        && ExpectTrue(
            registered.profileId == "calibrated-c",
            "registered role keeps profile identity")
        && ExpectChannels(
            registered,
            {31U, 47U, 63U, 255U, 255U, 255U},
            "registered C uses calibrated RGBWSV mapping")
        && ExpectTrue(builtinRole.available, "built-in white role resolves")
        && ExpectChannels(
            builtinRole,
            {255U, 255U, 255U, 21U, 255U, 255U},
            "built-in white role mapping");
}

bool InvalidRequestsFailWithStableReasons()
{
    slicer_core::ModelFillMaterialResolveRequest request;
    request.requestedMaterial = "material_role";
    const slicer_core::ModelFillMaterialResolution missingRole =
        slicer_core::ResolveModelFillMaterial(request);

    slicer_core::ModelFillMaterialRoleRegistration invalid;
    invalid.roleId = "k";
    invalid.resolvedMaterial = "new-k-channel";
    request.requestedRole = "k";
    request.roleRegistry.push_back(invalid);
    const slicer_core::ModelFillMaterialResolution invalidMapping =
        slicer_core::ResolveModelFillMaterial(request);

    request.requestedMaterial = "unknown";
    const slicer_core::ModelFillMaterialResolution unsupported =
        slicer_core::ResolveModelFillMaterial(request);

    return ExpectTrue(
               missingRole.reasonCode == "E_12E_MODEL_FILL_ROLE_REQUIRED",
               "missing role reason is stable")
        && ExpectTrue(
               invalidMapping.reasonCode
                   == "E_12E_MODEL_FILL_ROLE_MAPPING_INVALID",
               "invalid mapping reason is stable")
        && ExpectTrue(
               unsupported.reasonCode
                   == "E_12E_MODEL_FILL_MATERIAL_UNSUPPORTED",
               "unsupported material reason is stable");
}

}  // namespace

int main()
{
    const bool passed = BuiltinMaterialsResolveToExistingChannels()
        && ProfileDefaultIsExplicitAndDeterministic()
        && MaterialRolesRequireAnExplicitRegistration()
        && InvalidRequestsFailWithStableReasons();
    if (!passed)
    {
        return 1;
    }
    std::cout << "Model Fill material resolver tests complete.\n";
    return 0;
}
