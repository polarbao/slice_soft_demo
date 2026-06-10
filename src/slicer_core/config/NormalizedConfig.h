#pragma once

#include "slicer_core/json_value.h"

#include <string>

namespace slicer_core
{

/**
 * @brief Normalized configuration payload used by R2 migration skeleton.
 */
struct NormalizedConfig
{
    std::string schema;
    Json legacy_root;
};

}  // namespace slicer_core
