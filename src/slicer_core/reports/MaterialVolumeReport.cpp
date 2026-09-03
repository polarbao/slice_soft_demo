#include "slicer_core/reports/MaterialVolumeReport.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{

int ResolvePriority(const MaterialVolumePlan& plan, const std::size_t materialIndex)
{
    const std::span<const int> priorities = plan.MaterialPriorities();
    if (materialIndex >= priorities.size())
    {
        return 0;
    }
    return priorities[materialIndex];
}

const MaterialTopologyFact* FindFact(
    const std::span<const MaterialTopologyFact> facts,
    const std::string& materialName)
{
    for (const MaterialTopologyFact& fact : facts)
    {
        if (fact.materialName == materialName)
        {
            return &fact;
        }
    }
    return nullptr;
}

}  // namespace

MaterialVolumeLayerStat CountMaterialVolumeLayerOwners(
    const MaterialVolumePlan& plan,
    const int layerIndex,
    const std::span<const std::uint32_t> ownerLayer,
    const std::span<const std::uint8_t> modelMask)
{
    if (ownerLayer.size() != plan.ColumnCount() || modelMask.size() != plan.ColumnCount())
    {
        throw std::invalid_argument("Material volume layer statistics buffer size is invalid");
    }
    MaterialVolumeLayerStat stat;
    stat.layerIndex = layerIndex;
    stat.ownerPixelsByMaterial.assign(plan.MaterialNames().size(), 0U);
    for (std::size_t column{0}; column < ownerLayer.size(); ++column)
    {
        const std::uint32_t owner = ownerLayer[column];
        if (owner == kNoMaterialOwner)
        {
            if (modelMask[column] != 0U)
            {
                ++stat.unownedModelPixels;
            }
            continue;
        }
        if (static_cast<std::size_t>(owner) < stat.ownerPixelsByMaterial.size())
        {
            ++stat.ownerPixelsByMaterial.at(static_cast<std::size_t>(owner));
        }
    }
    return stat;
}

Json BuildMaterialVolumeReport(const MaterialVolumeReportInput& input)
{
    if (input.plan == nullptr || input.policy == nullptr)
    {
        throw std::invalid_argument("Material volume report requires a plan and a policy");
    }
    const MaterialVolumePlan& plan = *input.plan;
    const std::span<const std::string> materialNames = plan.MaterialNames();

    Json::Array materials;
    for (std::size_t index{0}; index < materialNames.size(); ++index)
    {
        const std::string& name = materialNames[index];
        Json::Object entry;
        entry["materialName"] = Json{name};
        entry["priority"] = Json{ResolvePriority(plan, index)};
        const MaterialTopologyFact* fact = FindFact(input.topologyFacts, name);
        entry["topology"] =
            Json{fact == nullptr ? std::string{"invalid"} : MaterialTopologyKindName(fact->kind)};
        if (fact != nullptr)
        {
            entry["triangleCount"] = Json{fact->triangleCount};
            entry["boundaryEdgeCount"] = Json{fact->boundaryEdgeCount};
            entry["materialInterfaceEdgeCount"] = Json{fact->materialInterfaceEdgeCount};
            entry["nonManifoldEdgeCount"] = Json{fact->nonManifoldEdgeCount};
        }
        if (input.rgbTable != nullptr && index < input.rgbTable->RgbByMaterial().size())
        {
            const std::array<std::uint8_t, 3>& rgb = input.rgbTable->RgbByMaterial()[index];
            entry["rgb"] = Json::array({
                Json{static_cast<int>(rgb[0])},
                Json{static_cast<int>(rgb[1])},
                Json{static_cast<int>(rgb[2])},
            });
            entry["rgbSource"] = Json{input.rgbTable->RgbSources()[index]};
        }
        materials.emplace_back(Json{std::move(entry)});
    }

    std::uint64_t ownerPixels{0U};
    std::uint64_t unownedModelPixels{0U};
    std::uint64_t whiteCarrierPixels{0U};
    // MR-09：逐材质累计，用于检出「已声明但一个像素都没拿到」的材质。
    std::vector<std::uint64_t> ownerPixelsTotalByMaterial(materialNames.size(), 0U);
    Json::Array layers;
    for (const MaterialVolumeLayerStat& stat : input.layers)
    {
        if (stat.ownerPixelsByMaterial.size() != materialNames.size())
        {
            throw std::invalid_argument(
                "Material volume report layer statistics do not match the material table");
        }
        Json::Array byMaterial;
        for (std::size_t index{0}; index < materialNames.size(); ++index)
        {
            ownerPixels += stat.ownerPixelsByMaterial.at(index);
            ownerPixelsTotalByMaterial.at(index) +=
                stat.ownerPixelsByMaterial.at(index);
            byMaterial.emplace_back(Json::object({
                {"materialName", Json{materialNames[index]}},
                {"pixels", Json{stat.ownerPixelsByMaterial.at(index)}},
            }));
        }
        unownedModelPixels += stat.unownedModelPixels;
        whiteCarrierPixels += stat.unprintableWhiteCarrierPixels;
        layers.emplace_back(Json::object({
            {"layerIndex", Json{stat.layerIndex}},
            {"ownerPixelsByMaterial", Json{std::move(byMaterial)}},
            {"unownedModelPixels", Json{stat.unownedModelPixels}},
            {"unprintableWhiteCarrierPixels", Json{stat.unprintableWhiteCarrierPixels}},
        }));
    }

    // MQ-05 放行的自交材质必须在报告里披露。MaterialVolumePlan.h 明确要求
    // 「不得静默吞掉」，若此处保持空数组，放行事实就只存在于内存里，
    // 产出物上完全看不出这一版是在放宽策略下切出来的。
    Json::Array warnings;
    for (const std::string& material : input.plan->ToleratedSelfIntersectingMaterials())
    {
        warnings.push_back(
            "material '" + material
            + "' was admitted under selfIntersectionPolicy=tolerate_closed_self_intersection");
    }

    // MR-09：已声明的材质在整个输出中一个像素都没拿到，即它被其它材质完全压掉。
    //
    // 该情形此前【完全静默】：退出码 0、errors 为空、unownedModelPixels 也是 0
    // （像素并非无主，而是全归了压过它的那个材质），产出物上唯一的痕迹就是
    // 这里的逐材质计数为 0。实测案例：内嵌资产误用同层号时，被包裹部件整个消失，
    // 只能靠肉眼在切片数据里发现。
    //
    // 取【警告】而非 fail-closed：材质被完全覆盖存在合法情形（声明了但几何在
    // 构建体积外、或被支撑/裁剪移除），一律阻断会拒掉本可交付的作业。
    // 同时把清单落成结构化字段 materialsWithoutPixels，使宿主与自动化检查
    // 不必解析告警文本即可判定。
    Json::Array materialsWithoutPixels;
    if (!input.layers.empty())
    {
        for (std::size_t index{0}; index < materialNames.size(); ++index)
        {
            if (ownerPixelsTotalByMaterial.at(index) != 0U)
            {
                continue;
            }
            materialsWithoutPixels.emplace_back(Json{materialNames[index]});
            warnings.push_back(
                "material '" + materialNames[index]
                + "' did not receive any pixel in the whole output; it is fully"
                  " overridden by another material. For layered assets check that"
                  " nested materials use distinct -L<n> layer numbers with the"
                  " enclosed one lower.");
        }
    }

    return Json::object({
        {"schema", "slicesoft.material_volume_report.1"},
        {"packageProtocol", "p0.rgbwsv.2"},
        {"enabled", input.policy->enabled},
        {"mode", input.policy->mode},
        {"missingMaterial", input.policy->missing_material},
        {"openSurface",
         Json::object({
             {"mode", input.policy->open_surface.mode},
             {"requestedThicknessMm", input.policy->open_surface.thickness_mm},
             {"placement", input.policy->open_surface.placement},
         })},
        {"overlap", Json::object({{"mode", input.policy->overlap.mode}})},
        {"materials", Json{std::move(materials)}},
        // MR-09：被完全压掉的材质清单，结构化以便宿主与自动化检查直接判定。
        {"materialsWithoutPixels", Json{std::move(materialsWithoutPixels)}},
        {"totals",
         Json::object({
             {"layerCount", Json{plan.LayerCount()}},
             {"columnCount", Json{static_cast<std::uint64_t>(plan.ColumnCount())}},
             {"intervalCount", Json{static_cast<std::uint64_t>(plan.Intervals().size())}},
             {"ownerPixels", Json{ownerPixels}},
             {"unownedModelPixels", Json{unownedModelPixels}},
             {"unprintableWhiteCarrierPixels", Json{whiteCarrierPixels}},
         })},
        {"layers", Json{std::move(layers)}},
        {"warnings", std::move(warnings)},
        {"errors", Json::array({})},
    });
}

Json BuildDisabledMaterialVolumeReport()
{
    return Json::object({
        {"schema", "slicesoft.material_volume_report.1"},
        {"packageProtocol", "p0.rgbwsv.2"},
        {"enabled", false},
        {"mode", "closed_intervals"},
        {"missingMaterial", "fail_closed"},
        {"openSurface", Json::object({
            {"mode", "reject"},
            {"requestedThicknessMm", 0.0},
            {"placement", "below_surface"}})},
        {"overlap", Json::object({{"mode", "explicit_priority"}})},
        {"materials", Json::array({})},
        {"totals", Json::object({
            {"layerCount", 0},
            {"columnCount", 0},
            {"intervalCount", 0},
            {"ownerPixels", 0},
            {"unownedModelPixels", 0},
            {"unprintableWhiteCarrierPixels", 0}})},
        {"layers", Json::array({})},
        {"warnings", Json::array({})},
        {"errors", Json::array({})},
    });
}

}  // namespace slicer_core
