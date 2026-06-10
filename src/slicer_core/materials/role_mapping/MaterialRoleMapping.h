#pragma once

#include "slicer_core/config.h"

#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Boundary DTO for material role mapping configuration.
 */
struct MaterialRoleMappingBoundary
{
    bool enabled{false};
    std::string mode;
    std::string default_role;
    bool allow_input_support_material{false};
    std::vector<MaterialRoleRuleConfig> rules;
};

/**
 * @brief Create a material role mapping boundary from legacy config.
 * @param config Legacy material role mapping config.
 * @return Boundary representation for material role mapping.
 */
MaterialRoleMappingBoundary MakeMaterialRoleMappingBoundary(const MaterialRoleMappingConfig& config);

}  // namespace slicer_core
