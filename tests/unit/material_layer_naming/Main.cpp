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
bool SameLayerSameClassCollisionIsReported()
{
    const auto naming = ResolveMaterialLayerNaming(Materials({"a-L1", "b-L1"}));
    return ExpectTrue(naming.violations.empty(), "both names satisfy the suffix rule")
        && ExpectTrue(naming.collisions.size() == 1U,
               "two regular materials in one layer collide on priority");
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
        && SameLayerSameClassCollisionIsReported()
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
