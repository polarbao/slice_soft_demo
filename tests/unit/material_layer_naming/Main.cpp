#include "slicer_core/materials/volume/MaterialLayerNameResolver.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{

using slicer_core::MaterialInfo;
using slicer_core::MaterialLayerClass;
using slicer_core::ResolveMaterialLayerNaming;

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

std::vector<MaterialInfo> Materials(const std::vector<std::string>& names)
{
    std::vector<MaterialInfo> materials;
    materials.reserve(names.size());
    for (const std::string& name : names)
    {
        MaterialInfo material;
        material.name = name;
        materials.push_back(material);
    }
    return materials;
}

/// @brief 双图层：上层压过下层，且同层内次序为 弹性 > 透明 > 常规。
bool TwoLayerPrioritiesAreOrdered()
{
    const auto materials = Materials({"sg-L1", "trans-L1", "sg-L2", "trans-L2"});
    const auto naming = ResolveMaterialLayerNaming(materials);
    const auto& p = naming.priorities;
    return ExpectTrue(naming.violations.empty(), "compliant names raise no violation")
        && ExpectTrue(naming.collisions.empty(), "compliant names do not collide")
        && ExpectTrue(naming.max_layer == 2, "max layer is 2")
        && ExpectTrue(p.at(0U) == 210, "sg-L1 is 210")
        && ExpectTrue(p.at(1U) == 220, "trans-L1 is 220")
        && ExpectTrue(p.at(2U) == 110, "sg-L2 is 110")
        && ExpectTrue(p.at(3U) == 120, "trans-L2 is 120")
        && ExpectTrue(p.at(0U) > p.at(2U), "upper regular beats lower regular")
        && ExpectTrue(p.at(1U) > p.at(3U), "upper transparent beats lower transparent")
// 层序主导：L1 的透明素材必须压过 L2 的常规素材。
// L1 是设计上的表面层，其光油要覆盖下层彩色，而不是被下层彩色顶掉。
        && ExpectTrue(p.at(1U) > p.at(2U),
               "an upper transparent material beats a lower regular material");
}

/// @brief 类别识别：标准名与简写等价，且大小写不敏感。
bool ClassesAreRecognizedWithAliasesAndCase()
{
    const auto materials = Materials({
        "transparent-L1", "TRANS-L1", "elasticity-L2", "EL-L2", "nail-L3"});
    const auto naming = ResolveMaterialLayerNaming(materials);
    return ExpectTrue(naming.names.at(0U).material_class == MaterialLayerClass::Transparent,
               "transparent is recognized")
        && ExpectTrue(naming.names.at(1U).material_class == MaterialLayerClass::Transparent,
               "TRANS alias is recognized case-insensitively")
        && ExpectTrue(naming.names.at(2U).material_class == MaterialLayerClass::Elasticity,
               "elasticity is recognized")
        && ExpectTrue(naming.names.at(3U).material_class == MaterialLayerClass::Elasticity,
               "EL alias is recognized case-insensitively")
        && ExpectTrue(naming.names.at(4U).material_class == MaterialLayerClass::Regular,
               "an unreserved base name is regular");
}

/// @brief 同层内次序：弹性 > 透明 > 常规。
///
/// 透明高于常规是刻意的：光油是最后覆盖的保护/增亮层，
/// 盖在彩色之上才有意义；若彩色压过光油，那块区域的光油等于没打。
bool SameLayerClassOrderIsElasticityTransparentRegular()
{
    const auto materials = Materials({"el-L1", "trans-L1", "sg-L1"});
    const auto naming = ResolveMaterialLayerNaming(materials);
    const auto& p = naming.priorities;
    return ExpectTrue(p.at(0U) > p.at(1U), "elasticity beats transparent in the same layer")
        && ExpectTrue(p.at(1U) > p.at(2U), "transparent beats regular in the same layer")
        && ExpectTrue(naming.collisions.empty(), "same-layer classes do not collide");
}

/// @brief 单图层也必须带 -L1（用户 2026-09-01 回签 A2）。
bool SingleLayerStillRequiresSuffix()
{
    const auto compliant = ResolveMaterialLayerNaming(Materials({"trans-L1", "sg-L1"}));
    const auto missing = ResolveMaterialLayerNaming(Materials({"trans", "sg"}));
    return ExpectTrue(compliant.violations.empty(), "single layer with -L1 is compliant")
        && ExpectTrue(compliant.max_layer == 1, "single layer max is 1")
        && ExpectTrue(missing.violations.size() == 2U, "missing suffix is reported per material")
        && ExpectTrue(missing.priorities.at(0U) == 0, "unparsed material gets no priority");
}

/// @brief 违规写法必须被拒，而不是猜测层号。
bool MalformedSuffixesAreRejected()
{
    const auto naming = ResolveMaterialLayerNaming(Materials({
        "sg-L0", "sg-L01", "sg-L", "sg-Lx", "-L1", "sg_L1"}));
    return ExpectTrue(naming.violations.size() == 6U, "all six malformed names are rejected")
        && ExpectTrue(naming.max_layer == 0, "no layer is inferred from malformed names");
}

/// @brief 名字主体不约束，但仍参与唯一性——同层同类重名会撞号并被回报。
/// @brief 【2026-09-08 规则变更】同层同类【不同素材名】共享优先级，不再判撞号。
///
/// 本函数原名 SameLayerSameClassCollisionIsReported，断言的是被推翻的旧规则：
/// 只承认「同一素材的多块」（`<素材名>-<序号>-L<层号>`）共享优先级，素材名不同
/// 就报撞号。用户裁定：除特殊素材外，需要贴图的素材命名权归设计侧，故同层出现
/// 两个不同名的常规素材是正常情形（gubao-xin 每层两块共用同一份贴图、仅 UV 不同）。
///
/// 安全性不依赖此处：若二者空间上真有重叠，MaterialVolumePlan 在构建期逐列
/// 阻断（OverlapUnresolved，"overlap with equal priority at ..."）。
bool SameLayerDifferentNamesShareOnePriority()
{
    const auto naming = ResolveMaterialLayerNaming(Materials({"a-L1", "b-L1"}));
    return ExpectTrue(naming.violations.empty(), "both names satisfy the suffix rule")
        && ExpectTrue(naming.collisions.empty(),
               "two differently named regular materials in one layer no longer collide")
        && ExpectTrue(naming.priorities.at(0U) == naming.priorities.at(1U),
               "they share one priority")
        && ExpectTrue(naming.priorities.at(0U) == 110, "single-layer regular is 110");
}

/// @brief 非打印定位保留名被整体跳过：不报违规、不计入层号、不产生条目。
///
/// materialInfos 来自 MTL 解析而非面绑定，故即使 ExtractFrameGeometry 已把
/// 定位面从网格剥离，`nail-Default` 仍会出现在这里。它没有 -L<n> 后缀，
/// 若不跳过就会被判违规、整份资产被拒（实测 gubao-xin 首个报错即此）。
bool FrameReservedNameIsSkippedEntirely()
{
    const auto naming =
        ResolveMaterialLayerNaming(Materials({"nail-Default", "cb-L1", "trans-L1"}));
    return ExpectTrue(naming.violations.empty(),
               "nail-Default must not be reported as a suffix violation")
        && ExpectTrue(naming.names.size() == 2U,
               "the reserved name produces no naming entry")
        && ExpectTrue(naming.max_layer == 1, "the reserved name does not raise max layer")
        && ExpectTrue(naming.collisions.empty(), "no collision among the printable pair")
        && ExpectTrue(naming.names.at(0U).material_name == "cb-L1",
               "entries keep source order minus the reserved name")
        && ExpectTrue(naming.priorities.at(0U) == 110, "cb-L1 is regular 110")
        && ExpectTrue(naming.priorities.at(1U) == 120, "trans-L1 is transparent 120");
}

/// @brief gubao-xin 新版导出的真实材质集整体解析（本次问题的回归用例）。
///
/// 每层两块共用同一份贴图数据、仅 UV 不同，设计侧自行命名为
/// lcb/cb、lsg/sg、Isg/sg；另有 trans-L1/L2 与 nail-Default。
/// 注意 Isg-L3 首字母是大写 I 而 lsg-L2 是小写 l，两者肉眼难辨但语义不同。
bool GubaoXinNewExportResolves()
{
    const auto naming = ResolveMaterialLayerNaming(Materials({
        "trans-L2", "lsg-L2", "trans-L1", "lcb-L1", "Isg-L3",
        "sg-L2", "cb-L1", "sg-L3", "nail-Default"}));
    const auto& p = naming.priorities;
    return ExpectTrue(naming.violations.empty(), "the new export has no naming violation")
        && ExpectTrue(naming.collisions.empty(), "same-layer pairs no longer collide")
        && ExpectTrue(naming.names.size() == 8U, "nail-Default is excluded, eight remain")
        && ExpectTrue(naming.max_layer == 3, "max layer is 3")
        && ExpectTrue(p.at(2U) == 320, "trans-L1 is 320")
        && ExpectTrue(p.at(3U) == 310 && p.at(6U) == 310,
               "lcb-L1 and cb-L1 share 310")
        && ExpectTrue(p.at(0U) == 220, "trans-L2 is 220")
        && ExpectTrue(p.at(1U) == 210 && p.at(5U) == 210,
               "lsg-L2 and sg-L2 share 210")
        && ExpectTrue(p.at(4U) == 110 && p.at(7U) == 110,
               "Isg-L3 and sg-L3 share 110")
        && ExpectTrue(p.at(2U) > p.at(0U) && p.at(0U) > p.at(4U),
               "layer order still dominates across the three layers");
}

/// @brief 同层同类【同素材名】带序号：共享同一 priority，且不报撞号。
bool SameLayerIndexedMaterialsShareOnePriority()
{
    const auto materials = Materials({"trans-1-L1", "trans-2-L1", "nail-L2"});
    const auto naming = ResolveMaterialLayerNaming(materials);
    const auto& p = naming.priorities;
    return ExpectTrue(naming.violations.empty(), "indexed names satisfy the suffix rule")
        && ExpectTrue(naming.collisions.empty(),
               "same base in one layer shares a priority without collision")
        && ExpectTrue(naming.max_layer == 2, "max layer is 2")
        && ExpectTrue(p.at(0U) == p.at(1U),
               "trans-1-L1 and trans-2-L1 resolve to the same priority")
        && ExpectTrue(p.at(0U) == 220, "indexed transparent in layer 1 is 220")
        && ExpectTrue(p.at(2U) == 110, "nail-L2 is 110")
        && ExpectTrue(p.at(0U) > p.at(2U), "upper layer still beats lower layer");
}

/// @brief 序号被剥离后主体才能与词表精确匹配，故带序号的透明素材仍归透明类。
bool IndexStrippedBeforeClassMatching()
{
    const auto naming = ResolveMaterialLayerNaming(
        Materials({"trans-1-L1", "el-2-L1", "nail-3-L1"}));
    return ExpectTrue(naming.names.at(0U).material_class == MaterialLayerClass::Transparent,
               "trans-1-L1 is transparent, not regular")
        && ExpectTrue(naming.names.at(1U).material_class == MaterialLayerClass::Elasticity,
               "el-2-L1 is elasticity")
        && ExpectTrue(naming.names.at(2U).material_class == MaterialLayerClass::Regular,
               "nail-3-L1 is regular")
        && ExpectTrue(naming.names.at(0U).base_name == "trans",
               "index is stripped from the base name")
        && ExpectTrue(naming.names.at(0U).layer_index == 1, "layer index is parsed")
        && ExpectTrue(naming.names.at(2U).layer_index == 3, "layer index 3 is parsed");
}

/// @brief 省略序号时 layer_index 为 0，且与带序号形式落在同一 priority。
bool OmittedIndexIsZeroAndMatchesIndexedForm()
{
    const auto naming = ResolveMaterialLayerNaming(
        Materials({"trans-L1", "trans-1-L1"}));
    return ExpectTrue(naming.violations.empty(), "both forms satisfy the suffix rule")
        && ExpectTrue(naming.collisions.empty(),
               "omitted and indexed forms of one base do not collide")
        && ExpectTrue(naming.names.at(0U).layer_index == 0, "omitted index is 0")
        && ExpectTrue(naming.names.at(1U).layer_index == 1, "explicit index is 1")
        && ExpectTrue(naming.priorities.at(0U) == naming.priorities.at(1U),
               "both forms resolve to the same priority");
}

/// @brief 序号非正整数或带前导零时不视为序号，主体因此不被截断。
bool MalformedIndexIsNotTreatedAsIndex()
{
    const auto naming = ResolveMaterialLayerNaming(
        Materials({"trans-0-L1", "trans-01-L1"}));
    return ExpectTrue(naming.violations.empty(), "suffix itself is still valid")
        && ExpectTrue(naming.names.at(0U).layer_index == 0,
               "-0- is not accepted as an index")
        && ExpectTrue(naming.names.at(0U).base_name == "trans-0",
               "malformed index stays in the base name")
        && ExpectTrue(naming.names.at(1U).base_name == "trans-01",
               "leading-zero index stays in the base name")
        && ExpectTrue(naming.names.at(0U).material_class == MaterialLayerClass::Regular,
               "trans-0 no longer matches the transparent vocabulary");
}

/// @brief gubao04 的真实六材质三图层组合：无违规、无撞号、优先级与规范一致。
bool Gubao04SixMaterialsResolveAsSpecified()
{
    const auto naming = ResolveMaterialLayerNaming(Materials(
        {"trans-L1", "nail-2-L1", "nail-1-L1", "trans-L2", "nail-L2", "nail-L3"}));
    const auto& p = naming.priorities;
    return ExpectTrue(naming.violations.empty(), "gubao04 names raise no violation")
        && ExpectTrue(naming.collisions.empty(), "gubao04 names do not collide")
        && ExpectTrue(naming.max_layer == 3, "max layer is 3")
        && ExpectTrue(p.at(0U) == 320, "trans-L1 is 320")
        && ExpectTrue(p.at(1U) == 310, "nail-2-L1 is 310")
        && ExpectTrue(p.at(2U) == 310, "nail-1-L1 shares 310 with nail-2-L1")
        && ExpectTrue(p.at(3U) == 220, "trans-L2 is 220")
        && ExpectTrue(p.at(4U) == 210, "nail-L2 is 210")
        && ExpectTrue(p.at(5U) == 110, "nail-L3 is 110");
}

/// @brief 三图层时层内偏移随最大层号伸缩，仍保持上层压过下层。
bool ThreeLayersScaleTheOffset()
{
    const auto materials = Materials({"sg-L1", "sg-L2", "sg-L3"});
    const auto naming = ResolveMaterialLayerNaming(materials);
    const auto& p = naming.priorities;
    return ExpectTrue(naming.max_layer == 3, "max layer is 3")
        && ExpectTrue(p.at(0U) == 310, "sg-L1 is 310 with three layers")
        && ExpectTrue(p.at(1U) == 210, "sg-L2 is 210")
        && ExpectTrue(p.at(2U) == 110, "sg-L3 is 110")
        && ExpectTrue(naming.collisions.empty(), "distinct layers do not collide");
}

/// @brief 真实旧命名资产（tm2-3）必须被判为违规，而不是猜出层号。
///
/// tm2-3 的四个材质为 sg-1 / touming-2 / touming-1 / sg-2：既无 -L<n> 后缀，
/// 且其中 touming-2 位于【上层】、touming-1 位于【下层】——编号与层序【反向】。
/// 若解析器试图从尾部数字推层序，会把两层完全推反。故必须整体拒绝。
bool LegacyTm23NamesAreRejectedNotGuessed()
{
    const auto naming = ResolveMaterialLayerNaming(
        Materials({"sg-1", "touming-2", "touming-1", "sg-2"}));
    return ExpectTrue(naming.violations.size() == 4U,
               "all four legacy tm2-3 names are reported as violations")
        && ExpectTrue(naming.max_layer == 0, "no layer is guessed from legacy names")
        && ExpectTrue(naming.priorities.at(0U) == 0
               && naming.priorities.at(1U) == 0
               && naming.priorities.at(2U) == 0
               && naming.priorities.at(3U) == 0,
               "legacy names receive no auto priority");
}

/// @brief 规范化后的同一资产（spec-l）必须完整解出四材质两图层。
bool NormalizedSpecLNamesAreAccepted()
{
    const auto naming = ResolveMaterialLayerNaming(
        Materials({"nail-L1", "trans-L1", "trans-L2", "nail-L2"}));
    const auto& p = naming.priorities;
    return ExpectTrue(naming.violations.empty(), "normalized names raise no violation")
        && ExpectTrue(naming.collisions.empty(), "normalized names do not collide")
        && ExpectTrue(naming.max_layer == 2, "two layers are resolved")
        && ExpectTrue(p.at(1U) == 220, "trans-L1 is 220")
        && ExpectTrue(p.at(0U) == 210, "nail-L1 is 210")
        && ExpectTrue(p.at(2U) == 120, "trans-L2 is 120")
        && ExpectTrue(p.at(3U) == 110, "nail-L2 is 110");
}

}  // namespace

int main()
{
    const bool ok = TwoLayerPrioritiesAreOrdered()
        && ClassesAreRecognizedWithAliasesAndCase()
        && SameLayerClassOrderIsElasticityTransparentRegular()
        && SingleLayerStillRequiresSuffix()
        && MalformedSuffixesAreRejected()
        && SameLayerDifferentNamesShareOnePriority()
        && FrameReservedNameIsSkippedEntirely()
        && GubaoXinNewExportResolves()
        && SameLayerIndexedMaterialsShareOnePriority()
        && IndexStrippedBeforeClassMatching()
        && OmittedIndexIsZeroAndMatchesIndexedForm()
        && MalformedIndexIsNotTreatedAsIndex()
        && Gubao04SixMaterialsResolveAsSpecified()
        && ThreeLayersScaleTheOffset()
        && LegacyTm23NamesAreRejectedNotGuessed()
        && NormalizedSpecLNamesAreAccepted();
    if (!ok)
    {
        return 1;
    }

    std::cout << "material_layer_naming_unit_tests: PASS\n";
    return 0;
}
