#pragma once

#include "slicer_core/materials/process_profile/MaterialProcessProfile.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief One explicitly calibrated material-role mapping for Model Fill.
 */
struct ModelFillMaterialRoleRegistration
{
    std::string roleId;
    std::string resolvedMaterial;
    std::array<std::uint8_t, 3> rgb{0U, 0U, 0U};
    std::uint8_t value{0U};
    std::string profileId;
};

/**
 * @brief Request for resolving a user-facing Model Fill selection.
 */
struct ModelFillMaterialResolveRequest
{
    std::string requestedMaterial{"white"};
    std::string requestedRole;
    std::array<std::uint8_t, 3> customRgb{0U, 0U, 0U};
    std::uint8_t value{0U};
    MaterialProcessProfileBoundary profile;
    std::vector<ModelFillMaterialRoleRegistration> roleRegistry;
};

/**
 * @brief Explicit RGBWSV-compatible result of Model Fill material resolution.
 */
struct ModelFillMaterialResolution
{
    bool available{false};
    std::string requestedMaterial;
    std::string requestedRole;
    std::string resolvedMaterial{"unavailable"};
    std::array<std::uint8_t, 6> resolvedChannels{
        255U,
        255U,
        255U,
        255U,
        255U,
        255U};
    std::array<std::uint8_t, 3> resolvedRgb{0U, 0U, 0U};
    std::uint8_t resolvedValue{0U};
    std::string profileId;
    std::string reasonCode;
};

/**
 * @brief Resolve built-in, profile-default, or registered role Model Fill material.
 * @param request Requested material, optional role, process profile, and calibrated registry.
 * @return Existing RGBWSV channel mapping or a stable unavailable reason.
 */
ModelFillMaterialResolution ResolveModelFillMaterial(
    const ModelFillMaterialResolveRequest& request);

}  // namespace slicer_core
