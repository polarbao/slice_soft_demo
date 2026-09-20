#include "slicer_core/config.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/transfer/TransferChannelError.h"
#include "slicer_core/materials/transfer/TransferMaterialVolumePlan.h"
#include "slicer_core/model.h"
#include "slicer_core/model/ModelLoadConfig.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

std::filesystem::path RealityAsset(const std::string& fileName)
{
#ifdef SLICESOFT_SOURCE_DIR
    return std::filesystem::path{SLICESOFT_SOURCE_DIR} / "model" / "obj" / "reality"
        / "finger_suoguo" / fileName;
#else
    return std::filesystem::path{"model"} / "obj" / "reality" / "finger_suoguo"
        / fileName;
#endif
}

slicer_core::ModelReport LoadProductionPosture(const std::filesystem::path& path)
{
    slicer_core::ModelLoadConfig config;
    config.input.model_path = path;
    config.input.format = "obj";
    config.auto_orient.enabled = true;
    return slicer_core::load_model_report(config, path.parent_path());
}

slicer_core::MaterialVolumeGrid CoarseGrid(const slicer_core::ModelReport& model)
{
    constexpr double pixelMm{0.50};
    constexpr double layerMm{0.038};
    slicer_core::MaterialVolumeGrid grid;
    grid.originXMm = model.bbox_mm.min.x;
    grid.originYMm = model.bbox_mm.min.y;
    grid.pixelSizeXMm = pixelMm;
    grid.pixelSizeYMm = pixelMm;
    grid.widthPx = std::max(
        1, static_cast<int>((model.bbox_mm.max.x - model.bbox_mm.min.x) / pixelMm));
    grid.heightPx = std::max(
        1, static_cast<int>((model.bbox_mm.max.y - model.bbox_mm.min.y) / pixelMm));
    grid.layerThicknessMm = layerMm;
    grid.layerCount = std::max(
        1, static_cast<int>((model.bbox_mm.max.z - model.bbox_mm.min.z) / layerMm));
    return grid;
}

slicer_core::TransferChannelPolicyConfig PolicyFor(
    const std::array<std::uint8_t, 3>& rgb)
{
    slicer_core::TransferChannelPolicyConfig policy;
    policy.enabled = true;
    policy.material_diffuse_rgb_values.push_back(rgb);
    policy.topology.self_intersection_policy = "tolerate_closed_self_intersection";
    policy.topology.max_self_intersection_pairs = 64;
    return policy;
}

bool Reality03BuildsMaterial02OnlyPlan()
{
    const std::filesystem::path path = RealityAsset("03.obj");
    if (!std::filesystem::exists(path))
    {
        std::cout << "SKIP 03.obj not present\n";
        return true;
    }
    const slicer_core::ModelReport model = LoadProductionPosture(path);
    const slicer_core::AdaptedTriangleMesh mesh =
        slicer_core::AdaptSceneModelToTriangleMesh(model);
    const slicer_core::MaterialVolumeGrid grid = CoarseGrid(model);
    const slicer_core::TransferMaterialVolumePlan plan =
        slicer_core::BuildTransferMaterialVolumePlan(
            PolicyFor({255U, 220U, 198U}), mesh, grid);

    bool passed = ExpectTrue(plan.HasRegion(), "03 has a transfer volume plan");
    passed = ExpectTrue(
                 plan.material.materialName == "02", "03 plan selects material 02")
        && passed;
    passed = ExpectTrue(
                 plan.volume->MaterialNames().size() == 1U
                     && plan.volume->MaterialNames().front() == "02",
                 "plan interval ownership excludes nail material 01")
        && passed;

    std::vector<std::uint8_t> modelMask(plan.columnCount, 1U);
    std::vector<std::uint32_t> owner(plan.columnCount);
    std::vector<std::uint8_t> mask(plan.columnCount);
    std::uint64_t transferPixels{0U};
    for (int layer{0}; layer < plan.layerCount; ++layer)
    {
        slicer_core::MaterializeTransferLayerMask(
            plan, layer, modelMask, owner, mask);
        transferPixels += static_cast<std::uint64_t>(
            std::count(mask.begin(), mask.end(), static_cast<std::uint8_t>(1U)));
    }
    return ExpectTrue(transferPixels > 0U, "03 material 02 produces non-empty T occupancy")
        && passed;
}

bool MissingOptionalRegionMaterializesEmptyMask()
{
    slicer_core::AdaptedTriangleMesh mesh;
    mesh.material_infos.push_back(
        slicer_core::MaterialInfo{"01", {63U, 190U, 126U}, true});
    slicer_core::MaterialVolumeGrid grid;
    grid.widthPx = 2;
    grid.heightPx = 2;
    grid.layerCount = 1;
    grid.pixelSizeXMm = 1.0;
    grid.pixelSizeYMm = 1.0;
    grid.layerThicknessMm = 1.0;
    const slicer_core::TransferMaterialVolumePlan plan =
        slicer_core::BuildTransferMaterialVolumePlan(
            PolicyFor({255U, 220U, 198U}), mesh, grid);

    std::vector<std::uint8_t> modelMask(4U, 1U);
    std::vector<std::uint32_t> owner(4U, 0U);
    std::vector<std::uint8_t> mask(4U, 9U);
    slicer_core::MaterializeTransferLayerMask(plan, 0, modelMask, owner, mask);
    return ExpectTrue(!plan.HasRegion(), "missing optional transfer has no plan")
        && ExpectTrue(
            std::all_of(mask.begin(), mask.end(), [](const std::uint8_t value)
            {
                return value == 0U;
            }),
            "missing optional transfer writes an empty mask");
}

bool OpenMatchedRegionFailsClosed()
{
    slicer_core::ModelReport scene;
    scene.material_infos.push_back(
        slicer_core::MaterialInfo{"custom", {10U, 20U, 30U}, true});
    slicer_core::Triangle triangle;
    triangle.a = {0.0, 0.0, 0.0};
    triangle.b = {1.0, 0.0, 0.0};
    triangle.c = {0.0, 1.0, 0.0};
    scene.triangles.push_back(triangle);
    slicer_core::TriangleTextureInfo texture;
    texture.material_name = "custom";
    scene.triangle_textures.push_back(texture);
    const slicer_core::AdaptedTriangleMesh mesh =
        slicer_core::AdaptSceneModelToTriangleMesh(scene);
    slicer_core::MaterialVolumeGrid grid;
    grid.widthPx = 1;
    grid.heightPx = 1;
    grid.layerCount = 1;
    grid.pixelSizeXMm = 1.0;
    grid.pixelSizeYMm = 1.0;
    grid.layerThicknessMm = 1.0;
    try
    {
        (void)slicer_core::BuildTransferMaterialVolumePlan(
            PolicyFor({10U, 20U, 30U}), mesh, grid);
    }
    catch (const slicer_core::TransferChannelError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::TransferChannelErrorCode::TopologyInvalid,
            "matched open transfer fails with stable T topology error");
    }
    return ExpectTrue(false, "matched open transfer must fail closed");
}

slicer_core::TransferChannelPolicyConfig WholeModelPolicy()
{
    slicer_core::TransferChannelPolicyConfig policy;
    policy.enabled = true;
    policy.match_source = "whole_model";
    policy.topology.self_intersection_policy = "tolerate_closed_self_intersection";
    policy.topology.max_self_intersection_pairs = 64;
    return policy;
}

slicer_core::AdaptedTriangleMesh GreenMaterialMesh()
{
    slicer_core::AdaptedTriangleMesh mesh;
    mesh.material_infos.push_back(
        slicer_core::MaterialInfo{"01", {63U, 190U, 126U}, true});
    return mesh;
}

slicer_core::MaterialVolumeGrid TwoByTwoGrid()
{
    slicer_core::MaterialVolumeGrid grid;
    grid.widthPx = 2;
    grid.heightPx = 2;
    grid.layerCount = 1;
    grid.pixelSizeXMm = 1.0;
    grid.pixelSizeYMm = 1.0;
    grid.layerThicknessMm = 1.0;
    return grid;
}

bool WholeModelTakesModelMaskVerbatim()
{
    const slicer_core::TransferMaterialVolumePlan plan =
        slicer_core::BuildTransferMaterialVolumePlan(
            WholeModelPolicy(), GreenMaterialMesh(), TwoByTwoGrid());

    // 先钉住被测分支确实触发：不然下面的逐像素比对可能整条空转。
    bool passed = ExpectTrue(
        plan.material.wholeModel, "whole_model policy resolves a whole-model match");
    passed = ExpectTrue(
                 plan.material.materialName
                     == slicer_core::kWholeModelTransferMaterialName,
                 "whole-model match reports the synthetic material name")
        && passed;
    // 整模的全部收益就在这一条：不建 volume，故不受流形与自交要求约束。
    passed = ExpectTrue(
                 !plan.volume.has_value(),
                 "whole-model plan skips volume solving entirely")
        && passed;
    passed = ExpectTrue(plan.HasRegion(), "whole-model plan still reports a region")
        && passed;

    // 故意用非全 1 的掩膜：全 1 时「逐像素照搬」与「无脑全写 1」无法区分。
    const std::vector<std::uint8_t> modelMask{1U, 0U, 1U, 1U};
    std::vector<std::uint32_t> owner(4U, 7U);
    std::vector<std::uint8_t> mask(4U, 9U);
    slicer_core::MaterializeTransferLayerMask(plan, 0, modelMask, owner, mask);
    passed = ExpectTrue(
                 mask == modelMask,
                 "whole-model mask copies the model mask pixel for pixel")
        && passed;
    return ExpectTrue(
               std::all_of(owner.begin(), owner.end(), [](const std::uint32_t value)
               {
                   return value == slicer_core::kNoMaterialOwner;
               }),
               "whole-model mask claims no material owner")
        && passed;
}

bool WholeModelCoversWhatColourMatchingMisses()
{
    // 同一网格、同一掩膜，只换 matchSource：颜色匹配落空，整模必须全覆盖。
    // 这一条证明整模分支确实改变了结果，而不是碰巧与既有行为一致。
    const slicer_core::AdaptedTriangleMesh mesh = GreenMaterialMesh();
    const slicer_core::MaterialVolumeGrid grid = TwoByTwoGrid();
    const std::vector<std::uint8_t> modelMask{1U, 0U, 1U, 1U};
    std::vector<std::uint32_t> owner(4U);

    const slicer_core::TransferMaterialVolumePlan colourPlan =
        slicer_core::BuildTransferMaterialVolumePlan(
            PolicyFor({255U, 220U, 198U}), mesh, grid);
    std::vector<std::uint8_t> colourMask(4U, 9U);
    slicer_core::MaterializeTransferLayerMask(
        colourPlan, 0, modelMask, owner, colourMask);

    const slicer_core::TransferMaterialVolumePlan wholePlan =
        slicer_core::BuildTransferMaterialVolumePlan(
            WholeModelPolicy(), mesh, grid);
    std::vector<std::uint8_t> wholeMask(4U, 9U);
    slicer_core::MaterializeTransferLayerMask(
        wholePlan, 0, modelMask, owner, wholeMask);

    return ExpectTrue(
               !colourPlan.HasRegion()
                   && std::all_of(
                       colourMask.begin(), colourMask.end(),
                       [](const std::uint8_t value) { return value == 0U; }),
               "unmatched colour policy yields an empty transfer mask")
        && ExpectTrue(
            wholeMask == modelMask && wholeMask != colourMask,
            "whole_model covers exactly what colour matching misses");
}

bool WholeModelWithColoursFailsClosed()
{
    // 整模不看材质表，配了颜色就是配置矛盾，必须在建 plan 时就拦掉。
    slicer_core::TransferChannelPolicyConfig policy = WholeModelPolicy();
    policy.material_diffuse_rgb_values.push_back({10U, 20U, 30U});
    try
    {
        (void)slicer_core::BuildTransferMaterialVolumePlan(
            policy, GreenMaterialMesh(), TwoByTwoGrid());
    }
    catch (const slicer_core::TransferChannelError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::TransferChannelErrorCode::ConfigInvalid,
            "whole_model with configured colours fails closed");
    }
    return ExpectTrue(false, "whole_model with configured colours must fail closed");
}

}  // namespace

int main()
{
    int failures{0};
    const auto run = [&failures](const bool passed, const char* name)
    {
        if (!passed)
        {
            std::cerr << "CASE FAILED " << name << '\n';
            ++failures;
        }
    };
    run(Reality03BuildsMaterial02OnlyPlan(), "reality_03_transfer_plan");
    run(MissingOptionalRegionMaterializesEmptyMask(), "missing_optional_empty_mask");
    run(OpenMatchedRegionFailsClosed(), "open_region_fail_closed");
    run(WholeModelTakesModelMaskVerbatim(), "whole_model_verbatim_mask");
    run(WholeModelCoversWhatColourMatchingMisses(),
        "whole_model_covers_colour_miss");
    run(WholeModelWithColoursFailsClosed(), "whole_model_colours_fail_closed");
    if (failures != 0)
    {
        std::cerr << "FAIL TransferMaterialVolumePlanTests " << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS TransferMaterialVolumePlanTests 6/6\n";
    return 0;
}
