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
    if (failures != 0)
    {
        std::cerr << "FAIL TransferMaterialVolumePlanTests " << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS TransferMaterialVolumePlanTests 3/3\n";
    return 0;
}
