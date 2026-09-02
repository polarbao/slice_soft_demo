#include "slicer_core/materials/volume/MaterialLayerNameResolver.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace slicer_core
{

namespace
{

// 层序【主导】，类别只在同层内决定次序。
// 层步长 100 远大于类别间隔 10，保证任何上层素材都压过任何下层素材——
// L1 是设计上的表面层，其光油必须覆盖 L2 的彩色，而不是相反。
constexpr int kLayerStep{100};
// 同层内次序：透明 > 常规。光油在工艺上是最后覆盖的保护/增亮层，
// 盖在彩色之上才有意义；若让彩色压过光油，那块区域的光油等于没打。
// 弹性材料是结构件，仍居最高。
constexpr int kElasticityRank{30};
constexpr int kTransparentRank{20};
constexpr int kRegularRank{10};

std::string Lowercase(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

/// @brief 从 `<素材名>-L<图层号>` 拆出主体与层号；不符合规范时返回 false。
bool SplitLayerSuffix(const std::string& name, std::string* base, int* layer)
{
    const std::size_t marker = name.rfind("-L");
    if (marker == std::string::npos || marker == 0U)
    {
        return false;
    }
    const std::string digits = name.substr(marker + 2U);
    if (digits.empty()
        || !std::all_of(digits.begin(), digits.end(), [](const unsigned char character) {
               return std::isdigit(character) != 0;
           }))
    {
        return false;
    }
// 层号必须为正整数：L0 与前导零形式（L01）都不接受，避免同一层出现两种写法。
    if (digits.front() == '0')
    {
        return false;
    }
    *layer = std::stoi(digits);
    *base = name.substr(0U, marker);
    return *layer > 0;
}

MaterialLayerClass ClassifyBaseName(const std::string& base)
{
    const std::string lowered = Lowercase(base);
    if (lowered == "transparent" || lowered == "trans")
    {
        return MaterialLayerClass::Transparent;
    }
    if (lowered == "elasticity" || lowered == "el")
    {
        return MaterialLayerClass::Elasticity;
    }
    return MaterialLayerClass::Regular;
}

int ClassRank(const MaterialLayerClass materialClass)
{
    switch (materialClass)
    {
        case MaterialLayerClass::Transparent:
            return kTransparentRank;
        case MaterialLayerClass::Elasticity:
            return kElasticityRank;
        case MaterialLayerClass::Regular:
            return kRegularRank;
    }
    return kRegularRank;
}

}  // namespace

std::string MaterialLayerClassName(const MaterialLayerClass materialClass)
{
    switch (materialClass)
    {
        case MaterialLayerClass::Transparent:
            return "transparent";
        case MaterialLayerClass::Elasticity:
            return "elasticity";
        case MaterialLayerClass::Regular:
            return "regular";
    }
    return "regular";
}

MaterialLayerNaming ResolveMaterialLayerNaming(
    const std::span<const MaterialInfo> materialInfos)
{
    MaterialLayerNaming naming;
    naming.names.reserve(materialInfos.size());

    for (const MaterialInfo& material : materialInfos)
    {
        MaterialLayerName entry;
        entry.material_name = material.name;
        std::string base;
        int layer{0};
        if (!SplitLayerSuffix(material.name, &base, &layer))
        {
            naming.violations.push_back(
                "material '" + material.name
                + "' does not end with the required -L<n> layer suffix");
            naming.names.push_back(entry);
            continue;
        }
        entry.base_name = base;
        entry.layer = layer;
        entry.material_class = ClassifyBaseName(base);
        entry.parsed = true;
        naming.max_layer = std::max(naming.max_layer, layer);
        naming.names.push_back(entry);
    }

// 优先级依赖最大层号，故必须在全部解析完成后再算，不能边解析边算。
    naming.priorities.assign(naming.names.size(), 0);
    std::map<int, std::string> byPriority;
    for (std::size_t index{0}; index < naming.names.size(); ++index)
    {
        const MaterialLayerName& entry = naming.names.at(index);
        if (!entry.parsed)
        {
            continue;
        }
        const int priority = (naming.max_layer + 1 - entry.layer) * kLayerStep
            + ClassRank(entry.material_class);
        naming.priorities.at(index) = priority;
        const auto existing = byPriority.find(priority);
        if (existing != byPriority.end())
        {
// MATVOL 对同级优先级 fail-closed，故撞号必须在此暴露而非留到切片期。
            naming.collisions.push_back(
                "materials '" + existing->second + "' and '" + entry.material_name
                + "' resolve to the same priority " + std::to_string(priority));
            continue;
        }
        byPriority.emplace(priority, entry.material_name);
    }
    return naming;
}

}  // namespace slicer_core
