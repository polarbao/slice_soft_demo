// MATVOL MV-03：封闭材质有序交点、compact 层区间与 caller-owned 单层 owner 物化。
//
// 验收覆盖：MV-F01/F02/F04 全层 owner 与独立 oracle 差异为 0；空洞不被包络填平；
// 奇数交点阻断；同级重叠阻断；热路径零分配与 buffer 地址复用；取消不留半成品。

#include "slicer_core/materials/volume/MaterialVolumeError.h"
#include "slicer_core/materials/volume/MaterialVolumePlan.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using slicer_core::AdaptedTriangleMesh;
using slicer_core::BuildMaterialVolumePlan;
using slicer_core::kNoMaterialOwner;
using slicer_core::MaterialVolumeBuildRequest;
using slicer_core::MaterialVolumeError;
using slicer_core::MaterialVolumeErrorCode;
using slicer_core::MaterialVolumeGrid;
using slicer_core::MaterialVolumePlan;
using slicer_core::MaterialVolumePolicyConfig;
using slicer_core::MaterializeMaterialOwnershipLayer;
using slicer_core::SurfaceTriangleAttributes;
using slicer_core::Vec3;

static_assert(!std::is_copy_constructible_v<MaterialVolumePlan>);
static_assert(!std::is_copy_assignable_v<MaterialVolumePlan>);
static_assert(std::is_move_constructible_v<MaterialVolumePlan>);

std::atomic<bool> gTrackAllocations{false};
std::atomic<std::size_t> gAllocationCount{0U};

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

/// @brief 追加占满 [0,4]x[0,4] 的闭合盒体，顶点独立不与其他材质焊接。
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
    grid.originXMm = 0.0;
    grid.originYMm = 0.0;
    grid.pixelSizeXMm = 1.0;
    grid.pixelSizeYMm = 1.0;
    grid.layerThicknessMm = 1.0;
    grid.layerCount = 8;
    return grid;
}

MaterialVolumePolicyConfig MakePolicy(const int priority01, const int priority02)
{
    MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.overlap.rules.push_back({"01", priority01});
    policy.overlap.rules.push_back({"02", priority02});
    return policy;
}

std::size_t ColumnCountOf(const MaterialVolumeGrid& grid)
{
    return static_cast<std::size_t>(grid.widthPx) * static_cast<std::size_t>(grid.heightPx);
}

/// @brief 独立 oracle：稠密逐层 owner，仅用于小网格 expected。
std::vector<std::uint32_t> BuildDenseOwnerOracle(
    const MaterialVolumePlan& plan,
    const MaterialVolumeGrid& grid,
    const std::vector<std::uint8_t>& modelMask)
{
    const std::size_t columnCount = ColumnCountOf(grid);
    std::vector<std::uint32_t> dense(
        static_cast<std::size_t>(grid.layerCount) * columnCount, kNoMaterialOwner);
    for (int layer{0}; layer < grid.layerCount; ++layer)
    {
        for (std::size_t column{0}; column < columnCount; ++column)
        {
            if (modelMask.at(column) == 0U)
            {
                continue;
            }
            bool hasOwner{false};
            int bestPriority{0};
            std::uint32_t owner{kNoMaterialOwner};
            for (const auto& interval : plan.ColumnIntervals(column))
            {
                if (layer < interval.firstLayerInclusive || layer > interval.lastLayerInclusive)
                {
                    continue;
                }
                const int priority = plan.MaterialPriorities()[interval.materialIndex];
                if (!hasOwner || priority > bestPriority)
                {
                    hasOwner = true;
                    bestPriority = priority;
                    owner = interval.materialIndex;
                }
            }
            dense.at(static_cast<std::size_t>(layer) * columnCount + column) = owner;
        }
    }
    return dense;
}

}  // namespace

void* operator new(std::size_t size)
{
    if (gTrackAllocations.load(std::memory_order_relaxed))
    {
        gAllocationCount.fetch_add(1U, std::memory_order_relaxed);
    }
    void* memory = std::malloc(size == 0U ? 1U : size);
    if (memory == nullptr)
    {
        throw std::bad_alloc{};
    }
    return memory;
}

void* operator new[](std::size_t size)
{
    return operator new(size);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace
{

/// @brief MV-F01：Z 向不重叠的两个闭合体，逐层 owner 可手算。
bool StackedClosedBodiesProduceHandCheckableOwners()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 2.0);
    AppendClosedBox(mesh, "01", 3.0, 5.0);
    const MaterialVolumeGrid grid = MakeGrid();
    const MaterialVolumePolicyConfig policy = MakePolicy(200, 100);

    MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = grid;
    const MaterialVolumePlan plan = BuildMaterialVolumePlan(request);

    bool passed{true};
    passed = ExpectTrue(plan.MaterialNames().size() == 2U, "plan reports two materials") && passed;
    passed = ExpectTrue(plan.LayerCount() == grid.layerCount, "plan preserves layer count") && passed;
    passed = ExpectTrue(plan.ColumnCount() == ColumnCountOf(grid), "plan preserves column count")
        && passed;

    // 每列应恰好两段区间：z=[0,2] -> 层 0..1；z=[3,5] -> 层 3..4。
    std::uint32_t material01{kNoMaterialOwner};
    std::uint32_t material02{kNoMaterialOwner};
    for (std::size_t index{0}; index < plan.MaterialNames().size(); ++index)
    {
        if (plan.MaterialNames()[index] == "01")
        {
            material01 = static_cast<std::uint32_t>(index);
        }
        else if (plan.MaterialNames()[index] == "02")
        {
            material02 = static_cast<std::uint32_t>(index);
        }
    }
    passed = ExpectTrue(
                 material01 != kNoMaterialOwner && material02 != kNoMaterialOwner,
                 "both material names are resolvable")
        && passed;

    for (std::size_t column{0}; column < plan.ColumnCount(); ++column)
    {
        const auto intervals = plan.ColumnIntervals(column);
        passed = ExpectTrue(intervals.size() == 2U, "each column carries exactly two intervals")
            && passed;
        if (intervals.size() != 2U)
        {
            break;
        }
        passed = ExpectTrue(
                     intervals[0].firstLayerInclusive == 0 && intervals[0].lastLayerInclusive == 1
                         && intervals[0].materialIndex == material02,
                     "lower interval spans layers 0..1 and belongs to material 02")
            && passed;
        passed = ExpectTrue(
                     intervals[1].firstLayerInclusive == 3 && intervals[1].lastLayerInclusive == 4
                         && intervals[1].materialIndex == material01,
                     "upper interval spans layers 3..4 and belongs to material 01")
            && passed;
    }

    const std::vector<std::uint8_t> modelMask(ColumnCountOf(grid), 1U);
    std::vector<std::uint32_t> owner(ColumnCountOf(grid), 0U);
    const std::array<std::uint32_t, 8> expected{
        material02, material02, kNoMaterialOwner, material01, material01,
        kNoMaterialOwner, kNoMaterialOwner, kNoMaterialOwner};
    for (int layer{0}; layer < grid.layerCount; ++layer)
    {
        MaterializeMaterialOwnershipLayer(plan, layer, modelMask, owner);
        for (const std::uint32_t value : owner)
        {
            if (value != expected.at(static_cast<std::size_t>(layer)))
            {
                passed = ExpectTrue(false, "materialized owner matches hand-computed expectation")
                    && passed;
                break;
            }
        }
    }
    return passed;
}

/// @brief MV-F02：Z 向重叠必须由显式优先级裁决，且与材质声明顺序无关。
bool OverlappingBodiesResolveByPriority()
{
    bool passed{true};
    for (int variant{0}; variant < 2; ++variant)
    {
        AdaptedTriangleMesh mesh;
        if (variant == 0)
        {
            AppendClosedBox(mesh, "02", 0.0, 3.0);
            AppendClosedBox(mesh, "01", 2.0, 5.0);
        }
        else
        {
            AppendClosedBox(mesh, "01", 2.0, 5.0);
            AppendClosedBox(mesh, "02", 0.0, 3.0);
        }
        const MaterialVolumeGrid grid = MakeGrid();
        const MaterialVolumePolicyConfig policy = MakePolicy(200, 100);

        MaterialVolumeBuildRequest request;
        request.mesh = &mesh;
        request.policy = &policy;
        request.grid = grid;
        const MaterialVolumePlan plan = BuildMaterialVolumePlan(request);

        std::uint32_t material01{kNoMaterialOwner};
        for (std::size_t index{0}; index < plan.MaterialNames().size(); ++index)
        {
            if (plan.MaterialNames()[index] == "01")
            {
                material01 = static_cast<std::uint32_t>(index);
            }
        }

        const std::vector<std::uint8_t> modelMask(ColumnCountOf(grid), 1U);
        std::vector<std::uint32_t> owner(ColumnCountOf(grid), 0U);
        // 层 2 同时被 z=[0,3] 与 z=[2,5] 覆盖，priority 200 的材质 01 必须胜出。
        MaterializeMaterialOwnershipLayer(plan, 2, modelMask, owner);
        bool allWinner{true};
        for (const std::uint32_t value : owner)
        {
            if (value != material01)
            {
                allWinner = false;
                break;
            }
        }
        passed = ExpectTrue(
                     allWinner,
                     "overlapped layer is owned by the higher priority material regardless of order")
            && passed;
    }
    return passed;
}

/// @brief MV-F04：分离实体必须保留为多段区间，空洞层不得被 first/last 包络填平。
bool SeparatedBodiesKeepCavityUnowned()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 2.0);
    AppendClosedBox(mesh, "02", 4.0, 6.0);
    const MaterialVolumeGrid grid = MakeGrid();
    MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.overlap.rules.push_back({"02", 100});

    MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = grid;
    const MaterialVolumePlan plan = BuildMaterialVolumePlan(request);

    bool passed{true};
    for (std::size_t column{0}; column < plan.ColumnCount(); ++column)
    {
        const auto intervals = plan.ColumnIntervals(column);
        passed = ExpectTrue(intervals.size() == 2U, "cavity column keeps two separate intervals")
            && passed;
        if (intervals.size() != 2U)
        {
            break;
        }
        passed = ExpectTrue(
                     intervals[0].firstLayerInclusive == 0 && intervals[0].lastLayerInclusive == 1,
                     "lower body spans layers 0..1")
            && passed;
        passed = ExpectTrue(
                     intervals[1].firstLayerInclusive == 4 && intervals[1].lastLayerInclusive == 5,
                     "upper body spans layers 4..5")
            && passed;
    }

    const std::vector<std::uint8_t> modelMask(ColumnCountOf(grid), 1U);
    std::vector<std::uint32_t> owner(ColumnCountOf(grid), 0U);
    for (const int cavityLayer : {2, 3})
    {
        MaterializeMaterialOwnershipLayer(plan, cavityLayer, modelMask, owner);
        for (const std::uint32_t value : owner)
        {
            if (value != kNoMaterialOwner)
            {
                passed = ExpectTrue(false, "cavity layer stays unowned") && passed;
                break;
            }
        }
    }
    return passed;
}

/// @brief 物化结果必须与独立稠密 oracle 逐层逐列零差异。
bool MaterializationMatchesDenseOracle()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 3.0);
    AppendClosedBox(mesh, "01", 2.0, 5.0);
    const MaterialVolumeGrid grid = MakeGrid();
    const MaterialVolumePolicyConfig policy = MakePolicy(200, 100);

    MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = grid;
    const MaterialVolumePlan plan = BuildMaterialVolumePlan(request);

    const std::size_t columnCount = ColumnCountOf(grid);
    std::vector<std::uint8_t> modelMask(columnCount, 1U);
    // 掺入若干空洞像素，验证 modelMask 屏蔽生效。
    modelMask.at(0U) = 0U;
    modelMask.at(columnCount - 1U) = 0U;

    const std::vector<std::uint32_t> dense = BuildDenseOwnerOracle(plan, grid, modelMask);
    std::vector<std::uint32_t> owner(columnCount, 0U);
    bool passed{true};
    for (int layer{0}; layer < grid.layerCount; ++layer)
    {
        MaterializeMaterialOwnershipLayer(plan, layer, modelMask, owner);
        for (std::size_t column{0}; column < columnCount; ++column)
        {
            const std::uint32_t expected =
                dense.at(static_cast<std::size_t>(layer) * columnCount + column);
            if (owner.at(column) != expected)
            {
                passed = ExpectTrue(false, "materialized layer matches dense oracle") && passed;
                break;
            }
        }
    }
    passed = ExpectTrue(owner.at(0U) == kNoMaterialOwner, "masked-out pixel stays unowned") && passed;
    return passed;
}

/// @brief 物化热路径不得分配，且可反复写入同一 buffer 地址。
bool MaterializationIsAllocationFreeAndReusesBuffer()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 3.0);
    AppendClosedBox(mesh, "01", 2.0, 5.0);
    const MaterialVolumeGrid grid = MakeGrid();
    const MaterialVolumePolicyConfig policy = MakePolicy(200, 100);

    MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = grid;
    const MaterialVolumePlan plan = BuildMaterialVolumePlan(request);

    const std::size_t columnCount = ColumnCountOf(grid);
    const std::vector<std::uint8_t> modelMask(columnCount, 1U);
    std::vector<std::uint32_t> owner(columnCount, 0U);
    const std::uint32_t* const bufferAddress = owner.data();

    gAllocationCount.store(0U, std::memory_order_relaxed);
    gTrackAllocations.store(true, std::memory_order_relaxed);
    for (int repeat{0}; repeat < 4; ++repeat)
    {
        for (int layer{0}; layer < grid.layerCount; ++layer)
        {
            MaterializeMaterialOwnershipLayer(plan, layer, modelMask, owner);
        }
    }
    gTrackAllocations.store(false, std::memory_order_relaxed);

    bool passed{true};
    passed = ExpectTrue(
                 gAllocationCount.load(std::memory_order_relaxed) == 0U,
                 "materialization hot path performs no heap allocation")
        && passed;
    passed = ExpectTrue(owner.data() == bufferAddress, "caller buffer address is reused") && passed;
    return passed;
}

/// @brief 断言构建按预期抛出指定稳定错误码。
bool ExpectBuildRejects(
    const MaterialVolumeBuildRequest& request,
    const MaterialVolumeErrorCode expectedCode,
    const std::string& message)
{
    try
    {
        const MaterialVolumePlan plan = BuildMaterialVolumePlan(request);
        (void)plan;
    }
    catch (const MaterialVolumeError& error)
    {
        if (error.Code() == expectedCode)
        {
            return true;
        }
        std::cerr << "FAIL " << message << " (unexpected code: " << error.what() << ")\n";
        return false;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << message << " (unexpected exception: " << error.what() << ")\n";
        return false;
    }
    std::cerr << "FAIL " << message << " (no rejection)\n";
    return false;
}

/// @brief 开放材质、缺失优先级、同级重叠与缺材质绑定全部构建期 fail closed。
bool BuildFailsClosedOnUnsupportedInput()
{
    bool passed{true};
    const MaterialVolumeGrid grid = MakeGrid();

    // 开放表面：单张水平面，垂直射线只命中一次。
    {
        AdaptedTriangleMesh mesh;
        AppendClosedBox(mesh, "02", 0.0, 2.0);
        const int p00 = AppendVertex(mesh, 0.0, 0.0, 3.0);
        const int p10 = AppendVertex(mesh, 4.0, 0.0, 3.0);
        const int p11 = AppendVertex(mesh, 4.0, 4.0, 3.0);
        const int p01 = AppendVertex(mesh, 0.0, 4.0, 3.0);
        AppendTriangle(mesh, "01", p00, p10, p11);
        AppendTriangle(mesh, "01", p00, p11, p01);
        const MaterialVolumePolicyConfig policy = MakePolicy(200, 100);
        MaterialVolumeBuildRequest request;
        request.mesh = &mesh;
        request.policy = &policy;
        request.grid = grid;
        passed = ExpectBuildRejects(
                     request,
                     MaterialVolumeErrorCode::OpenSurfaceRequiresPolicy,
                     "open surface material is rejected without an explicit policy")
            && passed;
    }

    // 缺失优先级规则。
    {
        AdaptedTriangleMesh mesh;
        AppendClosedBox(mesh, "02", 0.0, 2.0);
        MaterialVolumePolicyConfig policy;
        policy.enabled = true;
        MaterialVolumeBuildRequest request;
        request.mesh = &mesh;
        request.policy = &policy;
        request.grid = grid;
        passed = ExpectBuildRejects(
                     request,
                     MaterialVolumeErrorCode::OverlapUnresolved,
                     "material without an explicit priority rule is rejected")
            && passed;
    }

    // 同级优先级的实际重叠。
    {
        AdaptedTriangleMesh mesh;
        AppendClosedBox(mesh, "02", 0.0, 3.0);
        AppendClosedBox(mesh, "01", 2.0, 5.0);
        const MaterialVolumePolicyConfig policy = MakePolicy(100, 100);
        MaterialVolumeBuildRequest request;
        request.mesh = &mesh;
        request.policy = &policy;
        request.grid = grid;
        passed = ExpectBuildRejects(
                     request,
                     MaterialVolumeErrorCode::OverlapUnresolved,
                     "equal priority overlap is rejected at build time")
            && passed;
    }

    // 三角面未绑定材质。
    {
        AdaptedTriangleMesh mesh;
        AppendClosedBox(mesh, "", 0.0, 2.0);
        MaterialVolumePolicyConfig policy;
        policy.enabled = true;
        MaterialVolumeBuildRequest request;
        request.mesh = &mesh;
        request.policy = &policy;
        request.grid = grid;
        passed = ExpectBuildRejects(
                     request,
                     MaterialVolumeErrorCode::MaterialMissing,
                     "triangles without a bound material are rejected")
            && passed;
    }

    // 非法栅格。
    {
        AdaptedTriangleMesh mesh;
        AppendClosedBox(mesh, "02", 0.0, 2.0);
        const MaterialVolumePolicyConfig policy = MakePolicy(200, 100);
        MaterialVolumeGrid invalid = grid;
        invalid.layerThicknessMm = 0.0;
        MaterialVolumeBuildRequest request;
        request.mesh = &mesh;
        request.policy = &policy;
        request.grid = invalid;
        passed = ExpectBuildRejects(
                     request,
                     MaterialVolumeErrorCode::TopologyInvalid,
                     "non-positive layer thickness is rejected")
            && passed;
    }
    return passed;
}

/// @brief 构建期取消必须抛出且不产出半成品 plan。
bool BuildCancellationLeavesNoPartialPlan()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 3.0);
    const MaterialVolumeGrid grid = MakeGrid();
    MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.overlap.rules.push_back({"02", 100});

    MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = grid;
    request.cancellationRequested = []() { return true; };

    bool passed{true};
    passed = ExpectBuildRejects(
                 request,
                 MaterialVolumeErrorCode::BudgetExceeded,
                 "cancelled build fails closed without returning a plan")
        && passed;
    return passed;
}

/// @brief 物化的层号与缓冲区尺寸校验必须显式失败，不得静默无操作。
bool MaterializationValidatesArguments()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 2.0);
    const MaterialVolumeGrid grid = MakeGrid();
    MaterialVolumePolicyConfig policy;
    policy.enabled = true;
    policy.overlap.rules.push_back({"02", 100});

    MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = grid;
    const MaterialVolumePlan plan = BuildMaterialVolumePlan(request);

    const std::size_t columnCount = ColumnCountOf(grid);
    const std::vector<std::uint8_t> modelMask(columnCount, 1U);
    std::vector<std::uint32_t> owner(columnCount, 0U);
    std::vector<std::uint32_t> shortOwner(columnCount - 1U, 0U);

    const auto expectThrows = [](const std::function<void()>& operation, const std::string& message) {
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

    bool passed{true};
    passed = expectThrows(
                 [&]() { MaterializeMaterialOwnershipLayer(plan, -1, modelMask, owner); },
                 "negative layer index is rejected")
        && passed;
    passed = expectThrows(
                 [&]() {
                     MaterializeMaterialOwnershipLayer(plan, grid.layerCount, modelMask, owner);
                 },
                 "out-of-range layer index is rejected")
        && passed;
    passed = expectThrows(
                 [&]() { MaterializeMaterialOwnershipLayer(plan, 0, modelMask, shortOwner); },
                 "undersized owner buffer is rejected")
        && passed;
    return passed;
}

/// @brief compact 布局必须真的紧凑：区间总数远小于材质数 × 层数 × 像素数。
bool CompactLayoutAvoidsDenseStack()
{
    AdaptedTriangleMesh mesh;
    AppendClosedBox(mesh, "02", 0.0, 3.0);
    AppendClosedBox(mesh, "01", 2.0, 5.0);
    const MaterialVolumeGrid grid = MakeGrid();
    const MaterialVolumePolicyConfig policy = MakePolicy(200, 100);

    MaterialVolumeBuildRequest request;
    request.mesh = &mesh;
    request.policy = &policy;
    request.grid = grid;
    const MaterialVolumePlan plan = BuildMaterialVolumePlan(request);

    const std::size_t columnCount = ColumnCountOf(grid);
    const std::size_t denseStackSize = plan.MaterialNames().size()
        * static_cast<std::size_t>(grid.layerCount) * columnCount;

    bool passed{true};
    passed = ExpectTrue(
                 plan.ColumnIntervalOffsets().size() == columnCount + 1U,
                 "column offset table has ColumnCount + 1 entries")
        && passed;
    passed = ExpectTrue(
                 plan.ColumnIntervalOffsets().front() == 0U
                     && plan.ColumnIntervalOffsets().back() == plan.Intervals().size(),
                 "offset table brackets the flattened interval array")
        && passed;
    // 每列两段区间，总计 32 段，远小于 2 x 8 x 16 = 256 的稠密栈。
    passed = ExpectTrue(
                 plan.Intervals().size() == columnCount * 2U,
                 "flattened interval count equals two per column")
        && passed;
    passed = ExpectTrue(
                 plan.Intervals().size() * 4U < denseStackSize,
                 "compact layout is far smaller than a dense material x layer x pixel stack")
        && passed;
    return passed;
}

/// @brief 重复构建必须产出逐字节相同的 compact 计划。
bool PlanBuildIsDeterministic()
{
    const MaterialVolumeGrid grid = MakeGrid();
    const MaterialVolumePolicyConfig policy = MakePolicy(200, 100);
    const auto build = [&grid, &policy](AdaptedTriangleMesh& mesh) {
        MaterialVolumeBuildRequest request;
        request.mesh = &mesh;
        request.policy = &policy;
        request.grid = grid;
        return BuildMaterialVolumePlan(request);
    };

    AdaptedTriangleMesh first;
    AppendClosedBox(first, "02", 0.0, 3.0);
    AppendClosedBox(first, "01", 2.0, 5.0);
    AdaptedTriangleMesh second;
    AppendClosedBox(second, "02", 0.0, 3.0);
    AppendClosedBox(second, "01", 2.0, 5.0);

    const MaterialVolumePlan planA = build(first);
    const MaterialVolumePlan planB = build(second);

    bool passed{true};
    passed = ExpectTrue(
                 planA.Intervals().size() == planB.Intervals().size(),
                 "repeated builds produce the same interval count")
        && passed;
    if (planA.Intervals().size() != planB.Intervals().size())
    {
        return passed;
    }
    for (std::size_t index{0}; index < planA.Intervals().size(); ++index)
    {
        const auto& lhs = planA.Intervals()[index];
        const auto& rhs = planB.Intervals()[index];
        if (lhs.firstLayerInclusive != rhs.firstLayerInclusive
            || lhs.lastLayerInclusive != rhs.lastLayerInclusive
            || lhs.materialIndex != rhs.materialIndex)
        {
            passed = ExpectTrue(false, "repeated builds produce identical intervals") && passed;
            break;
        }
    }
    for (std::size_t index{0}; index < planA.ColumnIntervalOffsets().size(); ++index)
    {
        if (planA.ColumnIntervalOffsets()[index] != planB.ColumnIntervalOffsets()[index])
        {
            passed = ExpectTrue(false, "repeated builds produce identical offset tables") && passed;
            break;
        }
    }
    return passed;
}

}  // namespace

int main()
{
    const std::array<std::pair<const char*, std::function<bool()>>, 9> tests{{
        {"stacked_closed_bodies_hand_checkable", StackedClosedBodiesProduceHandCheckableOwners},
        {"overlapping_bodies_resolve_by_priority", OverlappingBodiesResolveByPriority},
        {"separated_bodies_keep_cavity_unowned", SeparatedBodiesKeepCavityUnowned},
        {"materialization_matches_dense_oracle", MaterializationMatchesDenseOracle},
        {"materialization_allocation_free", MaterializationIsAllocationFreeAndReusesBuffer},
        {"build_fails_closed_on_unsupported_input", BuildFailsClosedOnUnsupportedInput},
        {"build_cancellation_leaves_no_partial_plan", BuildCancellationLeavesNoPartialPlan},
        {"materialization_validates_arguments", MaterializationValidatesArguments},
        {"compact_layout_avoids_dense_stack", CompactLayoutAvoidsDenseStack},
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
    try
    {
        if (!PlanBuildIsDeterministic())
        {
            std::cerr << "FAIL plan_build_deterministic\n";
            ++failed;
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL plan_build_deterministic: " << error.what() << '\n';
        ++failed;
    }
    if (failed == 0)
    {
        std::cout << "PASS MatvolIntervalTests " << (tests.size() + 1U) << "/" << (tests.size() + 1U)
                  << '\n';
        return 0;
    }
    std::cerr << "FAIL MatvolIntervalTests " << failed << " failed\n";
    return 1;
}
