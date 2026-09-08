#include "slicer_core/materials/volume/MaterialLayerNameResolver.h"

#include "slicer_core/model/FrameGeometry.h"

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
        // 非打印画幅定位保留名不是打印材质：它的面在导入期已由
        // ExtractFrameGeometry 从网格里剥离，但 MTL 的材质表仍然带着它
        // （materialInfos 来自 MTL 解析，不来自面绑定）。故必须在此跳过 ——
        // 否则它会因为没有 -L<n> 后缀被判违规，整份资产被拒。
        //
        // 跳过意味着它既不计入 max_layer、也不产生 overlap 规则，这与
        // DOC_SPEC_MATERIAL_NAMING §6.3「nail-Default 不参与最大层号或
        // priority 计算」一致；它的面已不在网格中，故不需要规则。
        if (IsFrameMaterial(material.name))
        {
            continue;
        }
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
                && existing->second.material_class == entry.material_class)
            {
                // 同层同类共享同一 priority，不是撞号 —— 与素材名是否相同无关。
                //
                // 【2026-09-08 放宽】本判据原先还要求「基名相同」，只承认
                // `<素材名>-<序号>-L<层号>` 那种「同一素材的多块」。用户裁定：
                // 除特殊素材外，需要贴图的素材命名权归设计侧，故同一层里出现
                // 两个【不同名】的常规素材是正常情形（实测 gubao-xin：每层两块
                // 共用同一份贴图数据、仅 UV 不同，被命名为 lcb-L1/cb-L1、
                // lsg-L2/sg-L2、Isg-L3/sg-L3）。原判据会把这三对全判撞号、
                // 整份资产被拒。
                //
                // 放宽不会引入静默错误：同层同类在工艺上等价，若它们在空间上
                // 真有重叠，MaterialVolumePlan 在构建期逐列阻断并点名两个材质
                // （MaterialVolumeErrorCode::OverlapUnresolved，"overlap with
                // equal priority at ..."）。兜底在 MATVOL，不在命名层 ——
                // 这一条是读过该实现后确认的，不是推断。
                continue;
            }
            // 保留该守卫作为公式改动的绊线。
            //
            // 注意：当前公式下它【不可达】—— priority = (max+1-layer)*100 + rank，
            // rank ∈ {10,20,30}，故 |rank 差| <= 20 < 100，两个 priority 相等
            // 必然意味着层号与类别都相同，而那一支已在上面 continue。
            // 若日后改小层步长或增设类别，撞号才会重新可能发生，此时这里会拦住。
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
