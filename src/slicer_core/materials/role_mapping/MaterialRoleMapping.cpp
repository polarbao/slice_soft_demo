#include "slicer_core/materials/role_mapping/MaterialRoleMapping.h"

namespace slicer_core
{

MaterialRoleMappingBoundary MakeMaterialRoleMappingBoundary(const MaterialRoleMappingConfig& config)
{
    return MaterialRoleMappingBoundary{
        config.enabled,
        config.mode,
        config.default_role,
        config.allow_input_support_material,
        config.rules,
    };
}

}  // namespace slicer_core
