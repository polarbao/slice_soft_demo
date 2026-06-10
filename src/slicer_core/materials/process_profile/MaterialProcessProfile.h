#pragma once

#include "slicer_core/config.h"

namespace slicer_core
{

/**
 * @brief Boundary DTO for material process profile configuration.
 */
struct MaterialProcessProfileBoundary
{
    bool enabled{false};
    std::string name;
    std::string target;
    MaterialProcessRgbConfig rgb;
    MaterialProcessWhiteConfig white;
    MaterialProcessVarnishConfig varnish;
    MaterialProcessSupportConfig support;
    MaterialProcessValidationConfig validation;
};

/**
 * @brief Create a material process profile boundary from legacy config.
 * @param config Legacy material process profile config.
 * @return Boundary representation for material process profile.
 */
MaterialProcessProfileBoundary MakeMaterialProcessProfileBoundary(const MaterialProcessProfileConfig& config);

}  // namespace slicer_core
