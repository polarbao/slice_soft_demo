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
        {"warnings", Json::array({})},
        {"errors", Json::array({})},
    });
}

}  // namespace slicer_core
