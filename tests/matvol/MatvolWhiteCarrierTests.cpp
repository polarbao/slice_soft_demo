// MATVOL MV-06：最终 RGB 之后的按需补白与组合窄放行。
//
// 验收覆盖：纯白/近白差异仅在 W；绿色与浅桃色不误补；RGB 与 S/V 逐字节不变；
// 关闭策略时零写入；旧 Stage 15 组合禁令继续生效；MATVOL 窄放行不放宽其他禁令。

#include "slicer_core/config.h"
#include "slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h"
#include "slicer_core/materials/volume/MaterialVolumeWhiteCarrier.h"
#include "slicer_core/reports/MaterialVolumeReport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using slicer_core::ApplyMaterialVolumeWhiteCarrierLayer;
using slicer_core::IsMaterialVolumeWhiteCarrierCombinationAllowed;
using slicer_core::MaterialVolumeWhiteCarrierRequest;
using slicer_core::MaterialVolumeWhiteCarrierStats;
using slicer_core::SliceConfig;

constexpr std::uint8_t kSentinel{0xABU};
constexpr std::uint8_t kWhiteValue{200U};
constexpr std::uint8_t kInkThreshold{8U};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

MaterialVolumeWhiteCarrierRequest MakeEnabledRequest()
{
    MaterialVolumeWhiteCarrierRequest request;
    request.whiteUnderbaseEnabled = true;
    request.inkThreshold = kInkThreshold;
    request.whiteValue = kWhiteValue;
    return request;
}

SliceConfig MakeStage15Config()
{
    SliceConfig config;
    config.input.model_path = "model.obj";
    config.slicing_mode = "relief_heightfield";
    config.texture.enabled = true;
    config.texture.apply_mode = "solid_volume_from_top_surface";
    config.texture.unprintable_white_policy = "white_underbase";
    config.texture.unprintable_white_ink_threshold = kInkThreshold;
    config.texture.unprintable_white_value = kWhiteValue;
    return config;
}

SliceConfig MakeMatvolWhiteConfig()
{
    SliceConfig config;
    config.input.model_path = "model.obj";
    config.slicing_mode = "relief_heightfield";
    config.texture.unprintable_white_policy = "white_underbase";
    config.texture.unprintable_white_ink_threshold = kInkThreshold;
    config.texture.unprintable_white_value = kWhiteValue;
    config.material_volume_policy.enabled = true;
    config.material_volume_policy.overlap.rules.push_back({"01", 200});
    config.material_volume_policy.overlap.rules.push_back({"02", 100});
    return config;
}

bool ExpectConfigRejects(SliceConfig config, const std::string& fragment, const std::string& message)
{
    try
    {
        slicer_core::validate_slice_config(config);
    }
    catch (const std::exception& error)
    {
        const std::string what{error.what()};
        if (what.find(fragment) != std::string::npos)
        {
            return true;
        }
        std::cerr << "FAIL " << message << " (unexpected message: " << what << ")\n";
        return false;
    }
    std::cerr << "FAIL " << message << " (no rejection)\n";
    return false;
}

bool ExpectConfigAccepts(SliceConfig config, const std::string& message)
{
    try
    {
        slicer_core::validate_slice_config(config);
        return true;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << message << " (" << error.what() << ")\n";
        return false;
    }
}

/// @brief 纯白与近白命中补白；绿色与浅桃色不得误补；差异只出现在 W。
bool WhiteCarrierWritesOnlyWhiteChannel()
{
    // 四个像素：纯白、近白（在阈值内）、绿色、浅桃色。
    const std::array<std::array<std::uint8_t, 3>, 4> colours{{
        {255U, 255U, 255U},
        {250U, 249U, 248U},
        {63U, 190U, 126U},
        {255U, 220U, 198U},
    }};
    const std::size_t columnCount = colours.size();
    std::vector<std::uint8_t> rgb(columnCount * 3U, 0U);
    for (std::size_t column{0}; column < columnCount; ++column)
    {
        rgb.at(column * 3U) = colours.at(column)[0];
        rgb.at(column * 3U + 1U) = colours.at(column)[1];
        rgb.at(column * 3U + 2U) = colours.at(column)[2];
    }
    const std::vector<std::uint8_t> rgbBefore = rgb;
    const std::vector<std::uint8_t> mask(columnCount, 1U);
    std::vector<std::uint8_t> white(columnCount, 0U);
    MaterialVolumeWhiteCarrierStats stats;

    ApplyMaterialVolumeWhiteCarrierLayer(MakeEnabledRequest(), rgb, mask, white, stats);

    bool passed{true};
    passed = ExpectTrue(white.at(0U) == kWhiteValue, "pure white pixel receives the white carrier")
        && passed;
    passed = ExpectTrue(white.at(1U) == kWhiteValue, "near white pixel receives the white carrier")
        && passed;
    passed = ExpectTrue(white.at(2U) == 0U, "green pixel is not padded with white") && passed;
    passed = ExpectTrue(white.at(3U) == 0U, "peach pixel is not padded with white") && passed;
    passed = ExpectTrue(
                 stats.unprintableWhiteCarrierPixels == 2U,
                 "stats count exactly the two white-ish pixels")
        && passed;
    passed = ExpectTrue(stats.evaluatedModelPixels == columnCount, "stats count all model pixels")
        && passed;
    passed = ExpectTrue(rgb == rgbBefore, "RGB stays byte-identical after white carrier") && passed;

    // 与既有 Stage 15 谓词逐像素同口径。
    for (std::size_t column{0}; column < columnCount; ++column)
    {
        const bool expected =
            slicer_core::IsUnprintableWhiteTexel(kInkThreshold, colours.at(column));
        const bool actual = white.at(column) == kWhiteValue;
        passed = ExpectTrue(expected == actual, "white decision matches the Stage 15 predicate")
            && passed;
    }
    return passed;
}

/// @brief S 与 V 区域必须逐字节未变；用交错缓冲加哨兵证明。
bool WhiteCarrierLeavesSupportAndVarnishUntouched()
{
    const std::size_t columnCount = 4U;
    std::vector<std::uint8_t> rgb(columnCount * 3U, 255U);
    const std::vector<std::uint8_t> mask(columnCount, 1U);
    // 模拟 W / S / V 三段连续缓冲，只把 W 段交给补白。
    std::vector<std::uint8_t> wsv(columnCount * 3U, kSentinel);
    const std::span<std::uint8_t> whiteRegion{wsv.data(), columnCount};
    MaterialVolumeWhiteCarrierStats stats;

    ApplyMaterialVolumeWhiteCarrierLayer(MakeEnabledRequest(), rgb, mask, whiteRegion, stats);

    bool passed{true};
    for (std::size_t index{columnCount}; index < wsv.size(); ++index)
    {
        if (wsv.at(index) != kSentinel)
        {
            passed = ExpectTrue(false, "support and varnish regions stay untouched") && passed;
            break;
        }
    }
    for (std::size_t column{0}; column < columnCount; ++column)
    {
        passed = ExpectTrue(wsv.at(column) == kWhiteValue, "white region is written") && passed;
    }
    return passed;
}

/// @brief 策略关闭或像素非模型时零写入；缓冲区尺寸不符显式失败。
bool WhiteCarrierRespectsPolicyMaskAndSizes()
{
    const std::size_t columnCount = 4U;
    const std::vector<std::uint8_t> rgb(columnCount * 3U, 255U);
    std::vector<std::uint8_t> white(columnCount, kSentinel);
    MaterialVolumeWhiteCarrierStats stats;

    bool passed{true};
    // 策略关闭：不写、不计数。
    MaterialVolumeWhiteCarrierRequest disabled;
    disabled.inkThreshold = kInkThreshold;
    disabled.whiteValue = kWhiteValue;
    const std::vector<std::uint8_t> mask(columnCount, 1U);
    ApplyMaterialVolumeWhiteCarrierLayer(disabled, rgb, mask, white, stats);
    for (const std::uint8_t value : white)
    {
        passed = ExpectTrue(value == kSentinel, "disabled policy writes nothing") && passed;
    }
    passed = ExpectTrue(
                 stats.unprintableWhiteCarrierPixels == 0U && stats.evaluatedModelPixels == 0U,
                 "disabled policy records no statistics")
        && passed;

    // 非模型像素跳过。
    std::vector<std::uint8_t> partialWhite(columnCount, 0U);
    std::vector<std::uint8_t> partialMask(columnCount, 0U);
    partialMask.at(1U) = 1U;
    MaterialVolumeWhiteCarrierStats partialStats;
    ApplyMaterialVolumeWhiteCarrierLayer(
        MakeEnabledRequest(), rgb, partialMask, partialWhite, partialStats);
    passed = ExpectTrue(partialWhite.at(0U) == 0U, "non-model pixel is skipped") && passed;
    passed = ExpectTrue(partialWhite.at(1U) == kWhiteValue, "model pixel is evaluated") && passed;
    passed = ExpectTrue(
                 partialStats.evaluatedModelPixels == 1U,
                 "only model pixels are counted as evaluated")
        && passed;

    const auto expectInvalid = [](const std::function<void()>& operation, const std::string& message) {
        try
        {
            operation();
        }
        catch (const std::invalid_argument&)
        {
            return true;
        }
        catch (...)
        {
        }
        std::cerr << "FAIL " << message << '\n';
        return false;
    };
    std::vector<std::uint8_t> shortWhite(columnCount - 1U, 0U);
    passed = expectInvalid(
                 [&]() {
                     MaterialVolumeWhiteCarrierStats local;
                     ApplyMaterialVolumeWhiteCarrierLayer(
                         MakeEnabledRequest(), rgb, mask, shortWhite, local);
                 },
                 "mismatched white buffer size is rejected")
        && passed;
    const std::vector<std::uint8_t> shortRgb(columnCount * 3U - 1U, 255U);
    passed = expectInvalid(
                 [&]() {
                     MaterialVolumeWhiteCarrierStats local;
                     std::vector<std::uint8_t> localWhite(columnCount, 0U);
                     ApplyMaterialVolumeWhiteCarrierLayer(
                         MakeEnabledRequest(), shortRgb, mask, localWhite, local);
                 },
                 "mismatched RGB buffer size is rejected")
        && passed;
    return passed;
}

/// @brief 组合窄放行：只放行 MATVOL + white_underbase，其余既有禁令一律保持。
bool CombinationAllowanceStaysNarrow()
{
    bool passed{true};

    // 谓词层面。
    passed = ExpectTrue(
                 IsMaterialVolumeWhiteCarrierCombinationAllowed(true, "white_underbase", false, false),
                 "MATVOL with white_underbase is allowed")
        && passed;
    passed = ExpectTrue(
                 !IsMaterialVolumeWhiteCarrierCombinationAllowed(false, "white_underbase", false, false),
                 "white_underbase alone is not a MATVOL allowance")
        && passed;
    passed = ExpectTrue(
                 !IsMaterialVolumeWhiteCarrierCombinationAllowed(true, "fail_closed", false, false),
                 "MATVOL with fail_closed policy is not an allowance")
        && passed;
    passed = ExpectTrue(
                 !IsMaterialVolumeWhiteCarrierCombinationAllowed(true, "white_underbase", true, false),
                 "materialPolicy breaks the allowance")
        && passed;
    passed = ExpectTrue(
                 !IsMaterialVolumeWhiteCarrierCombinationAllowed(true, "white_underbase", false, true),
                 "materialRoleMapping breaks the allowance")
        && passed;

    // 配置层面：MATVOL + white_underbase 无需纹理顶面投影路径即可通过。
    passed = ExpectConfigAccepts(
                 MakeMatvolWhiteConfig(), "MATVOL with white_underbase passes validation")
        && passed;

    // 旧 Stage 15 组合仍按原样通过。
    passed = ExpectConfigAccepts(
                 MakeStage15Config(), "legacy Stage 15 white_underbase profile still passes")
        && passed;

    // 关闭 MATVOL 时，缺少纹理顶面投影路径的旧禁令必须继续生效。
    SliceConfig legacyWithoutTexture = MakeStage15Config();
    legacyWithoutTexture.texture.enabled = false;
    passed = ExpectConfigRejects(
                 legacyWithoutTexture,
                 "only supports the Legacy full-volume RGB texture path",
                 "legacy white_underbase without the texture path is still rejected")
        && passed;

    // MATVOL + white_underbase + materialPolicy 必须被拒。
    SliceConfig withMaterialPolicy = MakeMatvolWhiteConfig();
    withMaterialPolicy.material_policy.enabled = true;
    passed = ExpectConfigRejects(
                 withMaterialPolicy,
                 "does not support materialPolicy.enabled=true",
                 "MATVOL allowance does not loosen the materialPolicy ban")
        && passed;

    // MATVOL + white_underbase + 旧 roleMapping 必须被拒。
    SliceConfig withRoleMapping = MakeMatvolWhiteConfig();
    withRoleMapping.material_role_mapping.enabled = true;
    withRoleMapping.material_role_mapping.rules.push_back({"01", "rgb"});
    passed = ExpectConfigRejects(
                 withRoleMapping,
                 "does not support materialRoleMapping.enabled=true",
                 "MATVOL allowance does not loosen the materialRoleMapping ban")
        && passed;

    // whiteValue 与 emptyValue 冲突的禁令不得被放宽。
    SliceConfig conflictingWhite = MakeMatvolWhiteConfig();
    conflictingWhite.texture.unprintable_white_value = conflictingWhite.background.value;
    passed = ExpectConfigRejects(
                 conflictingWhite,
                 "must not equal output emptyValue",
                 "MATVOL allowance does not loosen the whiteValue conflict ban")
        && passed;

    return passed;
}


/// @brief 报告顶层字段集必须与 schema 的 required 清单完全一致，防止 builder 与 fixture 漂移。
bool ReportCarriesSchemaRequiredFields()
{
    using slicer_core::AdaptedTriangleMesh;
    using slicer_core::BuildMaterialRgbTable;
    using slicer_core::BuildMaterialVolumePlan;
    using slicer_core::BuildMaterialVolumeReport;
    using slicer_core::ClassifyMaterialTopologies;
    using slicer_core::CountMaterialVolumeLayerOwners;
    using slicer_core::MaterialInfo;
    using slicer_core::MaterializeMaterialOwnershipLayer;
    using slicer_core::MaterialRgbTableRequest;
    using slicer_core::MaterialVolumeBuildRequest;
    using slicer_core::MaterialVolumeGrid;
    using slicer_core::MaterialVolumeLayerStat;
    using slicer_core::MaterialVolumeReportInput;
    using slicer_core::SurfaceTriangleAttributes;
    using slicer_core::Vec3;

    AdaptedTriangleMesh mesh;
    const auto appendVertex = [&mesh](const double x, const double y, const double z) {
        const int index = static_cast<int>(mesh.mesh.vertices.size());
        mesh.mesh.vertices.push_back(Vec3{x, y, z});
        return index;
    };
    const auto appendTriangle =
        [&mesh](const std::string& name, const int a, const int b, const int c) {
            mesh.mesh.triangles.push_back({a, b, c});
            SurfaceTriangleAttributes attributes;
            attributes.source_triangle_index = mesh.triangle_attributes.size();
            attributes.material_name = name;
            mesh.triangle_attributes.push_back(attributes);
        };
    const auto appendBox = [&](const std::string& name, const double loZ, const double hiZ) {
        const int v000 = appendVertex(0.0, 0.0, loZ);
        const int v100 = appendVertex(4.0, 0.0, loZ);
        const int v110 = appendVertex(4.0, 4.0, loZ);
        const int v010 = appendVertex(0.0, 4.0, loZ);
        const int v001 = appendVertex(0.0, 0.0, hiZ);
        const int v101 = appendVertex(4.0, 0.0, hiZ);
        const int v111 = appendVertex(4.0, 4.0, hiZ);
        const int v011 = appendVertex(0.0, 4.0, hiZ);
        appendTriangle(name, v000, v110, v100);
        appendTriangle(name, v000, v010, v110);
        appendTriangle(name, v001, v101, v111);
        appendTriangle(name, v001, v111, v011);
        appendTriangle(name, v000, v100, v101);
        appendTriangle(name, v000, v101, v001);
        appendTriangle(name, v100, v110, v111);
        appendTriangle(name, v100, v111, v101);
        appendTriangle(name, v110, v010, v011);
        appendTriangle(name, v110, v011, v111);
        appendTriangle(name, v010, v000, v001);
        appendTriangle(name, v010, v001, v011);
    };
    appendBox("02", 0.0, 2.0);
    appendBox("01", 3.0, 5.0);

    MaterialVolumeGrid grid;
    grid.widthPx = 4;
    grid.heightPx = 4;
    grid.pixelSizeXMm = 1.0;
    grid.pixelSizeYMm = 1.0;
    grid.layerThicknessMm = 1.0;
    grid.layerCount = 8;

    slicer_core::MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.overlap.rules.push_back({"01", 200});
    policy.overlap.rules.push_back({"02", 100});

    MaterialVolumeBuildRequest buildRequest;
    buildRequest.mesh = &mesh;
    buildRequest.policy = &policy;
    buildRequest.grid = grid;
    const auto plan = BuildMaterialVolumePlan(buildRequest);

    MaterialInfo green;
    green.name = "01";
    green.diffuse_rgb = {63U, 190U, 126U};
    green.has_diffuse = true;
    MaterialInfo peach;
    peach.name = "02";
    peach.diffuse_rgb = {255U, 220U, 198U};
    peach.has_diffuse = true;
    const std::vector<MaterialInfo> infos{green, peach};

    MaterialRgbTableRequest tableRequest;
    tableRequest.plan = &plan;
    tableRequest.materialInfos = infos;
    const auto rgbTable = BuildMaterialRgbTable(tableRequest);
    const auto facts = ClassifyMaterialTopologies(mesh);

    const std::size_t columnCount = 16U;
    const std::vector<std::uint8_t> mask(columnCount, 1U);
    std::vector<std::uint32_t> owner(columnCount, 0U);
    std::vector<MaterialVolumeLayerStat> stats;
    for (const int layer : {0, 3})
    {
        MaterializeMaterialOwnershipLayer(plan, layer, mask, owner);
        stats.push_back(CountMaterialVolumeLayerOwners(plan, layer, owner, mask));
    }

    MaterialVolumeReportInput reportInput;
    reportInput.plan = &plan;
    reportInput.rgbTable = &rgbTable;
    reportInput.policy = &policy;
    reportInput.topologyFacts = facts;
    reportInput.layers = stats;
    const slicer_core::Json report = BuildMaterialVolumeReport(reportInput);

    bool passed{true};
    // materialsWithoutPixels 由 MR-09 新增（被完全压掉的材质清单）。
    // 下方 size() == required.size() 是【闭集】校验，故新增字段必须在此登记，
    // 否则报告多一个键就红——这正是该断言要防的「字段悄悄溜进 schema」。
    const std::array<const char*, 13> required{
        "schema", "packageProtocol", "enabled", "mode", "missingMaterial", "openSurface",
        "overlap", "materials", "materialsWithoutPixels", "totals", "layers", "warnings",
        "errors"};
    for (const char* field : required)
    {
        passed = ExpectTrue(report.contains(field), std::string{"report carries field "} + field)
            && passed;
    }
    passed = ExpectTrue(
                 report.as_object().size() == required.size(),
                 "report has no field outside the schema required set")
        && passed;
    passed = ExpectTrue(
                 report.at("schema").as_string() == "slicesoft.material_volume_report.1",
                 "report pins its schema identifier")
        && passed;
    passed = ExpectTrue(
                 report.at("packageProtocol").as_string() == "p0.rgbwsv.2",
                 "report pins the package protocol")
        && passed;
    passed = ExpectTrue(report.at("materials").size() == 2U, "report lists both materials") && passed;
    passed = ExpectTrue(report.at("layers").size() == 2U, "report lists both sampled layers")
        && passed;
    passed = ExpectTrue(
                 report.at("totals").at("ownerPixels").as_int() == 32,
                 "report totals accumulate owner pixels across layers")
        && passed;
    passed = ExpectTrue(
                 report.at("totals").at("unownedModelPixels").as_int() == 0,
                 "report totals report no unowned model pixels")
        && passed;
    passed = ExpectTrue(
                 report.at("materials").at(0U).at("rgbSource").as_string() == "mtl_kd",
                 "report records the MTL colour source")
        && passed;
    return passed;
}

}  // namespace

int main()
{
    const std::array<std::pair<const char*, std::function<bool()>>, 5> tests{{
        {"white_carrier_writes_only_white_channel", WhiteCarrierWritesOnlyWhiteChannel},
        {"white_carrier_leaves_support_varnish_untouched",
         WhiteCarrierLeavesSupportAndVarnishUntouched},
        {"white_carrier_respects_policy_mask_sizes", WhiteCarrierRespectsPolicyMaskAndSizes},
        {"combination_allowance_stays_narrow", CombinationAllowanceStaysNarrow},
        {"report_carries_schema_required_fields", ReportCarriesSchemaRequiredFields},
    }};

    int failed{0};
    for (const auto& [name, test] : tests)
    {
        try
        {
            if (!test())
            {
                std::cerr << "FAIL " << name << '\n';
                ++failed;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            ++failed;
        }
    }
    if (failed == 0)
    {
        std::cout << "PASS MatvolWhiteCarrierTests " << tests.size() << "/" << tests.size() << '\n';
        return 0;
    }
    std::cerr << "FAIL MatvolWhiteCarrierTests " << failed << " failed\n";
    return 1;
}
