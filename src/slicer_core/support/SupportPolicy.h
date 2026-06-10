#pragma once

#include "slicer_core/config.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Boundary policy for support generation settings.
 */
struct SupportPolicy
{
    bool enabled{false};
    std::string mode;
    int connectivity{8};
    int xy_dilation_px{0};
};

/**
 * @brief Create a support policy from legacy support config.
 * @param config Legacy support config.
 * @return Support policy boundary.
 */
SupportPolicy MakeSupportPolicy(const SupportConfig& config);

}  // namespace slicer_core
