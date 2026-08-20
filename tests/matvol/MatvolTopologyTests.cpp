// MATVOL MV-02：MaterialVolumePolicy 配置合同、拓扑分类与稳定错误码。
//
// 本测试覆盖任务卡 §5 的全部 fail-closed 验收项：未知枚举、Global/S3/S4 组合、
// 开放表面缺策略、重复规则、材质缺失，以及旧配置缺字段时行为不变。

#include "slicer_core/config.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/volume/MaterialTopologyClassifier.h"
#include "slicer_core/materials/volume/MaterialVolumeError.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

using slicer_core::AdaptedTriangleMesh;
using slicer_core::MaterialTopologyFact;
using slicer_core::MaterialTopologyKind;
using slicer_core::MaterialTopologyKindName;
using slicer_core::MaterialVolumeError;
using slicer_core::MaterialVolumeErrorCode;
using slicer_core::MaterialVolumeErrorCodeName;
using slicer_core::SliceConfig;
using slicer_core::SurfaceTriangleAttributes;
using slicer_core::Vec3;

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

/// @brief 断言校验按预期抛出，且消息包含指定片段。
bool ExpectValidationRejects(SliceConfig config, const std::string& fragment, const std::string& message)
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

// ---------------------------------------------------------------------------
// 网格构造：直接构造 AdaptedTriangleMesh，避免依赖 SceneModel 与自动定向
// ---------------------------------------------------------------------------

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

int AppendVertex(AdaptedTriangleMesh& mesh, const double x, const double y, const double z)
{
    const int index = static_cast<int>(mesh.mesh.vertices.size());
    mesh.mesh.vertices.push_back(Vec3{x, y, z});
    return index;
}

/// @brief 追加闭合盒体（12 面），顶点按需新建，不与其他材质共享。
void AppendClosedBox(
    AdaptedTriangleMesh& mesh,
    const std::string& materialName,
    const double loZ,
    const double hiZ)
{
    const int v000 = AppendVertex(mesh, 0.0, 0.0, loZ);
    const int v100 = AppendVertex(mesh, 1.0, 0.0, loZ);
    const int v110 = AppendVertex(mesh, 1.0, 1.0, loZ);
    const int v010 = AppendVertex(mesh, 0.0, 1.0, loZ);
    const int v001 = AppendVertex(mesh, 0.0, 0.0, hiZ);
    const int v101 = AppendVertex(mesh, 1.0, 0.0, hiZ);
    const int v111 = AppendVertex(mesh, 1.0, 1.0, hiZ);
    const int v011 = AppendVertex(mesh, 0.0, 1.0, hiZ);

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

const MaterialTopologyFact* FindFact(
    const std::vector<MaterialTopologyFact>& facts,
    const std::string& name)
{
    for (const MaterialTopologyFact& fact : facts)
    {
        if (fact.materialName == name)
        {
            return &fact;
        }
    }
    return nullptr;
}

/// @brief 构造通过前置校验的最小配置；modelPath 为 validate_slice_config 的硬前置。
SliceConfig MakeMinimalValidConfig()
{
    SliceConfig config;
    config.input.model_path = "model.obj";
    return config;
}

SliceConfig MakeEnabledPolicyConfig()
{
    SliceConfig config = MakeMinimalValidConfig();
    config.slicing_mode = "relief_heightfield";
    config.material_volume_policy.enabled = true;
    return config;
}

// ---------------------------------------------------------------------------
// 用例
// ---------------------------------------------------------------------------

/// @brief 稳定错误码名称与 DEV_MATVOL §8 冻结清单逐条一致，what() 带码前缀。
bool StableErrorCodesAreFrozen()
{
    bool passed{true};
    const std::array<std::pair<MaterialVolumeErrorCode, const char*>, 9> expected{{
        {MaterialVolumeErrorCode::UnsupportedPipeline, "E_MATVOL_UNSUPPORTED_PIPELINE"},
        {MaterialVolumeErrorCode::MaterialMissing, "E_MATVOL_MATERIAL_MISSING"},
        {MaterialVolumeErrorCode::OpenSurfaceRequiresPolicy, "E_MATVOL_OPEN_SURFACE_REQUIRES_POLICY"},
        {MaterialVolumeErrorCode::TopologyInvalid, "E_MATVOL_TOPOLOGY_INVALID"},
        {MaterialVolumeErrorCode::IntersectionUnpaired, "E_MATVOL_INTERSECTION_UNPAIRED"},
        {MaterialVolumeErrorCode::OverlapUnresolved, "E_MATVOL_OVERLAP_UNRESOLVED"},
        {MaterialVolumeErrorCode::ModelPixelUnowned, "E_MATVOL_MODEL_PIXEL_UNOWNED"},
        {MaterialVolumeErrorCode::ReplayMismatch, "E_MATVOL_REPLAY_MISMATCH"},
        {MaterialVolumeErrorCode::BudgetExceeded, "E_MATVOL_BUDGET_EXCEEDED"},
    }};
    for (const auto& [code, name] : expected)
    {
        passed = ExpectTrue(MaterialVolumeErrorCodeName(code) == name, "stable error code name is frozen")
            && passed;
    }

    const MaterialVolumeError error{MaterialVolumeErrorCode::TopologyInvalid, "sample detail"};
    const std::string what{error.what()};
    passed = ExpectTrue(
                 what == "E_MATVOL_TOPOLOGY_INVALID: sample detail",
                 "error what() carries the stable code prefix")
        && passed;
    passed = ExpectTrue(
                 error.Code() == MaterialVolumeErrorCode::TopologyInvalid,
                 "error preserves its code")
        && passed;
    return passed;
}

/// @brief 旧配置缺 materialVolumePolicy 时行为完全不变，且默认为关闭。
bool AbsentPolicyKeepsLegacyBehaviour()
{
    SliceConfig config = MakeMinimalValidConfig();
    bool passed{true};
    passed = ExpectTrue(!config.material_volume_policy.enabled, "policy defaults to disabled") && passed;
    passed = ExpectTrue(
                 config.material_volume_policy.mode == "closed_intervals",
                 "policy mode defaults to closed_intervals")
        && passed;
    passed = ExpectTrue(
                 config.material_volume_policy.open_surface.mode == "reject",
                 "open surface defaults to reject")
        && passed;
    passed = ExpectTrue(
                 config.material_volume_policy.open_surface.thickness_mm == 0.0,
                 "open surface thickness defaults to zero")
        && passed;
    passed = ExpectTrue(
                 config.material_volume_policy.missing_material == "fail_closed",
                 "missing material defaults to fail_closed")
        && passed;
    passed = ExpectTrue(
                 config.material_volume_policy.overlap.rules.empty(),
                 "overlap rules default to empty")
        && passed;

    // 关闭时即便字段取值非法也不得阻断旧 Profile。
    config.material_volume_policy.mode = "not_a_mode";
    config.material_volume_policy.open_surface.mode = "not_a_mode";
    try
    {
        slicer_core::validate_slice_config(config);
    }
    catch (const std::exception& error)
    {
        passed = ExpectTrue(false, std::string{"disabled policy must not block legacy config: "} + error.what())
            && passed;
    }
    return passed;
}

/// @brief 未知枚举、非法厚度、重复规则与空规则名全部 fail closed。
bool UnknownEnumAndRuleShapeFailClosed()
{
    bool passed{true};

    SliceConfig unknownMode = MakeEnabledPolicyConfig();
    unknownMode.material_volume_policy.mode = "voxel_soup";
    passed = ExpectValidationRejects(unknownMode, "mode must be closed_intervals", "unknown mode rejected")
        && passed;

    SliceConfig unknownMissing = MakeEnabledPolicyConfig();
    unknownMissing.material_volume_policy.missing_material = "inherit_neighbour";
    passed = ExpectValidationRejects(
                 unknownMissing, "missingMaterial must be fail_closed", "unknown missingMaterial rejected")
        && passed;

    SliceConfig unknownOpen = MakeEnabledPolicyConfig();
    unknownOpen.material_volume_policy.open_surface.mode = "extrude";
    passed = ExpectValidationRejects(
                 unknownOpen, "openSurface.mode must be reject or surface_band", "unknown openSurface mode rejected")
        && passed;

    SliceConfig unknownPlacement = MakeEnabledPolicyConfig();
    unknownPlacement.material_volume_policy.open_surface.placement = "above_surface";
    passed = ExpectValidationRejects(
                 unknownPlacement, "placement must be below_surface", "unknown placement rejected")
        && passed;

    SliceConfig unknownOverlap = MakeEnabledPolicyConfig();
    unknownOverlap.material_volume_policy.overlap.mode = "first_wins";
    passed = ExpectValidationRejects(
                 unknownOverlap, "overlap.mode must be explicit_priority", "unknown overlap mode rejected")
        && passed;

    SliceConfig emptyRule = MakeEnabledPolicyConfig();
    emptyRule.material_volume_policy.overlap.rules.push_back({"", 10});
    passed = ExpectValidationRejects(
                 emptyRule, "matchMaterialName must not be empty", "empty rule name rejected")
        && passed;

    SliceConfig duplicateRule = MakeEnabledPolicyConfig();
    duplicateRule.material_volume_policy.overlap.rules.push_back({"01", 200});
    duplicateRule.material_volume_policy.overlap.rules.push_back({"01", 100});
    passed = ExpectValidationRejects(
                 duplicateRule, "matchMaterialName must be unique", "duplicate rule rejected")
        && passed;

    return passed;
}

/// @brief 开放表面候选必须携带显式正厚度，负厚度一律阻断。
bool OpenSurfaceRequiresExplicitThickness()
{
    bool passed{true};

    SliceConfig missingThickness = MakeEnabledPolicyConfig();
    missingThickness.material_volume_policy.open_surface.mode = "surface_band";
    passed = ExpectValidationRejects(
                 missingThickness,
                 "surface_band requires positive thicknessMm",
                 "surface_band without thickness rejected")
        && passed;

    SliceConfig negativeThickness = MakeEnabledPolicyConfig();
    negativeThickness.material_volume_policy.open_surface.thickness_mm = -0.1;
    passed = ExpectValidationRejects(
                 negativeThickness, "thicknessMm must be non-negative", "negative thickness rejected")
        && passed;

    SliceConfig valid = MakeEnabledPolicyConfig();
    valid.material_volume_policy.open_surface.mode = "surface_band";
    valid.material_volume_policy.open_surface.thickness_mm = 0.2;
    try
    {
        slicer_core::validate_slice_config(valid);
    }
    catch (const std::exception& error)
    {
        passed = ExpectTrue(false, std::string{"explicit surface_band must pass: "} + error.what()) && passed;
    }
    return passed;
}

/// @brief 首批只放行 Legacy + relief_heightfield + S0，且与旧材质路径互斥。
bool UnsupportedPipelineCombinationsFailClosed()
{
    bool passed{true};

    SliceConfig scanline = MakeEnabledPolicyConfig();
    scanline.slicing_mode = "closed_mesh_scanline";
    passed = ExpectValidationRejects(
                 scanline, "requires slicingMode=relief_heightfield", "non-relief slicing mode rejected")
        && passed;

    SliceConfig sampling = MakeEnabledPolicyConfig();
    sampling.geometry_sampling.strategy = "layer_slab_supersample_2x2_at_least_two_candidate";
    passed = ExpectValidationRejects(
                 sampling, "requires geometrySampling.strategy=legacy_center_sample", "S3/S4 sampling rejected")
        && passed;

    SliceConfig withMaterialPolicy = MakeEnabledPolicyConfig();
    withMaterialPolicy.material_policy.enabled = true;
    passed = ExpectValidationRejects(
                 withMaterialPolicy, "does not support materialPolicy.enabled=true", "materialPolicy combination rejected")
        && passed;

    SliceConfig withRoleMapping = MakeEnabledPolicyConfig();
    withRoleMapping.material_role_mapping.enabled = true;
    withRoleMapping.material_role_mapping.rules.push_back({"01", "rgb"});
    passed = ExpectValidationRejects(
                 withRoleMapping, "does not support materialRoleMapping.enabled=true", "roleMapping combination rejected")
        && passed;

    return passed;
}

/// @brief 两个不共享顶点的闭合盒体各自判为 ClosedOrientable。
bool ClosedSubmeshesAreClassifiedClosed()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "01", 0.0, 1.0);
    AppendClosedBox(mesh, "02", 2.0, 3.0);
    const std::vector<MaterialTopologyFact> facts = slicer_core::ClassifyMaterialTopologies(mesh);

    bool passed{true};
    passed = ExpectTrue(facts.size() == 2U, "two material groups are reported") && passed;
    for (const std::string& name : {std::string{"01"}, std::string{"02"}})
    {
        const MaterialTopologyFact* fact = FindFact(facts, name);
        passed = ExpectTrue(fact != nullptr, "material group is present") && passed;
        if (fact == nullptr)
        {
            continue;
        }
        passed = ExpectTrue(fact->triangleCount == 12U, "closed box reports twelve triangles") && passed;
        passed = ExpectTrue(fact->boundaryEdgeCount == 0U, "closed box has no boundary edge") && passed;
        passed = ExpectTrue(fact->nonManifoldEdgeCount == 0U, "closed box has no non-manifold edge")
            && passed;
        passed = ExpectTrue(
                     fact->kind == MaterialTopologyKind::ClosedOrientable,
                     "closed box is classified ClosedOrientable")
            && passed;
    }
    passed = ExpectTrue(
                 MaterialTopologyKindName(MaterialTopologyKind::ClosedOrientable) == "closed_orientable",
                 "topology kind name is stable")
        && passed;
    return passed;
}

/// @brief 开放表面被判为 OpenSurface，且真开边计数非零。
bool OpenSubmeshIsClassifiedOpen()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 1.0);
    const int p00 = AppendVertex(mesh, 0.0, 0.0, 2.0);
    const int p10 = AppendVertex(mesh, 1.0, 0.0, 2.0);
    const int p11 = AppendVertex(mesh, 1.0, 1.0, 2.0);
    const int p01 = AppendVertex(mesh, 0.0, 1.0, 2.0);
    AppendTriangle(mesh, "01", p00, p10, p11);
    AppendTriangle(mesh, "01", p00, p11, p01);

    const std::vector<MaterialTopologyFact> facts = slicer_core::ClassifyMaterialTopologies(mesh);
    const MaterialTopologyFact* open = FindFact(facts, "01");
    const MaterialTopologyFact* closed = FindFact(facts, "02");

    bool passed{true};
    passed = ExpectTrue(open != nullptr && closed != nullptr, "both material groups are present") && passed;
    if (open != nullptr)
    {
        passed = ExpectTrue(open->boundaryEdgeCount == 4U, "open sheet reports four true boundary edges")
            && passed;
        passed = ExpectTrue(
                     open->materialInterfaceEdgeCount == 0U,
                     "isolated open sheet has no material interface edge")
            && passed;
        passed = ExpectTrue(
                     open->kind == MaterialTopologyKind::OpenSurface,
                     "open sheet is classified OpenSurface")
            && passed;
    }
    if (closed != nullptr)
    {
        passed = ExpectTrue(
                     closed->kind == MaterialTopologyKind::ClosedOrientable,
                     "neighbouring closed body keeps its classification")
            && passed;
    }
    return passed;
}

/// @brief 单个闭合立方体按材质切分（共享顶点、交界面不重复）：
///        交界边必须计入 materialInterfaceEdgeCount 而非 boundaryEdgeCount，
///        且两个子网格都不得被判为独立闭合体。
bool MaterialInterfaceEdgesAreCountedSeparately()
{
    AdaptedTriangleMesh mesh;
    const int v000 = AppendVertex(mesh, 0.0, 0.0, 0.0);
    const int v100 = AppendVertex(mesh, 1.0, 0.0, 0.0);
    const int v110 = AppendVertex(mesh, 1.0, 1.0, 0.0);
    const int v010 = AppendVertex(mesh, 0.0, 1.0, 0.0);
    const int v001 = AppendVertex(mesh, 0.0, 0.0, 1.0);
    const int v101 = AppendVertex(mesh, 1.0, 0.0, 1.0);
    const int v111 = AppendVertex(mesh, 1.0, 1.0, 1.0);
    const int v011 = AppendVertex(mesh, 0.0, 1.0, 1.0);

    // 材质 01：底面 + 四个侧面（10 面）
    AppendTriangle(mesh, "01", v000, v110, v100);
    AppendTriangle(mesh, "01", v000, v010, v110);
    AppendTriangle(mesh, "01", v000, v100, v101);
    AppendTriangle(mesh, "01", v000, v101, v001);
    AppendTriangle(mesh, "01", v100, v110, v111);
    AppendTriangle(mesh, "01", v100, v111, v101);
    AppendTriangle(mesh, "01", v110, v010, v011);
    AppendTriangle(mesh, "01", v110, v011, v111);
    AppendTriangle(mesh, "01", v010, v000, v001);
    AppendTriangle(mesh, "01", v010, v001, v011);
    // 材质 02：仅顶面（2 面），与 01 共享顶点，交界面不重复
    AppendTriangle(mesh, "02", v001, v101, v111);
    AppendTriangle(mesh, "02", v001, v111, v011);

    const std::vector<MaterialTopologyFact> facts = slicer_core::ClassifyMaterialTopologies(mesh);
    const MaterialTopologyFact* lower = FindFact(facts, "01");
    const MaterialTopologyFact* upper = FindFact(facts, "02");

    bool passed{true};
    passed = ExpectTrue(lower != nullptr && upper != nullptr, "both welded groups are present") && passed;
    if (upper != nullptr)
    {
        passed = ExpectTrue(upper->triangleCount == 2U, "top material owns two triangles") && passed;
        passed = ExpectTrue(
                     upper->boundaryEdgeCount == 0U,
                     "material interface edges are not counted as true boundary edges")
            && passed;
        passed = ExpectTrue(
                     upper->materialInterfaceEdgeCount == 4U,
                     "top material reports exactly four material interface edges")
            && passed;
        passed = ExpectTrue(
                     upper->kind == MaterialTopologyKind::OpenSurface,
                     "interface-bounded submesh is not treated as independently closed")
            && passed;
    }
    if (lower != nullptr)
    {
        passed = ExpectTrue(lower->triangleCount == 10U, "bottom material owns ten triangles") && passed;
        passed = ExpectTrue(
                     lower->boundaryEdgeCount == 0U,
                     "bottom material has no true boundary edge")
            && passed;
        passed = ExpectTrue(
                     lower->materialInterfaceEdgeCount == 4U,
                     "bottom material reports exactly four material interface edges")
            && passed;
    }
    return passed;
}

}  // namespace

int main()
{
    const std::array<std::pair<const char*, std::function<bool()>>, 8> tests{{
        {"stable_error_codes_frozen", StableErrorCodesAreFrozen},
        {"absent_policy_keeps_legacy_behaviour", AbsentPolicyKeepsLegacyBehaviour},
        {"unknown_enum_and_rule_shape_fail_closed", UnknownEnumAndRuleShapeFailClosed},
        {"open_surface_requires_explicit_thickness", OpenSurfaceRequiresExplicitThickness},
        {"unsupported_pipeline_combinations_fail_closed", UnsupportedPipelineCombinationsFailClosed},
        {"closed_submeshes_classified_closed", ClosedSubmeshesAreClassifiedClosed},
        {"open_submesh_classified_open", OpenSubmeshIsClassifiedOpen},
        {"material_interface_edges_counted_separately", MaterialInterfaceEdgesAreCountedSeparately},
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
        std::cout << "PASS MatvolTopologyTests " << tests.size() << "/" << tests.size() << '\n';
        return 0;
    }
    std::cerr << "FAIL MatvolTopologyTests " << failed << " failed\n";
    return 1;
}
