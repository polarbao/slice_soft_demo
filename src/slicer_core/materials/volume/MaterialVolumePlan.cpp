#include "slicer_core/materials/volume/MaterialVolumePlan.h"

#include "slicer_core/materials/volume/MaterialTopologyClassifier.h"
#include "slicer_core/materials/volume/MaterialVolumeError.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

// 与既有 S0 采样规则逐字一致（slicer.cpp:1179-1199、:1202-1208）。
constexpr double kBarycentricEpsilon{-1.0e-9};
constexpr double kDegenerateDenominator{1.0e-12};
constexpr double kCoincidentHitEpsilonMm{1.0e-9};

/// @brief 复刻既有 XY 重心坐标判定；竖直侧壁因分母退化被跳过。
bool PointInTriangleXy(
    const double px,
    const double py,
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    double& w0,
    double& w1,
    double& w2)
{
    const double denominator = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (std::abs(denominator) < kDegenerateDenominator)
    {
        return false;
    }
    w0 = ((b.y - c.y) * (px - c.x) + (c.x - b.x) * (py - c.y)) / denominator;
    w1 = ((c.y - a.y) * (px - c.x) + (a.x - c.x) * (py - c.y)) / denominator;
    w2 = 1.0 - w0 - w1;
    return w0 >= kBarycentricEpsilon && w1 >= kBarycentricEpsilon && w2 >= kBarycentricEpsilon;
}

int FirstLayerAtOrAboveZ(const double zMm, const double layerThicknessMm)
{
    return static_cast<int>(std::ceil(zMm / layerThicknessMm - 0.5));
}

int LastLayerAtOrBelowZ(const double zMm, const double layerThicknessMm)
{
    return static_cast<int>(std::floor(zMm / layerThicknessMm - 0.5));
}

std::string DescribeColumn(const int x, const int y)
{
    std::ostringstream stream;
    stream << "column (" << x << ", " << y << ")";
    return stream.str();
}

void ThrowIfCancelled(const MaterialVolumeBuildRequest& request)
{
    if (request.cancellationRequested && request.cancellationRequested())
    {
        throw MaterialVolumeError(
            MaterialVolumeErrorCode::BudgetExceeded,
            "material volume plan build was cancelled");
    }
}

/// @brief 解析材质的显式优先级；启用策略后每个材质都必须命中一条规则。
int ResolveExplicitPriority(const MaterialVolumePolicyConfig& policy, const std::string& materialName)
{
    for (const MaterialVolumeOverlapRuleConfig& rule : policy.overlap.rules)
    {
        if (rule.match_material_name == materialName)
        {
            return rule.priority;
        }
    }
    throw MaterialVolumeError(
        MaterialVolumeErrorCode::OverlapUnresolved,
        "material '" + materialName + "' has no explicit priority rule");
}

}  // namespace

std::span<const MaterialLayerInterval> MaterialVolumePlan::ColumnIntervals(
    const std::size_t columnIndex) const
{
    if (columnIndex >= columnCount_)
    {
        throw std::out_of_range("MaterialVolumePlan::ColumnIntervals column index out of range");
    }
    const std::uint32_t begin = columnIntervalOffsets_.at(columnIndex);
    const std::uint32_t end = columnIntervalOffsets_.at(columnIndex + 1U);
    return std::span<const MaterialLayerInterval>{
        intervals_.data() + begin, static_cast<std::size_t>(end - begin)};
}

MaterialVolumePlan BuildMaterialVolumePlan(const MaterialVolumeBuildRequest& request)
{
    if (request.mesh == nullptr || request.policy == nullptr)
    {
        throw MaterialVolumeError(
            MaterialVolumeErrorCode::TopologyInvalid,
            "material volume build request requires mesh and policy");
    }
    const MaterialVolumeGrid& grid = request.grid;
    if (grid.widthPx <= 0 || grid.heightPx <= 0 || grid.layerCount <= 0
        || grid.pixelSizeXMm <= 0.0 || grid.pixelSizeYMm <= 0.0 || grid.layerThicknessMm <= 0.0)
    {
        throw MaterialVolumeError(
            MaterialVolumeErrorCode::TopologyInvalid,
            "material volume grid must have positive extents and spacing");
    }
    // 溢出安全的列数计算。
    const std::uint64_t columnCount64 =
        static_cast<std::uint64_t>(grid.widthPx) * static_cast<std::uint64_t>(grid.heightPx);
    if (columnCount64 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw MaterialVolumeError(
            MaterialVolumeErrorCode::BudgetExceeded, "material volume grid column count overflows");
    }

    // 逐材质拓扑分类：只有 ClosedOrientable 才允许进入区间求解。
    const std::vector<MaterialTopologyFact> facts = ClassifyMaterialTopologies(*request.mesh);
    std::vector<std::string> materialNames;
    std::vector<int> materialPriorities;
    std::vector<std::string> toleratedSelfIntersecting;
    std::map<std::string, std::uint32_t> materialIndexByName;
    for (const MaterialTopologyFact& fact : facts)
    {
        if (!request.materialNameFilter.empty()
            && fact.materialName != request.materialNameFilter)
        {
            continue;
        }
        if (fact.materialName.empty())
        {
            throw MaterialVolumeError(
                MaterialVolumeErrorCode::MaterialMissing,
                "mesh contains triangles without a bound material");
        }
        if (fact.kind == MaterialTopologyKind::OpenSurface)
        {
            throw MaterialVolumeError(
                MaterialVolumeErrorCode::OpenSurfaceRequiresPolicy,
                "material '" + fact.materialName + "' is an open surface and requires an explicit "
                "openSurface policy");
        }
        // 有界放宽：仅当材质【仍是闭合曲面】（真开边与非流形边均为 0）且自交对数不超过
        // 显式上限时放行。闭合性保证由 Jordan–Brouwer 得到偶数交点，区间形状良好；
        // 但缠绕数大于 1 处的归属仍可能错，误差被相交面投影界住，故必须留痕供报告披露。
        // 上限使数千至数万对的「需重建」资产仍然 fail closed，本放宽不是它们的旁路。
        const bool toleratedSelfIntersection =
            fact.kind == MaterialTopologyKind::SelfIntersecting
            && request.policy->topology.self_intersection_policy
                == "tolerate_closed_self_intersection"
            && fact.boundaryEdgeCount == 0U
            && fact.nonManifoldEdgeCount == 0U
            && fact.confirmedSelfIntersectionPairs
                <= static_cast<std::uint64_t>(
                    request.policy->topology.max_self_intersection_pairs);
        if (fact.kind != MaterialTopologyKind::ClosedOrientable && !toleratedSelfIntersection)
        {
            throw MaterialVolumeError(
                MaterialVolumeErrorCode::TopologyInvalid,
                "material '" + fact.materialName + "' topology is "
                    + MaterialTopologyKindName(fact.kind));
        }
        if (toleratedSelfIntersection)
        {
            toleratedSelfIntersecting.push_back(fact.materialName);
        }
        materialIndexByName.emplace(fact.materialName, static_cast<std::uint32_t>(materialNames.size()));
        materialNames.push_back(fact.materialName);
        materialPriorities.push_back(ResolveExplicitPriority(*request.policy, fact.materialName));
    }
    if (materialNames.empty())
    {
        throw MaterialVolumeError(
            MaterialVolumeErrorCode::MaterialMissing,
            request.materialNameFilter.empty()
                ? "mesh declares no usable material"
                : "mesh does not contain filtered material '" + request.materialNameFilter + "'");
    }

    // 按材质分组三角面下标，供逐列求交复用，避免每列重扫全网格属性。
    std::vector<std::vector<std::size_t>> trianglesByMaterial(materialNames.size());
    const std::size_t triangleCount =
        std::min(request.mesh->mesh.triangles.size(), request.mesh->triangle_attributes.size());
    for (std::size_t index{0}; index < triangleCount; ++index)
    {
        const auto found =
            materialIndexByName.find(request.mesh->triangle_attributes.at(index).material_name);
        if (found == materialIndexByName.end())
        {
            continue;
        }
        trianglesByMaterial.at(found->second).push_back(index);
    }

    MaterialVolumePlan plan;
    plan.layerCount_ = grid.layerCount;
    plan.columnCount_ = static_cast<std::size_t>(columnCount64);
    plan.materialNames_ = std::move(materialNames);
    plan.materialPriorities_ = std::move(materialPriorities);
    plan.toleratedSelfIntersectingMaterials_ = std::move(toleratedSelfIntersecting);
    plan.topologyFacts_ = facts;
    plan.columnIntervalOffsets_.assign(plan.columnCount_ + 1U, 0U);

    // 逐列求交所用的复用缓冲，循环外分配一次。
    std::vector<double> hits;
    std::vector<double> merged;
    std::vector<MaterialLayerInterval> columnIntervals;

    for (int y{0}; y < grid.heightPx; ++y)
    {
        ThrowIfCancelled(request);
        for (int x{0}; x < grid.widthPx; ++x)
        {
            const std::size_t columnIndex = static_cast<std::size_t>(y)
                * static_cast<std::size_t>(grid.widthPx) + static_cast<std::size_t>(x);
            const double px = grid.originXMm + (static_cast<double>(x) + 0.5) * grid.pixelSizeXMm;
            const double py = grid.originYMm + (static_cast<double>(y) + 0.5) * grid.pixelSizeYMm;
            columnIntervals.clear();

            for (std::size_t material{0}; material < plan.materialNames_.size(); ++material)
            {
                hits.clear();
                for (const std::size_t triangleIndex : trianglesByMaterial.at(material))
                {
                    const std::array<int, 3>& corners =
                        request.mesh->mesh.triangles.at(triangleIndex);
                    const Vec3& a =
                        request.mesh->mesh.vertices.at(static_cast<std::size_t>(corners[0]));
                    const Vec3& b =
                        request.mesh->mesh.vertices.at(static_cast<std::size_t>(corners[1]));
                    const Vec3& c =
                        request.mesh->mesh.vertices.at(static_cast<std::size_t>(corners[2]));
                    double w0{0.0};
                    double w1{0.0};
                    double w2{0.0};
                    if (!PointInTriangleXy(px, py, a, b, c, w0, w1, w2))
                    {
                        continue;
                    }
                    hits.push_back(w0 * a.z + w1 * b.z + w2 * c.z);
                }
                if (hits.empty())
                {
                    continue;
                }
                std::sort(hits.begin(), hits.end());

                // 合并共面与共享边造成的重复命中，保持奇偶配对稳定。
                merged.clear();
                for (const double z : hits)
                {
                    if (!merged.empty() && std::abs(z - merged.back()) <= kCoincidentHitEpsilonMm)
                    {
                        continue;
                    }
                    merged.push_back(z);
                }
                if (merged.size() % 2U != 0U)
                {
                    throw MaterialVolumeError(
                        MaterialVolumeErrorCode::IntersectionUnpaired,
                        "material '" + plan.materialNames_.at(material) + "' produced "
                            + std::to_string(merged.size()) + " unpaired intersections at "
                            + DescribeColumn(x, y));
                }
                for (std::size_t pair{0}; pair + 1U < merged.size(); pair += 2U)
                {
                    const int firstLayer =
                        std::max(0, FirstLayerAtOrAboveZ(merged.at(pair), grid.layerThicknessMm));
                    const int lastLayer = std::min(
                        grid.layerCount - 1,
                        LastLayerAtOrBelowZ(merged.at(pair + 1U), grid.layerThicknessMm));
                    if (firstLayer > lastLayer)
                    {
                        continue;
                    }
                    columnIntervals.push_back(MaterialLayerInterval{
                        firstLayer, lastLayer, static_cast<std::uint32_t>(material)});
                }
            }

            // 同级优先级的实际重叠必须在构建期阻断，物化阶段不再判定。
            for (std::size_t left{0}; left < columnIntervals.size(); ++left)
            {
                for (std::size_t right{left + 1U}; right < columnIntervals.size(); ++right)
                {
                    const MaterialLayerInterval& lhs = columnIntervals.at(left);
                    const MaterialLayerInterval& rhs = columnIntervals.at(right);
                    if (lhs.materialIndex == rhs.materialIndex)
                    {
                        continue;
                    }
                    const int overlapFirst =
                        std::max(lhs.firstLayerInclusive, rhs.firstLayerInclusive);
                    const int overlapLast = std::min(lhs.lastLayerInclusive, rhs.lastLayerInclusive);
                    if (overlapFirst > overlapLast)
                    {
                        continue;
                    }
                    if (plan.materialPriorities_.at(lhs.materialIndex)
                        == plan.materialPriorities_.at(rhs.materialIndex))
                    {
                        throw MaterialVolumeError(
                            MaterialVolumeErrorCode::OverlapUnresolved,
                            "materials '" + plan.materialNames_.at(lhs.materialIndex) + "' and '"
                                + plan.materialNames_.at(rhs.materialIndex)
                                + "' overlap with equal priority at " + DescribeColumn(x, y));
                    }
                }
            }

            // 确定性排序：先按起始层，再按材质下标。
            std::sort(
                columnIntervals.begin(),
                columnIntervals.end(),
                [](const MaterialLayerInterval& lhs, const MaterialLayerInterval& rhs) {
                    if (lhs.firstLayerInclusive != rhs.firstLayerInclusive)
                    {
                        return lhs.firstLayerInclusive < rhs.firstLayerInclusive;
                    }
                    return lhs.materialIndex < rhs.materialIndex;
                });
            plan.intervals_.insert(
                plan.intervals_.end(), columnIntervals.begin(), columnIntervals.end());
            plan.columnIntervalOffsets_.at(columnIndex + 1U) =
                static_cast<std::uint32_t>(plan.intervals_.size());
        }
    }
    ThrowIfCancelled(request);
    return plan;
}

void MaterializeMaterialOwnershipLayer(
    const MaterialVolumePlan& plan,
    const int layerIndex,
    const std::span<const std::uint8_t> modelMask,
    const std::span<std::uint32_t> ownerOut)
{
    const std::size_t columnCount = plan.ColumnCount();
    if (layerIndex < 0 || layerIndex >= plan.LayerCount())
    {
        throw std::invalid_argument(
            "Material ownership materialization layer index is out of range");
    }
    if (modelMask.size() != columnCount || ownerOut.size() != columnCount)
    {
        throw std::invalid_argument(
            "Material ownership materialization buffer size is invalid");
    }
    const std::span<const std::uint32_t> offsets = plan.ColumnIntervalOffsets();
    const std::span<const MaterialLayerInterval> intervals = plan.Intervals();
    const std::span<const int> priorities = plan.MaterialPriorities();

    for (std::size_t column{0}; column < columnCount; ++column)
    {
        ownerOut[column] = kNoMaterialOwner;
        if (modelMask[column] == 0U)
        {
            continue;
        }
        int bestPriority{0};
        bool hasOwner{false};
        const std::uint32_t begin = offsets[column];
        const std::uint32_t end = offsets[column + 1U];
        for (std::uint32_t index{begin}; index < end; ++index)
        {
            const MaterialLayerInterval& interval = intervals[index];
            if (layerIndex < interval.firstLayerInclusive
                || layerIndex > interval.lastLayerInclusive)
            {
                continue;
            }
            const int priority = priorities[interval.materialIndex];
            if (!hasOwner || priority > bestPriority)
            {
                hasOwner = true;
                bestPriority = priority;
                ownerOut[column] = interval.materialIndex;
            }
        }
    }
}

}  // namespace slicer_core
