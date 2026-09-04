#include "slicer_core/model/MtlMaterialParser.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{

using slicer_core::MaterialInfo;
using slicer_core::model_detail::ApplyMtlMaterialLine;
using slicer_core::model_detail::MtlMaterialContext;
using slicer_core::model_detail::MtlMaterialLineResult;
using slicer_core::model_detail::TrimMaterialName;

constexpr double kTolerance = 1.0e-9;

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool NearlyEqual(const double left, const double right)
{
    return std::abs(left - right) <= kTolerance;
}

MtlMaterialLineResult Apply(
    const std::string& token,
    const std::string& arguments,
    MaterialInfo* material)
{
    const MtlMaterialContext context{};
    return ApplyMtlMaterialLine(token, arguments, context, material);
}

/// @brief 未声明不透明度时必须保持完全不透明的默认值。
bool DefaultsToOpaque()
{
    const MaterialInfo material;
    return ExpectTrue(!material.has_opacity, "fresh material declares no opacity")
        && ExpectTrue(NearlyEqual(material.opacity, 1.0), "default opacity is 1.0");
}

/// @brief `d` 直接就是不透明度。
bool DissolveIsOpacityDirectly()
{
    MaterialInfo material;
    const MtlMaterialLineResult result = Apply("d", " 0.4200", &material);
    return ExpectTrue(result.applied, "d is recognized")
        && ExpectTrue(!result.opacity_conflict, "single d does not conflict")
        && ExpectTrue(material.has_opacity, "d sets has_opacity")
        && ExpectTrue(NearlyEqual(material.opacity, 0.42), "d 0.42 yields opacity 0.42");
}

/// @brief `Tr` 与 `d` 语义相反，必须归一为 opacity = 1 - Tr。
bool TransmissionIsInverted()
{
    MaterialInfo material;
    const MtlMaterialLineResult result = Apply("Tr", " 1.0000", &material);
    return ExpectTrue(result.applied, "Tr is recognized")
        && ExpectTrue(NearlyEqual(material.opacity, 0.0), "Tr 1.0 yields opacity 0.0");
}

/// @brief `d 0` 与 `Tr 1` 说的是同一件事，不得报矛盾。
bool AgreeingDissolveAndTransmissionDoNotConflict()
{
    MaterialInfo material;
    const MtlMaterialLineResult first = Apply("d", " 0.0000", &material);
    const MtlMaterialLineResult second = Apply("Tr", " 1.0000", &material);
    return ExpectTrue(first.applied && second.applied, "both tokens are recognized")
        && ExpectTrue(!second.opacity_conflict, "d 0 and Tr 1 agree")
        && ExpectTrue(NearlyEqual(material.opacity, 0.0), "agreed opacity stays 0.0");
}

/// @brief 自相矛盾的文件必须被回报，而不是让后出现的令牌静默胜出。
bool ContradictingOpacityIsReported()
{
    MaterialInfo material;
    const MtlMaterialLineResult first = Apply("d", " 0.0000", &material);
    const MtlMaterialLineResult second = Apply("Tr", " 0.0000", &material);
    return ExpectTrue(first.applied && second.applied, "both tokens are recognized")
        && ExpectTrue(!first.opacity_conflict, "first token cannot conflict")
        && ExpectTrue(second.opacity_conflict, "d 0.0 with Tr 0.0 is reported as a conflict");
}

/// @brief 越界与非法数值必须被夹紧或拒绝，不得污染下游。
bool OutOfRangeAndMalformedAreHandled()
{
    MaterialInfo high;
    Apply("d", " 4.5", &high);
    MaterialInfo low;
    Apply("Tr", " 9.0", &low);
    MaterialInfo malformed;
    const MtlMaterialLineResult bad = Apply("d", " not-a-number", &malformed);
    return ExpectTrue(NearlyEqual(high.opacity, 1.0), "d above 1 clamps to 1.0")
        && ExpectTrue(NearlyEqual(low.opacity, 0.0), "Tr above 1 clamps opacity to 0.0")
        && ExpectTrue(!bad.applied, "malformed value is not applied")
        && ExpectTrue(!malformed.has_opacity, "malformed value leaves has_opacity false");
}

/// @brief 既有 Kd 行为必须与下沉前完全一致。
bool DiffuseColorIsUnchanged()
{
    MaterialInfo material;
    const MtlMaterialLineResult result = Apply("Kd", " 0.9804 0.9804 0.9804", &material);
    return ExpectTrue(result.applied, "Kd is recognized")
        && ExpectTrue(material.has_diffuse, "Kd sets has_diffuse")
        && ExpectTrue(material.diffuse_rgb.at(0U) == 250U, "0.9804 quantizes to 250")
        && ExpectTrue(material.diffuse_rgb.at(1U) == 250U, "green quantizes to 250")
        && ExpectTrue(material.diffuse_rgb.at(2U) == 250U, "blue quantizes to 250");
}

/// @brief 不相关的关键字与空材质指针不得产生副作用。
bool UnrelatedTokensAndNullMaterialAreIgnored()
{
    MaterialInfo material;
    const MtlMaterialLineResult ignored = Apply("Ns", " 0.0000", &material);
    const MtlMaterialLineResult nullTarget = Apply("d", " 0.5", nullptr);
    return ExpectTrue(!ignored.applied, "Ns is not consumed")
        && ExpectTrue(!material.has_opacity, "Ns leaves opacity untouched")
        && ExpectTrue(!material.has_diffuse, "Ns leaves diffuse untouched")
        && ExpectTrue(!nullTarget.applied, "null material is rejected");
}

/// @brief MO-06：带空格的材质名必须整行保留，不得截断为首个令牌。
bool MaterialNameKeepsInnerSpaces()
{
    return ExpectTrue(TrimMaterialName(" sg (1)") == "sg (1)", "sg (1) is kept whole")
        && ExpectTrue(TrimMaterialName(" sg (2)") == "sg (2)", "sg (2) is kept whole")
        && ExpectTrue(TrimMaterialName(" sg (1)") != TrimMaterialName(" sg (2)"),
               "sg (1) and sg (2) stay distinct")
        && ExpectTrue(TrimMaterialName("  touming  ") == "touming", "outer whitespace is trimmed")
        && ExpectTrue(TrimMaterialName("   ").empty(), "all-whitespace yields an empty name")
        && ExpectTrue(TrimMaterialName("").empty(), "empty input yields an empty name");
}

}  // namespace

int main()
{
    const bool ok = DefaultsToOpaque()
        && DissolveIsOpacityDirectly()
        && TransmissionIsInverted()
        && AgreeingDissolveAndTransmissionDoNotConflict()
        && ContradictingOpacityIsReported()
        && OutOfRangeAndMalformedAreHandled()
        && DiffuseColorIsUnchanged()
        && UnrelatedTokensAndNullMaterialAreIgnored()
        && MaterialNameKeepsInnerSpaces();
    if (!ok)
    {
        return 1;
    }

    std::cout << "mtl_material_parser_unit_tests: PASS\n";
    return 0;
}
