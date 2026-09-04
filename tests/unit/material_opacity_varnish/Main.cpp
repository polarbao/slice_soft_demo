#include "slicer_core/materials/volume/MaterialOpacityVarnishResolver.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{

using slicer_core::MaterialInfo;
using slicer_core::MaterialVolumePolicyConfig;
using slicer_core::ResolveMaterialOpacityVarnish;

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

MaterialInfo Material(const std::string& name, const double opacity, const bool declared)
{
    MaterialInfo material;
    material.name = name;
    material.opacity = opacity;
    material.has_opacity = declared;
    return material;
}

MaterialVolumePolicyConfig Policy(const bool enabled, const double opacityMax)
{
    MaterialVolumePolicyConfig policy;
    policy.opacity_varnish.enabled = enabled;
    policy.opacity_varnish.opacity_max = opacityMax;
    return policy;
}

/// @brief 未启用时必须完全惰性，不判定也不出诊断。
bool DisabledYieldsNothing()
{
    const std::vector<MaterialInfo> materials{Material("touming", 0.0, true)};
    const auto result = ResolveMaterialOpacityVarnish(Policy(false, 0.001), materials);
    return ExpectTrue(!result.enabled, "disabled policy reports disabled")
        && ExpectTrue(result.varnish_materials.empty(), "disabled policy selects nothing")
        && ExpectTrue(result.warnings.empty(), "disabled policy warns about nothing")
        && ExpectTrue(result.decisions.empty(), "disabled policy decides nothing");
}

/// @brief C1：d=0 的材质判为光油。
bool FullyTransparentIsVarnish()
{
    const std::vector<MaterialInfo> materials{Material("touming", 0.0, true)};
    const auto result = ResolveMaterialOpacityVarnish(Policy(true, 0.001), materials);
    return ExpectTrue(result.varnish_materials.count("touming") == 1U,
               "opacity 0.0 is varnish")
        && ExpectTrue(result.warnings.empty(), "fully transparent does not warn")
        && ExpectTrue(result.decisions.at(0U).varnish, "decision records varnish");
}

/// @brief C1：判据走容差，恰好等于上限也算光油。
bool ThresholdIsInclusiveAndUsesTolerance()
{
    const std::vector<MaterialInfo> materials{
        Material("at_max", 0.001, true),
        Material("just_above", 0.0011, true)};
    const auto result = ResolveMaterialOpacityVarnish(Policy(true, 0.001), materials);
    return ExpectTrue(result.varnish_materials.count("at_max") == 1U,
               "opacity equal to the max is varnish")
        && ExpectTrue(result.varnish_materials.count("just_above") == 0U,
               "opacity above the max is not varnish");
}

/// @brief C4：半透明不是光油，按 RGB 处理，且必须出诊断。
bool SemiTransparentIsRgbWithWarning()
{
    const std::vector<MaterialInfo> materials{Material("half", 0.42, true)};
    const auto result = ResolveMaterialOpacityVarnish(Policy(true, 0.001), materials);
    return ExpectTrue(result.varnish_materials.empty(), "0.42 is not varnish")
        && ExpectTrue(result.warnings.size() == 1U, "semi-transparent warns exactly once")
        && ExpectTrue(result.decisions.at(0U).semi_transparent,
               "decision records semi transparency")
        && ExpectTrue(result.warnings.at(0U).find("half") != std::string::npos,
               "the warning names the material");
}

/// @brief 完全不透明既不是光油也不出诊断。
bool OpaqueIsSilent()
{
    const std::vector<MaterialInfo> materials{Material("sg", 1.0, true)};
    const auto result = ResolveMaterialOpacityVarnish(Policy(true, 0.001), materials);
    return ExpectTrue(result.varnish_materials.empty(), "opaque is not varnish")
        && ExpectTrue(result.warnings.empty(), "opaque does not warn");
}

/// @brief 未声明不透明度的材质按不透明处理且不出诊断，避免既有资产产生噪声。
bool UndeclaredOpacityIsSilentlyOpaque()
{
    const std::vector<MaterialInfo> materials{Material("legacy", 1.0, false)};
    const auto result = ResolveMaterialOpacityVarnish(Policy(true, 0.001), materials);
    return ExpectTrue(result.varnish_materials.empty(), "undeclared is not varnish")
        && ExpectTrue(result.warnings.empty(), "undeclared does not warn")
        && ExpectTrue(!result.decisions.at(0U).declared, "decision records undeclared");
}

/// @brief 双图层资产：两层透明素材各自独立判为光油。
bool TwoLayerAssetSelectsBothTransparentMaterials()
{
    const std::vector<MaterialInfo> materials{
        Material("touming-1", 0.0, true),
        Material("sg-1", 1.0, true),
        Material("touming-2", 0.0, true),
        Material("sg-2", 1.0, true)};
    const auto result = ResolveMaterialOpacityVarnish(Policy(true, 0.001), materials);
    return ExpectTrue(result.varnish_materials.size() == 2U, "both layers select varnish")
        && ExpectTrue(result.varnish_materials.count("touming-1") == 1U, "layer 1 selected")
        && ExpectTrue(result.varnish_materials.count("touming-2") == 1U, "layer 2 selected")
        && ExpectTrue(result.warnings.empty(), "no spurious warnings on a clean asset");
}

}  // namespace

int main()
{
    const bool ok = DisabledYieldsNothing()
        && FullyTransparentIsVarnish()
        && ThresholdIsInclusiveAndUsesTolerance()
        && SemiTransparentIsRgbWithWarning()
        && OpaqueIsSilent()
        && UndeclaredOpacityIsSilentlyOpaque()
        && TwoLayerAssetSelectsBothTransparentMaterials();
    if (!ok)
    {
        return 1;
    }

    std::cout << "material_opacity_varnish_unit_tests: PASS\n";
    return 0;
}
