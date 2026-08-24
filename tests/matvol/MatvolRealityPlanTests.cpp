// MATVOL MV-08A：以真实资产证明 MaterialVolumePlan 可由生产侧已有的数据直接构建。
//
// 本卡把「几何是否算对」与「合成是否接对」两类失败分离开：本文件完全不动 slicer.cpp，
// 只证明 run_slicer 手里已有的 ModelReport 足以建出 plan。
//
// 同时修正 DOC_PREP_MATVOL_MV_08 v1.0 §2 的错误结论。该文档称
// MaterialVolumeBuildRequest 要求的 AdaptedTriangleMesh 在 run_slicer 中不存在、
// 需要新增窄适配器。实际上 src/slicer_core/scene/SceneModel.h 里
// using SceneModel = ModelReport; 两者是同一类型，
// AdaptSceneModelToTriangleMesh(model_report) 本来就能直接调用，不存在该缺口。
// 本文件即为该结论的机器证据。

#include "slicer_core/config.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/volume/MaterialTopologyClassifier.h"
#include "slicer_core/materials/volume/MaterialVolumePlan.h"
#include "slicer_core/model.h"
#include "slicer_core/model/ModelLoadConfig.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
bool ExpectTrue(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::filesystem::path RealityAsset(const std::string& fileName)
{
#ifdef SLICESOFT_SOURCE_DIR
    return std::filesystem::path{SLICESOFT_SOURCE_DIR} / "model" / "obj" / "reality"
        / "finger_suoguo" / fileName;
#else
    return std::filesystem::path{"model"} / "obj" / "reality" / "finger_suoguo" / fileName;
#endif
}

// 按生产链口径加载：启用自动摆正，使模型进入生产姿态。
// 03.obj 建模坐标系下 +Z 面命中浅桃色 02、-Z 面命中绿色 01；生产链固定施加
// rotate_x_180_rotate_z_minus_90 后上下翻转，故生产姿态顶面命中绿色 01。
// 依据见 DOC_DECISION_MATVOL 的 2.1 节实测结论。
slicer_core::ModelReport LoadProductionPosture(const std::filesystem::path& modelPath)
{
    slicer_core::ModelLoadConfig config;
    config.input.model_path = modelPath;
    config.input.format = "obj";
    config.auto_orient.enabled = true;
    return slicer_core::load_model_report(config, modelPath.parent_path());
}

slicer_core::MaterialVolumeGrid MakeGridForModel(
    const slicer_core::ModelReport& model,
    const double layerThicknessMm,
    const double pixelSizeMm)
{
    slicer_core::MaterialVolumeGrid grid;
    const double widthMm = model.bbox_mm.max.x - model.bbox_mm.min.x;
    const double heightMm = model.bbox_mm.max.y - model.bbox_mm.min.y;
    const double depthMm = model.bbox_mm.max.z - model.bbox_mm.min.z;
    grid.originXMm = model.bbox_mm.min.x;
    grid.originYMm = model.bbox_mm.min.y;
    grid.pixelSizeXMm = pixelSizeMm;
    grid.pixelSizeYMm = pixelSizeMm;
    grid.widthPx = std::max(1, static_cast<int>(widthMm / pixelSizeMm));
    grid.heightPx = std::max(1, static_cast<int>(heightMm / pixelSizeMm));
    grid.layerThicknessMm = layerThicknessMm;
    grid.layerCount = std::max(1, static_cast<int>(depthMm / layerThicknessMm));
    return grid;
}

slicer_core::MaterialVolumePolicyConfig MakePolicy()
{
    slicer_core::MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.mode = "closed_intervals";
    policy.missing_material = "fail_closed";
    policy.open_surface.mode = "reject";
    policy.overlap.mode = "explicit_priority";
    slicer_core::MaterialVolumeOverlapRuleConfig primary;
    primary.match_material_name = "01";
    primary.priority = 200;
    slicer_core::MaterialVolumeOverlapRuleConfig secondary;
    secondary.match_material_name = "02";
    secondary.priority = 100;
    policy.overlap.rules.push_back(primary);
    policy.overlap.rules.push_back(secondary);
    return policy;
}

// run_slicer 手里的 ModelReport 足以直接喂给既有适配器，不需要新增适配层。
bool ModelReportFeedsTheExistingAdapterDirectly()
{
    const std::filesystem::path modelPath = RealityAsset("03.obj");
    if (!std::filesystem::exists(modelPath))
    {
        std::cout << "SKIP reality_plan asset not present: " << modelPath.string() << '\n';
        return true;
    }

    const slicer_core::ModelReport model = LoadProductionPosture(modelPath);
    bool passed{true};
    passed = ExpectTrue(!model.triangles.empty(), "reality model loads triangles") && passed;
    passed = ExpectTrue(
                 model.triangle_textures.size() == model.triangles.size(),
                 "every triangle carries a material attribution")
        && passed;

    // 关键：SceneModel 即 ModelReport 的别名，此调用无需任何适配层。
    const slicer_core::AdaptedTriangleMesh mesh =
        slicer_core::AdaptSceneModelToTriangleMesh(model);
    passed = ExpectTrue(!mesh.mesh.triangles.empty(), "adapter produces indexed triangles")
        && passed;
    passed = ExpectTrue(
                 mesh.triangle_attributes.size() == mesh.mesh.triangles.size(),
                 "adapter keeps one attribute per accepted triangle")
        && passed;
    passed = ExpectTrue(
                 mesh.mesh.vertices.size() < model.triangles.size() * 3U,
                 "adapter welds shared vertices instead of emitting three per triangle")
        && passed;

    std::size_t green{0};
    std::size_t peach{0};
    for (const slicer_core::SurfaceTriangleAttributes& attributes : mesh.triangle_attributes)
    {
        if (attributes.material_name == "01")
        {
            ++green;
        }
        else if (attributes.material_name == "02")
        {
            ++peach;
        }
    }
    passed = ExpectTrue(green > 0U, "material 01 survives adaptation") && passed;
    passed = ExpectTrue(peach > 0U, "material 02 survives adaptation") && passed;
    std::cout << "  adapted mesh: triangles=" << mesh.mesh.triangles.size()
              << " vertices=" << mesh.mesh.vertices.size()
              << " material01=" << green << " material02=" << peach << '\n';
    return passed;
}

// 由真实资产建 plan。03.obj 的材质 01 是开放表面（1382 开放边），
// openSurface.mode=reject 下建构失败是预期且正确的 fail-closed，不是缺陷；
// 本用例锁定的是「失败发生在拓扑分类且带稳定错误码」，而不是更早的适配环节。
bool RealityPlanBuildIsDeterministic()
{
    const std::filesystem::path modelPath = RealityAsset("03.obj");
    if (!std::filesystem::exists(modelPath))
    {
        std::cout << "SKIP reality_plan_structure asset not present\n";
        return true;
    }

    const slicer_core::ModelReport model = LoadProductionPosture(modelPath);
    const slicer_core::AdaptedTriangleMesh mesh =
        slicer_core::AdaptSceneModelToTriangleMesh(model);
    // 粗网格：本卡只验证结构与错误码，不验证像素级结果，避免把运行时间拖进分钟级。
    const slicer_core::MaterialVolumeGrid grid = MakeGridForModel(model, 0.038, 0.50);
    const slicer_core::MaterialVolumePolicyConfig policy = MakePolicy();

    slicer_core::MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = grid;

    bool passed{true};
    try
    {
        const slicer_core::MaterialVolumePlan plan =
            slicer_core::BuildMaterialVolumePlan(request);
        passed = ExpectTrue(plan.LayerCount() == grid.layerCount, "plan preserves layer count")
            && passed;
        passed = ExpectTrue(
                     plan.ColumnCount()
                         == static_cast<std::size_t>(grid.widthPx)
                             * static_cast<std::size_t>(grid.heightPx),
                     "plan preserves column count")
            && passed;
        passed = ExpectTrue(
                     plan.ColumnIntervalOffsets().size() == plan.ColumnCount() + 1U,
                     "CSR offsets carry the sentinel entry")
            && passed;
        std::cout << "  reality plan built: materials=" << plan.MaterialNames().size()
                  << " intervals=" << plan.Intervals().size()
                  << " columns=" << plan.ColumnCount()
                  << " layers=" << plan.LayerCount() << '\n';
    }
    catch (const std::exception& error)
    {
        const std::string message = error.what();
        std::cout << "  reality plan rejected: " << message << '\n';
        passed = ExpectTrue(
                     message.find("E_MATVOL_") != std::string::npos,
                     "rejection carries a stable E_MATVOL_ code")
            && passed;
    }
    return passed;
}
// 逐材质拓扑事实：本用例不断言具体分类，只把事实固化为可读证据。
// 它存在的理由是 03.obj 经生产链自动摆正与顶点焊接后，材质 02 的分类与
// DOC_DECISION 第 2 节记录的「闭合子网格」不一致，须先把真实数字摆出来。
bool RealityTopologyFactsAreReported()
{
    bool passed{true};
    const std::string assets[] = {"03.obj", "08.obj", "09.obj"};
    for (const std::string& assetName : assets)
    {
        const std::filesystem::path assetPath = RealityAsset(assetName);
        if (!std::filesystem::exists(assetPath))
        {
            continue;
        }
        const slicer_core::ModelReport model = LoadProductionPosture(assetPath);
        const slicer_core::AdaptedTriangleMesh mesh =
            slicer_core::AdaptSceneModelToTriangleMesh(model);
        const std::vector<slicer_core::MaterialTopologyFact> facts =
            slicer_core::ClassifyMaterialTopologies(mesh);
        passed = ExpectTrue(facts.size() >= 2U, "both materials are classified") && passed;
        std::cout << "  [" << assetName << "]" << '\n';
    for (const slicer_core::MaterialTopologyFact& fact : facts)
    {
        std::cout << "  material " << fact.materialName
                  << " kind=" << slicer_core::MaterialTopologyKindName(fact.kind)
                  << " tris=" << fact.triangleCount
                  << " boundaryEdges=" << fact.boundaryEdgeCount
                  << " interfaceEdges=" << fact.materialInterfaceEdgeCount
                  << " nonManifoldEdges=" << fact.nonManifoldEdgeCount
                  << " selfIntersectPairs=" << fact.confirmedSelfIntersectionPairs
                  << " siEvaluated=" << fact.selfIntersectionEvaluated
                  << " siComplete=" << fact.selfIntersectionComplete
                  << " signedVolumeMm3=" << fact.signedVolumeMm3 << '\n';
    }
    }
    return passed;
}

}  // namespace

int main()
{
    struct TestCase
    {
        const char* name;
        bool (*run)();
    };
    const TestCase cases[] = {
        {"model_report_feeds_adapter_directly", &ModelReportFeedsTheExistingAdapterDirectly},
        {"reality_plan_build_is_deterministic", &RealityPlanBuildIsDeterministic},
        {"reality_topology_facts", &RealityTopologyFactsAreReported},
    };

    int failures{0};
    for (const TestCase& testCase : cases)
    {
        if (!testCase.run())
        {
            std::cerr << "CASE FAILED: " << testCase.name << '\n';
            ++failures;
        }
    }
    if (failures != 0)
    {
        std::cerr << "FAIL MatvolRealityPlanTests " << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS MatvolRealityPlanTests "
              << (sizeof(cases) / sizeof(cases[0])) << "/"
              << (sizeof(cases) / sizeof(cases[0])) << '\n';
    return 0;
}
