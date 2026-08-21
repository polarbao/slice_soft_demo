// MATVOL MV-05：owner -> RGB 合成。
//
// 验收覆盖：绿色与浅桃色精确为 MTL 量化值；未绑定材质按策略阻断或走显式 fallback；
// model 像素必须有唯一 owner；只写 RGB，不触碰 W/S/V；顺序无关且可重复。

#include "slicer_core/materials/volume/MaterialLayerRgbComposer.h"
#include "slicer_core/materials/volume/MaterialVolumeError.h"
#include "slicer_core/materials/volume/MaterialVolumePlan.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using slicer_core::AdaptedTriangleMesh;
using slicer_core::BuildMaterialRgbTable;
using slicer_core::BuildMaterialVolumePlan;
using slicer_core::ComposeMaterialLayerRgb;
using slicer_core::kNoMaterialOwner;
using slicer_core::MaterialInfo;
using slicer_core::MaterializeMaterialOwnershipLayer;
using slicer_core::MaterialRgbFallbackPolicy;
using slicer_core::MaterialRgbTable;
using slicer_core::MaterialRgbTableRequest;
using slicer_core::MaterialVolumeBuildRequest;
using slicer_core::MaterialVolumeError;
using slicer_core::MaterialVolumeErrorCode;
using slicer_core::MaterialVolumeGrid;
using slicer_core::MaterialVolumePlan;
using slicer_core::MaterialVolumePolicyConfig;
using slicer_core::SurfaceTriangleAttributes;
using slicer_core::Vec3;

static_assert(!std::is_copy_constructible_v<MaterialRgbTable>);
static_assert(std::is_move_constructible_v<MaterialRgbTable>);

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

int AppendVertex(AdaptedTriangleMesh& mesh, const double x, const double y, const double z)
{
    const int index = static_cast<int>(mesh.mesh.vertices.size());
    mesh.mesh.vertices.push_back(Vec3{x, y, z});
    return index;
}

void AppendTriangle(
    AdaptedTriangleMesh& mesh,
    const std::string& materialName,
    const int a,
    const int b,
    const int c)
{
    mesh.mesh.triangles.push_back({a, b, c});
    SurfaceTriangleAttributes attributes;
    attributes.source_triangle_index = mesh.triangle_attributes.size();
    attributes.material_name = materialName;
    mesh.triangle_attributes.push_back(attributes);
}

void AppendClosedBox(
    AdaptedTriangleMesh& mesh,
    const std::string& materialName,
    const double loZ,
    const double hiZ)
{
    const int v000 = AppendVertex(mesh, 0.0, 0.0, loZ);
    const int v100 = AppendVertex(mesh, 4.0, 0.0, loZ);
    const int v110 = AppendVertex(mesh, 4.0, 4.0, loZ);
    const int v010 = AppendVertex(mesh, 0.0, 4.0, loZ);
    const int v001 = AppendVertex(mesh, 0.0, 0.0, hiZ);
    const int v101 = AppendVertex(mesh, 4.0, 0.0, hiZ);
    const int v111 = AppendVertex(mesh, 4.0, 4.0, hiZ);
    const int v011 = AppendVertex(mesh, 0.0, 4.0, hiZ);
    AppendTriangle(mesh, materialName, v000, v110, v100);
    AppendTriangle(mesh, materialName, v000, v010, v110);
    AppendTriangle(mesh, materialName, v001, v101, v111);
    AppendTriangle(mesh, materialName, v001, v111, v011);
    AppendTriangle(mesh, materialName, v000, v100, v101);
    AppendTriangle(mesh, materialName, v000, v101, v001);
    AppendTriangle(mesh, materialName, v100, v110, v111);
    AppendTriangle(mesh, materialName, v100, v111, v101);
    AppendTriangle(mesh, materialName, v110, v010, v011);
    AppendTriangle(mesh, materialName, v110, v011, v111);
    AppendTriangle(mesh, materialName, v010, v000, v001);
    AppendTriangle(mesh, materialName, v010, v001, v011);
}

MaterialVolumeGrid MakeGrid()
{
    MaterialVolumeGrid grid;
    grid.widthPx = 4;
    grid.heightPx = 4;
    grid.pixelSizeXMm = 1.0;
    grid.pixelSizeYMm = 1.0;
    grid.layerThicknessMm = 1.0;
    grid.layerCount = 8;
    return grid;
}

/// @brief 与 03.mtl 一致的两个材质：绿色 [63,190,126] 与浅桃色 [255,220,198]。
std::vector<MaterialInfo> MakeRealisticMaterialInfos()
{
    MaterialInfo green;
    green.name = "01";
    green.diffuse_rgb = {63U, 190U, 126U};
    green.has_diffuse = true;
    MaterialInfo peach;
    peach.name = "02";
    peach.diffuse_rgb = {255U, 220U, 198U};
    peach.has_diffuse = true;
    return {green, peach};
}

MaterialVolumePolicyConfig MakePolicy()
{
    MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.overlap.rules.push_back({"01", 200});
    policy.overlap.rules.push_back({"02", 100});
    return policy;
}

std::size_t ColumnCountOf(const MaterialVolumeGrid& grid)
{
    return static_cast<std::size_t>(grid.widthPx) * static_cast<std::size_t>(grid.heightPx);
}

/// @brief 纵深两材质的 RGB 必须精确等于 MTL 量化值，且随层切换。
bool DepthOwnersResolveExactMtlColours()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 2.0);
    AppendClosedBox(mesh, "01", 3.0, 5.0);
    const MaterialVolumeGrid grid = MakeGrid();
    const MaterialVolumePolicyConfig policy = MakePolicy();

    MaterialVolumeBuildRequest buildRequest;
    buildRequest.mesh = &mesh;
    buildRequest.policy = &policy;
    buildRequest.grid = grid;
    const MaterialVolumePlan plan = BuildMaterialVolumePlan(buildRequest);

    const std::vector<MaterialInfo> infos = MakeRealisticMaterialInfos();
    MaterialRgbTableRequest tableRequest;
    tableRequest.plan = &plan;
    tableRequest.materialInfos = infos;
    const MaterialRgbTable table = BuildMaterialRgbTable(tableRequest);

    bool passed{true};
    passed = ExpectTrue(table.RgbByMaterial().size() == 2U, "RGB table covers both materials")
        && passed;
    for (const std::string& source : table.RgbSources())
    {
        passed = ExpectTrue(source == "mtl_kd", "RGB source is the MTL diffuse colour") && passed;
    }

    const std::size_t columnCount = ColumnCountOf(grid);
    std::vector<std::uint32_t> owner(columnCount, 0U);
    std::vector<std::uint8_t> rgb(columnCount * 3U, 0U);

    // 层 0..1 由材质 02 拥有 -> 浅桃色；层 3..4 由材质 01 拥有 -> 绿色。
    const std::array<std::pair<int, std::array<std::uint8_t, 3>>, 4> expectations{{
        {0, {255U, 220U, 198U}},
        {1, {255U, 220U, 198U}},
        {3, {63U, 190U, 126U}},
        {4, {63U, 190U, 126U}},
    }};
    for (const auto& [layer, expected] : expectations)
    {
        const std::vector<std::uint8_t> mask(columnCount, 1U);
        MaterializeMaterialOwnershipLayer(plan, layer, mask, owner);
        ComposeMaterialLayerRgb(table, owner, mask, rgb);
        for (std::size_t column{0}; column < columnCount; ++column)
        {
            const std::size_t base = column * 3U;
            if (rgb.at(base) != expected[0] || rgb.at(base + 1U) != expected[1]
                || rgb.at(base + 2U) != expected[2])
            {
                passed = ExpectTrue(false, "composed RGB equals the exact quantized MTL colour")
                    && passed;
                break;
            }
        }
    }
    return passed;
}

/// @brief 缺 Kd 的材质默认 fail closed；显式策略下才允许 fallback。
bool MissingDiffuseFailsClosedUnlessExplicit()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 2.0);
    const MaterialVolumeGrid grid = MakeGrid();
    MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.overlap.rules.push_back({"02", 100});

    MaterialVolumeBuildRequest buildRequest;
    buildRequest.mesh = &mesh;
    buildRequest.policy = &policy;
    buildRequest.grid = grid;
    const MaterialVolumePlan plan = BuildMaterialVolumePlan(buildRequest);

    bool passed{true};
    {
        MaterialRgbTableRequest request;
        request.plan = &plan;
        // 空 materialInfos：材质 02 无 Kd。
        try
        {
            const MaterialRgbTable table = BuildMaterialRgbTable(request);
            (void)table;
            passed = ExpectTrue(false, "missing diffuse without fallback is rejected") && passed;
        }
        catch (const MaterialVolumeError& error)
        {
            passed = ExpectTrue(
                         error.Code() == MaterialVolumeErrorCode::MaterialMissing,
                         "missing diffuse reports E_MATVOL_MATERIAL_MISSING")
                && passed;
        }
    }
    {
        MaterialRgbTableRequest request;
        request.plan = &plan;
        request.fallbackPolicy = MaterialRgbFallbackPolicy::ExplicitFallback;
        request.explicitFallbackRgb = {12U, 34U, 56U};
        const MaterialRgbTable table = BuildMaterialRgbTable(request);
        passed = ExpectTrue(
                     table.RgbByMaterial().size() == 1U
                         && table.RgbByMaterial()[0] == std::array<std::uint8_t, 3>{12U, 34U, 56U},
                     "explicit fallback colour is applied")
            && passed;
        passed = ExpectTrue(
                     table.RgbSources()[0] == "explicit_fallback",
                     "RGB source records the explicit fallback")
            && passed;
    }
    return passed;
}

/// @brief 标记为模型却无 owner 的像素必须报 E_MATVOL_MODEL_PIXEL_UNOWNED。
bool ModelPixelWithoutOwnerFailsClosed()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 2.0);
    const MaterialVolumeGrid grid = MakeGrid();
    MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.overlap.rules.push_back({"02", 100});

    MaterialVolumeBuildRequest buildRequest;
    buildRequest.mesh = &mesh;
    buildRequest.policy = &policy;
    buildRequest.grid = grid;
    const MaterialVolumePlan plan = BuildMaterialVolumePlan(buildRequest);

    const std::vector<MaterialInfo> infos = MakeRealisticMaterialInfos();
    MaterialRgbTableRequest tableRequest;
    tableRequest.plan = &plan;
    tableRequest.materialInfos = infos;
    const MaterialRgbTable table = BuildMaterialRgbTable(tableRequest);

    const std::size_t columnCount = ColumnCountOf(grid);
    // 层 5 位于实体之上：owner 全空，但 mask 谎称此处有模型。
    std::vector<std::uint32_t> owner(columnCount, kNoMaterialOwner);
    const std::vector<std::uint8_t> lyingMask(columnCount, 1U);
    std::vector<std::uint8_t> rgb(columnCount * 3U, 0U);

    bool passed{true};
    try
    {
        ComposeMaterialLayerRgb(table, owner, lyingMask, rgb);
        passed = ExpectTrue(false, "unowned model pixel is rejected") && passed;
    }
    catch (const MaterialVolumeError& error)
    {
        passed = ExpectTrue(
                     error.Code() == MaterialVolumeErrorCode::ModelPixelUnowned,
                     "unowned model pixel reports E_MATVOL_MODEL_PIXEL_UNOWNED")
            && passed;
    }

    // mask 为 0 时写入调用方给定的 unowned 颜色，不报错。
    MaterialRgbTableRequest unownedRequest;
    unownedRequest.plan = &plan;
    unownedRequest.materialInfos = infos;
    unownedRequest.unownedRgb = {7U, 8U, 9U};
    const MaterialRgbTable unownedTable = BuildMaterialRgbTable(unownedRequest);
    const std::vector<std::uint8_t> emptyMask(columnCount, 0U);
    ComposeMaterialLayerRgb(unownedTable, owner, emptyMask, rgb);
    passed = ExpectTrue(
                 rgb.at(0U) == 7U && rgb.at(1U) == 8U && rgb.at(2U) == 9U,
                 "unowned non-model pixel receives the caller-provided colour")
        && passed;
    return passed;
}

/// @brief 合成只写 RGB 区域：越界哨兵字节必须原封不动，证明不触碰 W/S/V。
bool CompositionWritesOnlyRgbRegion()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 2.0);
    const MaterialVolumeGrid grid = MakeGrid();
    MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.overlap.rules.push_back({"02", 100});

    MaterialVolumeBuildRequest buildRequest;
    buildRequest.mesh = &mesh;
    buildRequest.policy = &policy;
    buildRequest.grid = grid;
    const MaterialVolumePlan plan = BuildMaterialVolumePlan(buildRequest);

    const std::vector<MaterialInfo> infos = MakeRealisticMaterialInfos();
    MaterialRgbTableRequest tableRequest;
    tableRequest.plan = &plan;
    tableRequest.materialInfos = infos;
    const MaterialRgbTable table = BuildMaterialRgbTable(tableRequest);

    const std::size_t columnCount = ColumnCountOf(grid);
    const std::vector<std::uint8_t> mask(columnCount, 1U);
    std::vector<std::uint32_t> owner(columnCount, 0U);
    MaterializeMaterialOwnershipLayer(plan, 0, mask, owner);

    // 模拟 RGBWSV 交错缓冲：前 3N 字节交给合成器，其后 3N 字节填哨兵代表 W/S/V。
    constexpr std::uint8_t kSentinel{0xABU};
    std::vector<std::uint8_t> interleaved(columnCount * 6U, kSentinel);
    const std::span<std::uint8_t> rgbRegion{interleaved.data(), columnCount * 3U};
    ComposeMaterialLayerRgb(table, owner, mask, rgbRegion);

    bool passed{true};
    for (std::size_t index{columnCount * 3U}; index < interleaved.size(); ++index)
    {
        if (interleaved.at(index) != kSentinel)
        {
            passed = ExpectTrue(false, "composition leaves the W/S/V region untouched") && passed;
            break;
        }
    }
    passed = ExpectTrue(
                 interleaved.at(0U) == 255U && interleaved.at(1U) == 220U
                     && interleaved.at(2U) == 198U,
                 "composition still writes the RGB region")
        && passed;

    // 缓冲区尺寸不符必须显式失败。
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
    std::vector<std::uint8_t> shortRgb(columnCount * 3U - 1U, 0U);
    passed = expectInvalid(
                 [&]() { ComposeMaterialLayerRgb(table, owner, mask, shortRgb); },
                 "undersized RGB buffer is rejected")
        && passed;
    const std::vector<std::uint8_t> shortMask(columnCount - 1U, 1U);
    std::vector<std::uint8_t> rgb(columnCount * 3U, 0U);
    passed = expectInvalid(
                 [&]() { ComposeMaterialLayerRgb(table, owner, shortMask, rgb); },
                 "mismatched mask size is rejected")
        && passed;
    return passed;
}

/// @brief 材质声明顺序与重复调用都不得改变合成结果。
bool CompositionIsOrderIndependentAndRepeatable()
{
    const MaterialVolumeGrid grid = MakeGrid();
    const MaterialVolumePolicyConfig policy = MakePolicy();
    const std::vector<MaterialInfo> infos = MakeRealisticMaterialInfos();
    const std::size_t columnCount = ColumnCountOf(grid);

    const auto composeLayer = [&](const bool reversedDeclaration, const int layer) {
        AdaptedTriangleMesh mesh;
        if (reversedDeclaration)
        {
            AppendClosedBox(mesh, "01", 2.0, 5.0);
            AppendClosedBox(mesh, "02", 0.0, 3.0);
        }
        else
        {
            AppendClosedBox(mesh, "02", 0.0, 3.0);
            AppendClosedBox(mesh, "01", 2.0, 5.0);
        }
        MaterialVolumeBuildRequest buildRequest;
        buildRequest.mesh = &mesh;
        buildRequest.policy = &policy;
        buildRequest.grid = grid;
        const MaterialVolumePlan plan = BuildMaterialVolumePlan(buildRequest);
        MaterialRgbTableRequest tableRequest;
        tableRequest.plan = &plan;
        tableRequest.materialInfos = infos;
        const MaterialRgbTable table = BuildMaterialRgbTable(tableRequest);
        const std::vector<std::uint8_t> mask(columnCount, 1U);
        std::vector<std::uint32_t> owner(columnCount, 0U);
        MaterializeMaterialOwnershipLayer(plan, layer, mask, owner);
        std::vector<std::uint8_t> rgb(columnCount * 3U, 0U);
        ComposeMaterialLayerRgb(table, owner, mask, rgb);
        return rgb;
    };

    bool passed{true};
    for (const int layer : {0, 2, 4})
    {
        const std::vector<std::uint8_t> forward = composeLayer(false, layer);
        const std::vector<std::uint8_t> reversed = composeLayer(true, layer);
        const std::vector<std::uint8_t> repeated = composeLayer(false, layer);
        passed = ExpectTrue(forward == reversed, "composition is independent of declaration order")
            && passed;
        passed = ExpectTrue(forward == repeated, "composition is repeatable") && passed;
    }
    // 层 2 为重叠层，priority 200 的绿色必须胜出。
    const std::vector<std::uint8_t> overlapped = composeLayer(false, 2);
    passed = ExpectTrue(
                 overlapped.at(0U) == 63U && overlapped.at(1U) == 190U
                     && overlapped.at(2U) == 126U,
                 "overlapped layer composes the higher priority material colour")
        && passed;
    return passed;
}

}  // namespace

int main()
{
    const std::array<std::pair<const char*, std::function<bool()>>, 5> tests{{
        {"depth_owners_resolve_exact_mtl_colours", DepthOwnersResolveExactMtlColours},
        {"missing_diffuse_fails_closed_unless_explicit", MissingDiffuseFailsClosedUnlessExplicit},
        {"model_pixel_without_owner_fails_closed", ModelPixelWithoutOwnerFailsClosed},
        {"composition_writes_only_rgb_region", CompositionWritesOnlyRgbRegion},
        {"composition_order_independent_repeatable", CompositionIsOrderIndependentAndRepeatable},
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
        std::cout << "PASS MatvolRgbComposeTests " << tests.size() << "/" << tests.size() << '\n';
        return 0;
    }
    std::cerr << "FAIL MatvolRgbComposeTests " << failed << " failed\n";
    return 1;
}
