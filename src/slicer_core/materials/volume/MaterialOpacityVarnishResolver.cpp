#include "slicer_core/materials/volume/MaterialOpacityVarnishResolver.h"

namespace slicer_core
{

MaterialOpacityVarnishResolution ResolveMaterialOpacityVarnish(
    const MaterialVolumePolicyConfig& policy,
    const std::span<const MaterialInfo> materialInfos)
{
    MaterialOpacityVarnishResolution resolution;
    resolution.enabled = policy.opacity_varnish.enabled;
    resolution.opacity_max = policy.opacity_varnish.opacity_max;
    if (!resolution.enabled)
    {
        return resolution;
    }

    resolution.decisions.reserve(materialInfos.size());
    for (const MaterialInfo& material : materialInfos)
    {
        MaterialOpacityDecision decision;
        decision.material_name = material.name;
        decision.opacity = material.opacity;
        decision.declared = material.has_opacity;
        if (!material.has_opacity)
        {
// 未声明不透明度的材质按完全不透明处理，且不出诊断：
// 绝大多数既有资产都不声明，否则诊断会被噪声淹没。
            resolution.decisions.push_back(decision);
            continue;
        }
        if (material.opacity <= resolution.opacity_max)
        {
            decision.varnish = true;
            resolution.varnish_materials.insert(material.name);
        }
        else if (material.opacity < 1.0)
        {
// C4：半透明按工艺语义不是光油，落回 RGB，但必须留下可见诊断，
// 不得静默丢弃设计意图。
            decision.semi_transparent = true;
            resolution.warnings.push_back(
                "semi-transparent material '" + material.name + "' (opacity "
                + std::to_string(material.opacity)
                + ") is not varnish and is processed as RGB");
        }
        resolution.decisions.push_back(decision);
    }
    return resolution;
}

}  // namespace slicer_core
