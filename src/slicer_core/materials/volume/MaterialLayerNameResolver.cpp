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

/// @brief 判断字符串是否为不带前导零的正整数十进制表示。
bool IsPositiveDecimal(const std::string& digits)
{
    if (digits.empty() || digits.front() == '0')
    {
        return false;
    }
    return std::all_of(
        digits.begin(), digits.end(), [](const unsigned char character) {
            return std::isdigit(character) != 0;
        });
}

/**
 * @brief 从 `<素材名>[-<同层序号>]-L<图层号>` 拆出主体、序号与层号。
 *
 * 同层同类素材唯一时名字中省略序号（`trans-L1`）；该层同类有多个时带序号
 * （`trans-1-L1`、`trans-2-L1`）。序号剥离后 `base` 才能与词表精确匹配。
 *
 * 语法上无法区分「序号」与「素材名本身以 -<数字> 结尾」，故规范要求素材名
 * 主体不得以 `-<数字>` 结尾；本函数按规范一律将尾部 `-<数字>` 视为序号。
 *
 * @return 不符合规范时返回 false。
 */
bool SplitLayerSuffix(
    const std::string& name, std::string* base, int* layerIndex, int* layer)
{
    const std::size_t marker = name.rfind("-L");
    if (marker == std::string::npos || marker == 0U)
    {
        return false;
    }
// 层号必须为正整数：L0 与前导零形式（L01）都不接受，避免同一层出现两种写法。
    const std::string digits = name.substr(marker + 2U);
    if (!IsPositiveDecimal(digits))
    {
        return false;
    }
    *layer = std::stoi(digits);
    if (*layer <= 0)
    {
        return false;
    }
    std::string head = name.substr(0U, marker);
    *layerIndex = 0;
// 尾部若为 `-<正整数>` 则视为同层序号并剥离；剥离后主体不得为空
    // （`-1-L1` 这类没有素材名的写法不接受）。
    const std::size_t dash = head.rfind('-');
    if (dash != std::string::npos && dash > 0U)
    {
        const std::string indexDigits = head.substr(dash + 1U);
        if (IsPositiveDecimal(indexDigits))
        {
            *layerIndex = std::stoi(indexDigits);
            head = head.substr(0U, dash);
        }
    }
    if (head.empty())
    {
        return false;
    }
    *base = head;
    return true;
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
        int layerIndex{0};
        int layer{0};
        if (!SplitLayerSuffix(material.name, &base, &layerIndex, &layer))
        {
            naming.violations.push_back(
                "material '" + material.name
                + "' does not end with the required -L<n> layer suffix");
            naming.names.push_back(entry);
            continue;
        }
        entry.base_name = base;
        entry.layer_index = layerIndex;
        entry.layer = layer;
        entry.material_class = ClassifyBaseName(base);
        entry.parsed = true;
        naming.max_layer = std::max(naming.max_layer, layer);
        naming.names.push_back(entry);
    }

// 优先级依赖最大层号，故必须在全部解析完成后再算，不能边解析边算。
    naming.priorities.assign(naming.names.size(), 0);
    // 记录每个 priority 的首个占用者及其 (层号, 类别)，用于区分
    // 「同层同类多素材共享优先级」（合法）与「不同层或不同类算出同值」（撞号）。
    struct PriorityOwner
    {
        std::string material_name;
        std::string base_name;
        int layer{0};
        MaterialLayerClass material_class{MaterialLayerClass::Regular};
    };
    std::map<int, PriorityOwner> byPriority;
    for (std::size_t index{0}; index < naming.names.size(); ++index)
    {
        const MaterialLayerName& entry = naming.names.at(index);
        if (!entry.parsed)
        {
            continue;
        }
// 序号【不参与】计算：同层同类素材工艺上等价，理应共享同一 priority。
        const int priority = (naming.max_layer + 1 - entry.layer) * kLayerStep
            + ClassRank(entry.material_class);
        naming.priorities.at(index) = priority;
        const auto existing = byPriority.find(priority);
        if (existing != byPriority.end())
        {
            if (existing->second.layer == entry.layer
                && existing->second.material_class == entry.material_class
                && existing->second.base_name == entry.base_name)
            {
// 同层、同类、【同素材名】：这正是 `<素材名>-<序号>-L<层号>` 要表达的
                // 「同一素材在该层的多块」，不是撞号。二者若在空间上真有重叠，
                // 由 MATVOL 自身的同级重叠阻断兜住；此处硬分先后反而会凭序号
                // 臆造出工艺上并不存在的覆盖关系。
                //
                // 素材名【不同】的同层同类（如 a-L1 与 b-L1）仍判撞号：
                // 序号规则是「同一素材的多块」，不是「同层可放任意多种素材」，
                // 放宽到不同素材名会让两种本应各自裁决的素材静默共享优先级。
                continue;
            }
// MATVOL 对同级优先级 fail-closed，故真撞号必须在此暴露而非留到切片期。
            naming.collisions.push_back(
                "materials '" + existing->second.material_name + "' and '"
                + entry.material_name + "' resolve to the same priority "
                + std::to_string(priority));
            continue;
        }
        byPriority.emplace(
            priority,
            PriorityOwner{
                entry.material_name,
                entry.base_name,
                entry.layer,
                entry.material_class});
    }
    return naming;
}

}  // namespace slicer_core
