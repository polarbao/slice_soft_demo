#include "slicer_core/materials/process_profile/ModelFillMaterialResolver.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

std::string LowerAscii(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

bool IsResolvedMaterial(const std::string& material)
{
    return material == "white"
        || material == "varnish"
        || material == "rgb";
}

void ApplyResolvedMaterial(
    ModelFillMaterialResolution& resolution,
    const std::string& material,
    const std::array<std::uint8_t, 3>& rgb,
    const std::uint8_t value)
{
    resolution.available = true;
    resolution.resolvedMaterial = material;
    resolution.resolvedRgb = rgb;
    resolution.resolvedValue = value;
    resolution.resolvedChannels.fill(255U);
    if (material == "white")
    {
        resolution.resolvedChannels.at(3U) = value;
    }
    else if (material == "varnish")
    {
        resolution.resolvedChannels.at(5U) = value;
    }
    else
    {
        resolution.resolvedChannels.at(0U) = rgb.at(0U);
        resolution.resolvedChannels.at(1U) = rgb.at(1U);
        resolution.resolvedChannels.at(2U) = rgb.at(2U);
    }
}

ModelFillMaterialResolution Unavailable(
    ModelFillMaterialResolution resolution,
    const std::string& reasonCode)
{
    resolution.available = false;
    resolution.resolvedMaterial = "unavailable";
    resolution.reasonCode = reasonCode;
    return resolution;
}

}  // namespace

ModelFillMaterialResolution ResolveModelFillMaterial(
    const ModelFillMaterialResolveRequest& request)
{
    ModelFillMaterialResolution resolution;
    resolution.requestedMaterial = LowerAscii(request.requestedMaterial);
    resolution.requestedRole = LowerAscii(request.requestedRole);

    if (IsResolvedMaterial(resolution.requestedMaterial))
    {
        ApplyResolvedMaterial(
            resolution,
            resolution.requestedMaterial,
            request.customRgb,
            request.value);
        return resolution;
    }

    if (resolution.requestedMaterial == "profile_default")
    {
        if (!request.profile.enabled)
        {
            return Unavailable(
                std::move(resolution),
                "E_12E_MODEL_FILL_PROFILE_UNAVAILABLE");
        }
        resolution.profileId = request.profile.name;
        if (request.profile.white.enabled)
        {
            ApplyResolvedMaterial(
                resolution,
                "white",
                request.customRgb,
                request.profile.white.value);
            return resolution;
        }
        if (request.profile.varnish.enabled)
        {
            ApplyResolvedMaterial(
                resolution,
                "varnish",
                request.customRgb,
                request.profile.varnish.value);
            return resolution;
        }
        if (request.profile.rgb.enabled)
        {
            ApplyResolvedMaterial(
                resolution,
                "rgb",
                request.customRgb,
                request.value);
            return resolution;
        }
        return Unavailable(
            std::move(resolution),
            "E_12E_MODEL_FILL_PROFILE_NO_PRINT_MATERIAL");
    }

    if (resolution.requestedMaterial != "material_role")
    {
        return Unavailable(
            std::move(resolution),
            "E_12E_MODEL_FILL_MATERIAL_UNSUPPORTED");
    }
    if (resolution.requestedRole.empty())
    {
        return Unavailable(
            std::move(resolution),
            "E_12E_MODEL_FILL_ROLE_REQUIRED");
    }

    if (IsResolvedMaterial(resolution.requestedRole))
    {
        resolution.profileId = request.profile.name;
        ApplyResolvedMaterial(
            resolution,
            resolution.requestedRole,
            request.customRgb,
            request.value);
        return resolution;
    }

    for (const ModelFillMaterialRoleRegistration& registration :
         request.roleRegistry)
    {
        if (LowerAscii(registration.roleId) != resolution.requestedRole)
        {
            continue;
        }
        const std::string material = LowerAscii(registration.resolvedMaterial);
        if (!IsResolvedMaterial(material))
        {
            return Unavailable(
                std::move(resolution),
                "E_12E_MODEL_FILL_ROLE_MAPPING_INVALID");
        }
        resolution.profileId = registration.profileId;
        ApplyResolvedMaterial(
            resolution,
            material,
            registration.rgb,
            registration.value);
        return resolution;
    }

    return Unavailable(
        std::move(resolution),
        "E_12E_MODEL_FILL_ROLE_UNREGISTERED");
}

}  // namespace slicer_core
