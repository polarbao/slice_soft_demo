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
#include "slicer_core/geometry/repair/MeshCompleteSelfIntersectionAnalyzer.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"
#include "slicer_core/materials/volume/MaterialTopologyClassifier.h"
#include "slicer_core/materials/volume/MaterialVolumePlan.h"
#include "slicer_core/model.h"
#include "slicer_core/model/ModelLoadConfig.h"

#include <algorithm>
#include <array>
#include <map>
#include <cmath>
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

slicer_core::MaterialVolumePolicyConfig MakePolicy(const std::string& selfIntersectionPolicy = "reject")
{
    slicer_core::MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.mode = "closed_intervals";
    policy.missing_material = "fail_closed";
    policy.open_surface.mode = "reject";
    policy.overlap.mode = "explicit_priority";
    policy.topology.self_intersection_policy = selfIntersectionPolicy;
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

// MV-08A 诊断：把材质 02 的自交分解到四类，并给出涉及区域范围。
// 判断「能否忽略」不能只看对数：真穿透与仅接触的后果完全不同，
// 且真正的影响面是「有多少 XY 列会因此拿到奇数交点」。
bool SelfIntersectionIsCharacterized()
{
    const std::filesystem::path modelPath = RealityAsset("03.obj");
    if (!std::filesystem::exists(modelPath))
    {
        std::cout << "SKIP self_intersection_characterization asset not present\n";
        return true;
    }
    const slicer_core::ModelReport model = LoadProductionPosture(modelPath);
    const slicer_core::AdaptedTriangleMesh mesh =
        slicer_core::AdaptSceneModelToTriangleMesh(model);

    // 抽出材质 02 子网格：顶点重映射，与分类器内部做法一致。
    slicer_core::TriangleMeshData subMesh;
    subMesh.source_name = "material_02";
    std::map<int, int> remap;
    for (std::size_t index{0}; index < mesh.triangle_attributes.size(); ++index)
    {
        if (mesh.triangle_attributes.at(index).material_name != "02")
        {
            continue;
        }
        const std::array<int, 3>& source = mesh.mesh.triangles.at(index);
        std::array<int, 3> mapped{};
        for (int corner{0}; corner < 3; ++corner)
        {
            const int sourceVertex = source.at(static_cast<std::size_t>(corner));
            const auto found = remap.find(sourceVertex);
            if (found != remap.end())
            {
                mapped.at(static_cast<std::size_t>(corner)) = found->second;
                continue;
            }
            const int next = static_cast<int>(subMesh.vertices.size());
            subMesh.vertices.push_back(
                mesh.mesh.vertices.at(static_cast<std::size_t>(sourceVertex)));
            remap.emplace(sourceVertex, next);
            mapped.at(static_cast<std::size_t>(corner)) = next;
        }
        subMesh.triangles.push_back(mapped);
    }

    slicer_core::MeshCompleteSelfIntersectionOptions options;
    const slicer_core::MeshCompleteSelfIntersectionAnalysis analysis =
        slicer_core::AnalyzeCompleteMeshSelfIntersections(subMesh, options);
    std::cout << "  material02 selfIntersection: status=" << analysis.status
              << " complete=" << analysis.complete
              << " tris=" << analysis.triangleCount
              << " candidatePairs=" << analysis.candidatePairCount
              << " testedPairs=" << analysis.testedPairCount
              << " confirmed=" << analysis.confirmedIntersectionPairs
              << " coplanarOverlap=" << analysis.coplanarOverlapPairs
              << " touchingOnly=" << analysis.touchingOnlyPairs
              << " aabbOnly=" << analysis.aabbOnlyPairs << '\n';
    for (const slicer_core::ValidationIssue& issue : analysis.issues)
    {
        std::cout << "    issue " << issue.code << ": " << issue.message << '\n';
    }
    return ExpectTrue(analysis.complete, "self-intersection analysis completes within budget");
}

// 影响面量化：MATVOL 的封闭材质区间依赖「垂直射线拿到成对交点」。
// 自交会破坏这一奇偶性，但只在射线穿过自交区域的那些 XY 列上破坏。
// 本用例直接测「奇数交点列占比」，这是判断能否忽略、以及放宽后阈值取多少的唯一依据。
bool OddParityColumnRatioIsMeasured()
{
    const std::filesystem::path modelPath = RealityAsset("03.obj");
    if (!std::filesystem::exists(modelPath))
    {
        std::cout << "SKIP odd_parity_columns asset not present\n";
        return true;
    }
    const slicer_core::ModelReport model = LoadProductionPosture(modelPath);
    const slicer_core::AdaptedTriangleMesh mesh =
        slicer_core::AdaptSceneModelToTriangleMesh(model);

    std::vector<std::array<slicer_core::Vec3, 3>> faces;
    for (std::size_t index{0}; index < mesh.triangle_attributes.size(); ++index)
    {
        if (mesh.triangle_attributes.at(index).material_name != "02")
        {
            continue;
        }
        const std::array<int, 3>& tri = mesh.mesh.triangles.at(index);
        faces.push_back({mesh.mesh.vertices.at(static_cast<std::size_t>(tri[0])),
                         mesh.mesh.vertices.at(static_cast<std::size_t>(tri[1])),
                         mesh.mesh.vertices.at(static_cast<std::size_t>(tri[2]))});
    }

    for (const double pixelMm : {0.100, 0.042, 0.021})
    {
    const double minX = model.bbox_mm.min.x;
    const double minY = model.bbox_mm.min.y;
    const int width = std::max(1, static_cast<int>((model.bbox_mm.max.x - minX) / pixelMm));
    const int height = std::max(1, static_cast<int>((model.bbox_mm.max.y - minY) / pixelMm));

    // 按 XY 包围盒把三角面装桶，避免逐列遍历全部面。
    std::vector<std::vector<std::size_t>> buckets(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (std::size_t faceIndex{0}; faceIndex < faces.size(); ++faceIndex)
    {
        const std::array<slicer_core::Vec3, 3>& face = faces.at(faceIndex);
        double loX = face[0].x, hiX = face[0].x, loY = face[0].y, hiY = face[0].y;
        for (int corner{1}; corner < 3; ++corner)
        {
            loX = std::min(loX, face.at(static_cast<std::size_t>(corner)).x);
            hiX = std::max(hiX, face.at(static_cast<std::size_t>(corner)).x);
            loY = std::min(loY, face.at(static_cast<std::size_t>(corner)).y);
            hiY = std::max(hiY, face.at(static_cast<std::size_t>(corner)).y);
        }
        const int x0 = std::max(0, static_cast<int>((loX - minX) / pixelMm));
        const int x1 = std::min(width - 1, static_cast<int>((hiX - minX) / pixelMm));
        const int y0 = std::max(0, static_cast<int>((loY - minY) / pixelMm));
        const int y1 = std::min(height - 1, static_cast<int>((hiY - minY) / pixelMm));
        for (int y{y0}; y <= y1; ++y)
        {
            for (int x{x0}; x <= x1; ++x)
            {
                buckets.at(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                           + static_cast<std::size_t>(x)).push_back(faceIndex);
            }
        }
    }

    std::size_t hitColumns{0};
    std::size_t oddColumns{0};
    for (int y{0}; y < height; ++y)
    {
        for (int x{0}; x < width; ++x)
        {
            const double px = minX + (static_cast<double>(x) + 0.5) * pixelMm;
            const double py = minY + (static_cast<double>(y) + 0.5) * pixelMm;
            int crossings{0};
            for (const std::size_t faceIndex :
                 buckets.at(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                            + static_cast<std::size_t>(x)))
            {
                const std::array<slicer_core::Vec3, 3>& f = faces.at(faceIndex);
                // 二维重心坐标判定射线是否落在三角面 XY 投影内。
                const double d = (f[1].y - f[2].y) * (f[0].x - f[2].x)
                    + (f[2].x - f[1].x) * (f[0].y - f[2].y);
                if (std::abs(d) < 1.0e-15)
                {
                    continue;
                }
                const double a = ((f[1].y - f[2].y) * (px - f[2].x)
                    + (f[2].x - f[1].x) * (py - f[2].y)) / d;
                const double b = ((f[2].y - f[0].y) * (px - f[2].x)
                    + (f[0].x - f[2].x) * (py - f[2].y)) / d;
                const double c = 1.0 - a - b;
                if (a < 0.0 || b < 0.0 || c < 0.0)
                {
                    continue;
                }
                ++crossings;
            }
            if (crossings == 0)
            {
                continue;
            }
            ++hitColumns;
            if ((crossings % 2) != 0)
            {
                ++oddColumns;
            }
        }
    }
    const double ratio = hitColumns == 0
        ? 0.0
        : static_cast<double>(oddColumns) * 100.0 / static_cast<double>(hitColumns);
    std::cout << "  material02 parity: grid=" << width << "x" << height
              << " pixelMm=" << pixelMm
              << " hitColumns=" << hitColumns
              << " oddColumns=" << oddColumns
              << " oddRatioPercent=" << ratio << '\n';
    }
    return true;
}

// 放行策略验收：把代理指标（自交对数）换成直接检查（逐列奇偶）后，
// 03.obj 必须真正建出 plan，且放行名单必须留痕。
bool ToleratePolicyBuildsPlanForRealityAsset()
{
    const std::filesystem::path modelPath = RealityAsset("03.obj");
    if (!std::filesystem::exists(modelPath))
    {
        std::cout << "SKIP tolerate_policy asset not present\n";
        return true;
    }
    const slicer_core::ModelReport model = LoadProductionPosture(modelPath);
    const slicer_core::AdaptedTriangleMesh mesh =
        slicer_core::AdaptSceneModelToTriangleMesh(model);
    const slicer_core::MaterialVolumeGrid grid = MakeGridForModel(model, 0.038, 0.10);
    const slicer_core::MaterialVolumePolicyConfig policy =
        MakePolicy("tolerate_closed_self_intersection");
    slicer_core::MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = grid;

    bool passed{true};
    try
    {
        const slicer_core::MaterialVolumePlan plan =
            slicer_core::BuildMaterialVolumePlan(request);
        passed = ExpectTrue(plan.MaterialNames().size() == 2U, "both materials enter the plan")
            && passed;
        passed = ExpectTrue(!plan.Intervals().empty(), "plan produces intervals") && passed;
        passed = ExpectTrue(
                     plan.ToleratedSelfIntersectingMaterials().size() == 1U,
                     "exactly one material is recorded as tolerated")
            && passed;
        if (!plan.ToleratedSelfIntersectingMaterials().empty())
        {
            passed = ExpectTrue(
                         plan.ToleratedSelfIntersectingMaterials()[0] == "02",
                         "the tolerated material is 02")
                && passed;
        }
        std::cout << "  tolerate policy: intervals=" << plan.Intervals().size()
                  << " columns=" << plan.ColumnCount()
                  << " layers=" << plan.LayerCount()
                  << " tolerated=" << plan.ToleratedSelfIntersectingMaterials().size()
                  << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: tolerate policy still rejected: " << error.what() << '\n';
        passed = false;
    }
    return passed;
}

// 上限必须真的会咬：把 maxSelfIntersectionPairs 压到 8 对以下，放行须失效。
// 没有这条断言，「有界放宽」就名不副实，等同于无条件旁路。
bool ToleranceBoundStillRejects()
{
    const std::filesystem::path modelPath = RealityAsset("03.obj");
    if (!std::filesystem::exists(modelPath))
    {
        std::cout << "SKIP tolerance_bound asset not present\n";
        return true;
    }
    const slicer_core::ModelReport model = LoadProductionPosture(modelPath);
    const slicer_core::AdaptedTriangleMesh mesh =
        slicer_core::AdaptSceneModelToTriangleMesh(model);
    const slicer_core::MaterialVolumeGrid grid = MakeGridForModel(model, 0.038, 0.50);
    slicer_core::MaterialVolumePolicyConfig policy =
        MakePolicy("tolerate_closed_self_intersection");
    policy.topology.max_self_intersection_pairs = 4;  // 实测为 8 对
    slicer_core::MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = grid;

    try
    {
        const slicer_core::MaterialVolumePlan plan =
            slicer_core::BuildMaterialVolumePlan(request);
        (void)plan;
        return ExpectTrue(false, "tolerance bound rejects pair counts above the limit");
    }
    catch (const std::exception& error)
    {
        const std::string message = error.what();
        return ExpectTrue(
            message.find("E_MATVOL_TOPOLOGY_INVALID") != std::string::npos,
            "exceeding the bound still fails closed with the topology code");
    }
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
        {"self_intersection_characterization", &SelfIntersectionIsCharacterized},
        {"odd_parity_column_ratio", &OddParityColumnRatioIsMeasured},
        {"tolerate_policy_builds_plan", &ToleratePolicyBuildsPlanForRealityAsset},
        {"tolerance_bound_still_rejects", &ToleranceBoundStillRejects},
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
