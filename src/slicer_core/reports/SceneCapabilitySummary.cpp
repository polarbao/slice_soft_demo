#include "slicer_core/reports/SceneCapabilitySummary.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr std::size_t kChannelCount{6U};
constexpr std::array<const char*, kChannelCount> kChannelNames{
    "R", "G", "B", "W", "S", "V"};

Json ChannelCountsToJson(
    const std::array<std::uint64_t, kChannelCount>& counts)
{
    Json::Object object;
    for (std::size_t index{0U}; index < kChannelCount; ++index)
    {
        object.emplace(kChannelNames.at(index), counts.at(index));
    }
    return Json{std::move(object)};
}

Json TransformToJson(const ModelTransform& transform)
{
    return Json::object({
        {"translateXmm", transform.translatexmm},
        {"translateYmm", transform.translateymm},
        {"rotateXdeg", transform.rotatexdeg},
        {"rotateYdeg", transform.rotateydeg},
        {"rotateZdeg", transform.rotatezdeg},
        {"uniformScale", transform.uniformscale},
        {"mirrorX", transform.mirrorx},
        {"mirrorY", transform.mirrory},
        {"landOnBuildPlate", transform.landonbuildplate},
    });
}

bool HasOwnership(
    const SceneInstanceRasterLayer& layer,
    const std::size_t pixelIndex)
{
    return layer.modelownership.at(pixelIndex) != 0U
        || layer.modelvarnishownership.at(pixelIndex) != 0U
        || layer.outervarnishownership.at(pixelIndex) != 0U
        || layer.supportownership.at(pixelIndex) != 0U;
}

Json EmptyBounds()
{
    return Json::object({
        {"valid", false},
        {"min", Json::array({0.0, 0.0, 0.0})},
        {"max", Json::array({0.0, 0.0, 0.0})},
    });
}

Json RasterBounds(
    const SceneRasterGrid& grid,
    const int minimumX,
    const int minimumY,
    const int minimumLayer,
    const int maximumX,
    const int maximumY,
    const int maximumLayer)
{
    if (minimumLayer < 0)
    {
        return EmptyBounds();
    }
    return Json::object({
        {"valid", true},
        {"min",
         Json::array({
             grid.originxmm
                 + static_cast<double>(minimumX) * grid.pitchxmm,
             grid.originymm
                 + static_cast<double>(minimumY) * grid.pitchymm,
             grid.originzmm
                 + static_cast<double>(minimumLayer)
                     * grid.layerthicknessmm,
         })},
        {"max",
         Json::array({
             grid.originxmm
                 + static_cast<double>(maximumX + 1) * grid.pitchxmm,
             grid.originymm
                 + static_cast<double>(maximumY + 1) * grid.pitchymm,
             grid.originzmm
                 + static_cast<double>(maximumLayer + 1)
                     * grid.layerthicknessmm,
         })},
    });
}

Json BuildVisibleInstance(
    const SceneModelInstance& sceneInstance,
    const SceneInstanceRaster& raster)
{
    if (!raster.localgrid.IsValid()
        || raster.layers.size()
            != static_cast<std::size_t>(raster.localgrid.layercount))
    {
        throw std::invalid_argument(
            "scene capability summary requires a complete visible raster");
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(raster.localgrid.widthpx)
        * static_cast<std::size_t>(raster.localgrid.heightpx);
    const std::size_t byteCount = pixelCount * kChannelCount;
    std::array<std::uint64_t, kChannelCount> printPixels{};
    int minimumX = raster.localgrid.widthpx;
    int minimumY = raster.localgrid.heightpx;
    int minimumLayer = raster.localgrid.layercount;
    int maximumX{-1};
    int maximumY{-1};
    int maximumLayer{-1};

    for (const SceneInstanceRasterLayer& layer : raster.layers)
    {
        if (layer.output.channels.size() != byteCount
            || layer.modelownership.size() != pixelCount
            || layer.modelvarnishownership.size() != pixelCount
            || layer.outervarnishownership.size() != pixelCount
            || layer.supportownership.size() != pixelCount)
        {
            throw std::invalid_argument(
                "scene capability summary raster layer is incomplete");
        }
        for (std::size_t pixelIndex{0U};
             pixelIndex < pixelCount;
             ++pixelIndex)
        {
            const std::size_t byteOffset = pixelIndex * kChannelCount;
            for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
            {
                if (layer.output.channels.at(byteOffset + channel)
                    != raster.protocol.empty_value)
                {
                    ++printPixels.at(channel);
                }
            }
            if (!HasOwnership(layer, pixelIndex))
            {
                continue;
            }
            const int x = static_cast<int>(
                pixelIndex % static_cast<std::size_t>(raster.localgrid.widthpx));
            const int y = static_cast<int>(
                pixelIndex / static_cast<std::size_t>(raster.localgrid.widthpx));
            minimumX = std::min(minimumX, x);
            minimumY = std::min(minimumY, y);
            minimumLayer = std::min(minimumLayer, layer.layerindex);
            maximumX = std::max(maximumX, x);
            maximumY = std::max(maximumY, y);
            maximumLayer = std::max(maximumLayer, layer.layerindex);
        }
    }

    const std::uint64_t sampleCount =
        static_cast<std::uint64_t>(pixelCount)
        * static_cast<std::uint64_t>(raster.layers.size());
    std::array<std::uint64_t, kChannelCount> emptyPixels{};
    for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
    {
        emptyPixels.at(channel) = sampleCount - printPixels.at(channel);
    }
    const int firstLayer = maximumLayer < 0 ? -1 : minimumLayer;
    return Json::object({
        {"instanceId", sceneInstance.instance.instanceid},
        {"modelId", sceneInstance.instance.modelid},
        {"layerRange", Json::array({firstLayer, maximumLayer})},
        {"printPixels", ChannelCountsToJson(printPixels)},
        {"emptyPixels", ChannelCountsToJson(emptyPixels)},
        {"bboxMm",
         RasterBounds(
             raster.localgrid,
             minimumX,
             minimumY,
             firstLayer,
             maximumX,
             maximumY,
             maximumLayer)},
        {"transformApplied", TransformToJson(sceneInstance.effectivetransform)},
    });
}

Json BuildVisibleInstance(
    const SceneModelInstance& sceneInstance,
    const SceneInstanceComposeStatistics& statistics)
{
    const auto& raster = statistics.raster;
    if (!raster.available || !raster.grid.IsValid())
    {
        throw std::invalid_argument(
            "scene capability summary requires fused raster statistics");
    }
    const std::uint64_t sampleCount =
        static_cast<std::uint64_t>(raster.grid.widthpx)
        * static_cast<std::uint64_t>(raster.grid.heightpx)
        * static_cast<std::uint64_t>(raster.grid.layercount);
    for (std::size_t channel{0U}; channel < kChannelCount; ++channel)
    {
        if (raster.printpixels[channel] > sampleCount
            || raster.emptypixels[channel] > sampleCount
            || raster.printpixels[channel] + raster.emptypixels[channel]
                != sampleCount)
        {
            throw std::invalid_argument(
                "scene capability summary fused channel counts are invalid");
        }
    }
    const int firstLayer = raster.maximumlayer < 0
        ? -1 : raster.minimumlayer;
    return Json::object({
        {"instanceId", sceneInstance.instance.instanceid},
        {"modelId", sceneInstance.instance.modelid},
        {"layerRange", Json::array({firstLayer, raster.maximumlayer})},
        {"printPixels", ChannelCountsToJson(raster.printpixels)},
        {"emptyPixels", ChannelCountsToJson(raster.emptypixels)},
        {"bboxMm",
         RasterBounds(
             raster.grid,
             raster.minimumx,
             raster.minimumy,
             firstLayer,
             raster.maximumx,
             raster.maximumy,
             raster.maximumlayer)},
        {"transformApplied", TransformToJson(sceneInstance.effectivetransform)},
    });
}

Json BuildHiddenInstance(const SceneModelInstance& sceneInstance)
{
    const std::array<std::uint64_t, kChannelCount> zeroCounts{};
    return Json::object({
        {"instanceId", sceneInstance.instance.instanceid},
        {"modelId", sceneInstance.instance.modelid},
        {"layerRange", Json::array({-1, -1})},
        {"printPixels", ChannelCountsToJson(zeroCounts)},
        {"emptyPixels", ChannelCountsToJson(zeroCounts)},
        {"bboxMm", EmptyBounds()},
        {"transformApplied", TransformToJson(sceneInstance.effectivetransform)},
    });
}

}  // namespace

bool SceneCapabilitySummaryDocument::IsValid() const
{
    return perinstance.is_array()
        && profileecho.is_object()
        && profileecho.contains("profileVersion")
        && profileecho.at("profileVersion").is_string()
        && !profileecho.at("profileVersion").as_string().empty()
        && profileecho.contains("profileHash")
        && profileecho.at("profileHash").is_string()
        && !profileecho.at("profileHash").as_string().empty();
}

std::optional<SceneCapabilitySummaryDocument> BuildSceneCapabilitySummary(
    const MultiModelScene& scene,
    const std::vector<SceneInstanceRaster>& rasters,
    const Json& profileDocument)
{
    if (!profileDocument.is_object()
        || !profileDocument.contains("profileVersion")
        || !profileDocument.contains("profileHash"))
    {
        return std::nullopt;
    }
    if (!profileDocument.at("profileVersion").is_string()
        || profileDocument.at("profileVersion").as_string().empty()
        || !profileDocument.at("profileHash").is_string()
        || profileDocument.at("profileHash").as_string().empty())
    {
        throw std::invalid_argument(
            "scene capability summary Profile identity is invalid");
    }

    std::unordered_map<std::string, const SceneInstanceRaster*> byInstanceId;
    byInstanceId.reserve(rasters.size());
    for (const SceneInstanceRaster& raster : rasters)
    {
        if (raster.instanceid.empty()
            || !byInstanceId.emplace(raster.instanceid, &raster).second)
        {
            throw std::invalid_argument(
                "scene capability summary raster identity is invalid");
        }
    }

    Json::Array perInstance;
    perInstance.reserve(scene.instances.size());
    for (const SceneModelInstance& sceneInstance : scene.instances)
    {
        const auto found = byInstanceId.find(
            sceneInstance.instance.instanceid);
        if (found == byInstanceId.end()
            || found->second->modelid != sceneInstance.instance.modelid
            || found->second->visible != sceneInstance.instance.visible)
        {
            throw std::invalid_argument(
                "scene capability summary instance evidence is stale");
        }
        perInstance.push_back(
            sceneInstance.instance.visible
                ? BuildVisibleInstance(sceneInstance, *found->second)
                : BuildHiddenInstance(sceneInstance));
    }
    if (byInstanceId.size() != scene.instances.size())
    {
        throw std::invalid_argument(
            "scene capability summary has unreferenced rasters");
    }

    SceneCapabilitySummaryDocument result;
    result.perinstance = Json{std::move(perInstance)};
    result.profileecho = Json::object({
        {"profileVersion", profileDocument.at("profileVersion")},
        {"profileHash", profileDocument.at("profileHash")},
    });
    if (!result.IsValid())
    {
        throw std::invalid_argument(
            "scene capability summary failed validation");
    }
    return result;
}

std::optional<SceneCapabilitySummaryDocument> BuildSceneCapabilitySummary(
    const MultiModelScene& scene,
    const std::vector<SceneInstanceRaster>& rasters,
    const std::filesystem::path& profileConfigPath)
{
    if (profileConfigPath.empty())
    {
        return std::nullopt;
    }
    std::ifstream input{profileConfigPath, std::ios::binary};
    if (!input)
    {
        throw std::invalid_argument(
            "scene capability summary Profile cannot be read");
    }
    return BuildSceneCapabilitySummary(scene, rasters, Json::parse(input));
}

std::optional<SceneCapabilitySummaryDocument> BuildSceneCapabilitySummary(
    const MultiModelScene& scene,
    const SceneLayerComposeStatistics& statistics,
    const std::filesystem::path& profileConfigPath)
{
    if (profileConfigPath.empty())
    {
        return std::nullopt;
    }
    std::ifstream input{profileConfigPath, std::ios::binary};
    if (!input)
    {
        throw std::invalid_argument(
            "scene capability summary Profile cannot be read");
    }
    const Json profileDocument = Json::parse(input);
    if (!profileDocument.is_object()
        || !profileDocument.contains("profileVersion")
        || !profileDocument.contains("profileHash"))
    {
        return std::nullopt;
    }
    if (!profileDocument.at("profileVersion").is_string()
        || profileDocument.at("profileVersion").as_string().empty()
        || !profileDocument.at("profileHash").is_string()
        || profileDocument.at("profileHash").as_string().empty())
    {
        throw std::invalid_argument(
            "scene capability summary Profile identity is invalid");
    }

    std::unordered_map<
        std::string,
        const SceneInstanceComposeStatistics*> byInstanceId;
    byInstanceId.reserve(statistics.instances.size());
    for (const SceneInstanceComposeStatistics& instance : statistics.instances)
    {
        if (instance.instanceid.empty()
            || !byInstanceId.emplace(instance.instanceid, &instance).second)
        {
            throw std::invalid_argument(
                "scene capability summary fused instance identity is invalid");
        }
    }

    Json::Array perInstance;
    perInstance.reserve(scene.instances.size());
    std::size_t visibleCount{0U};
    for (const SceneModelInstance& sceneInstance : scene.instances)
    {
        if (!sceneInstance.instance.visible)
        {
            perInstance.push_back(BuildHiddenInstance(sceneInstance));
            continue;
        }
        ++visibleCount;
        const auto found = byInstanceId.find(
            sceneInstance.instance.instanceid);
        if (found == byInstanceId.end())
        {
            throw std::invalid_argument(
                "scene capability summary is missing fused visible-instance evidence");
        }
        perInstance.push_back(
            BuildVisibleInstance(sceneInstance, *found->second));
    }
    if (visibleCount != byInstanceId.size()
        || visibleCount != statistics.visibleinstancecount
        || scene.instances.size() != statistics.totalinstancecount)
    {
        throw std::invalid_argument(
            "scene capability summary fused instance counts do not match");
    }

    SceneCapabilitySummaryDocument result;
    result.perinstance = Json{std::move(perInstance)};
    result.profileecho = Json::object({
        {"profileVersion", profileDocument.at("profileVersion")},
        {"profileHash", profileDocument.at("profileHash")},
    });
    if (!result.IsValid())
    {
        throw std::invalid_argument(
            "scene capability summary failed validation");
    }
    return result;
}

}  // namespace slicer_core
