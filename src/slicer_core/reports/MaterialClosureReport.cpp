#include "slicer_core/reports/MaterialClosureReport.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core
{
namespace
{

Json BuildUnavailableDiagnostic()
{
    return Json::object({
        {"severity", "warning"},
        {"code", "MATERIAL_CLOSURE_SOURCE_UNAVAILABLE"},
        {"message", "Material closure diagnostic source is not available."},
        {"layerIndex", nullptr},
        {"pixelCount", 0},
    });
}

Json BuildCandidateOnlyDiagnostic(const std::uint64_t pixelCount)
{
    return Json::object({
        {"severity", "warning"},
        {"code", "MATERIAL_CLOSURE_CANDIDATE_ONLY"},
        {"message", "RGBWSV inference is candidate evidence and cannot provide production acceptance."},
        {"layerIndex", nullptr},
        {"pixelCount", pixelCount},
    });
}

Json BuildGapDiagnostic(
    const std::string& code,
    const std::string& message,
    const int layerIndex,
    const int pixelCount)
{
    return Json::object({
        {"severity", "warning"},
        {"code", code},
        {"message", message},
        {"layerIndex", layerIndex},
        {"pixelCount", pixelCount},
    });
}

Json::Array BuildGapTypes(const MaterialClosureCandidateLayer& layer)
{
    Json::Array types;
    if (layer.colorFillGapPixels > 0)
    {
        types.emplace_back("COLOR_FILL_GAP");
    }
    if (layer.modelSupportGapPixels > 0)
    {
        types.emplace_back("MODEL_SUPPORT_GAP");
    }
    if (layer.colorSupportGapPixels > 0)
    {
        types.emplace_back("COLOR_SUPPORT_GAP");
    }
    return types;
}

}  // namespace

Json BuildMaterialClosureReportSkeleton(const MaterialClosureConfig& config, const int layerCount)
{
    if (layerCount < 0)
    {
        throw std::invalid_argument("material closure layer count must not be negative");
    }

    Json::Array diagnostics;
    if (config.enabled)
    {
        diagnostics.emplace_back(BuildUnavailableDiagnostic());
    }

    return Json::object({
        {"schema", "p0.material_closure.1"},
        {"packageProtocol", "p0.rgbwsv.2"},
        {"enabled", config.enabled},
        {"mode", config.mode},
        {"source", "unavailable"},
        {"confidence", "unavailable"},
        {"closureStatus", "not_available"},
        {"productionAcceptance", "not_evaluated"},
        {"repair",
         Json::object({
             {"enabled", config.repair.enabled},
             {"maxGapPx", config.max_gap_px},
             {"attempted", false},
             {"repairedPixels", 0},
         })},
        {"totals",
         Json::object({
             {"layerCount", layerCount},
             {"evaluatedLayerCount", 0},
             {"passLayerCount", 0},
             {"warningLayerCount", 0},
             {"failLayerCount", 0},
             {"totalGapPixels", 0},
             {"colorFillGapPixels", 0},
             {"modelSupportGapPixels", 0},
             {"colorSupportGapPixels", 0},
             {"internalVoidGapPixels", 0},
             {"varnishSupportGapPixels", 0},
             {"repairedPixels", 0},
             {"externalBackgroundProtectedPixels", 0},
         })},
        {"worstLayers", Json::array({})},
        {"layers", Json::array({})},
        {"diagnostics", Json{std::move(diagnostics)}},
    });
}

Json BuildMaterialClosureCandidateReport(
    const MaterialClosureConfig& config,
    const std::vector<MaterialClosureCandidateLayer>& layers)
{
    if (!config.enabled)
    {
        return BuildMaterialClosureReportSkeleton(config, static_cast<int>(layers.size()));
    }

    std::uint64_t totalGapPixels{0U};
    std::uint64_t colorFillGapPixels{0U};
    std::uint64_t modelSupportGapPixels{0U};
    std::uint64_t colorSupportGapPixels{0U};
    std::uint64_t externalBackgroundProtectedPixels{0U};
    Json::Array layerItems;
    Json::Array diagnostics;
    std::vector<MaterialClosureCandidateLayer> worstLayers;

    layerItems.reserve(layers.size());
    for (const MaterialClosureCandidateLayer& layer : layers)
    {
        if (layer.layerIndex < 0 || layer.gapPixels < 0 || layer.colorFillGapPixels < 0
            || layer.modelSupportGapPixels < 0 || layer.colorSupportGapPixels < 0
            || layer.externalBackgroundProtectedPixels < 0)
        {
            throw std::invalid_argument("material closure candidate report contains negative values");
        }

        totalGapPixels += static_cast<std::uint64_t>(layer.gapPixels);
        colorFillGapPixels += static_cast<std::uint64_t>(layer.colorFillGapPixels);
        modelSupportGapPixels += static_cast<std::uint64_t>(layer.modelSupportGapPixels);
        colorSupportGapPixels += static_cast<std::uint64_t>(layer.colorSupportGapPixels);
        externalBackgroundProtectedPixels +=
            static_cast<std::uint64_t>(layer.externalBackgroundProtectedPixels);

        layerItems.emplace_back(Json::object({
            {"layerIndex", layer.layerIndex},
            {"zMm", layer.zMm},
            {"closureStatus", "warning"},
            {"gapPixels", layer.gapPixels},
            {"gaps",
             Json::object({
                 {"colorFill", layer.colorFillGapPixels},
                 {"modelSupport", layer.modelSupportGapPixels},
                 {"colorSupport", layer.colorSupportGapPixels},
                 {"internalVoid", 0},
                 {"varnishSupport", 0},
             })},
            {"repair",
             Json::object({
                 {"attempted", false},
                 {"repairedPixels", 0},
                 {"remainingGapPixels", layer.gapPixels},
             })},
            {"externalBackgroundProtectedPixels", layer.externalBackgroundProtectedPixels},
            {"gapPreviewPath", ""},
        }));

        if (layer.colorFillGapPixels > 0)
        {
            diagnostics.emplace_back(BuildGapDiagnostic(
                "COLOR_FILL_GAP",
                "Candidate empty pixels were inferred between RGB color and W/V fill.",
                layer.layerIndex,
                layer.colorFillGapPixels));
        }
        if (layer.modelSupportGapPixels > 0)
        {
            diagnostics.emplace_back(BuildGapDiagnostic(
                "MODEL_SUPPORT_GAP",
                "Candidate empty pixels were inferred between model material and support.",
                layer.layerIndex,
                layer.modelSupportGapPixels));
        }
        if (layer.colorSupportGapPixels > 0)
        {
            diagnostics.emplace_back(BuildGapDiagnostic(
                "COLOR_SUPPORT_GAP",
                "Candidate empty pixels were inferred between RGB color and support.",
                layer.layerIndex,
                layer.colorSupportGapPixels));
        }
        if (layer.gapPixels > 0)
        {
            worstLayers.push_back(layer);
        }
    }

    diagnostics.insert(diagnostics.begin(), BuildCandidateOnlyDiagnostic(totalGapPixels));
    std::stable_sort(
        worstLayers.begin(),
        worstLayers.end(),
        [](const MaterialClosureCandidateLayer& first, const MaterialClosureCandidateLayer& second)
        {
            if (first.gapPixels != second.gapPixels)
            {
                return first.gapPixels > second.gapPixels;
            }
            return first.layerIndex < second.layerIndex;
        });

    Json::Array worstLayerItems;
    const std::size_t worstLayerCount = std::min<std::size_t>(20U, worstLayers.size());
    worstLayerItems.reserve(worstLayerCount);
    for (std::size_t index{0U}; index < worstLayerCount; ++index)
    {
        const MaterialClosureCandidateLayer& layer = worstLayers.at(index);
        worstLayerItems.emplace_back(Json::object({
            {"layerIndex", layer.layerIndex},
            {"zMm", layer.zMm},
            {"gapPixels", layer.gapPixels},
            {"types", Json{BuildGapTypes(layer)}},
        }));
    }

    return Json::object({
        {"schema", "p0.material_closure.1"},
        {"packageProtocol", "p0.rgbwsv.2"},
        {"enabled", true},
        {"mode", config.mode},
        {"source", "rgbwsv_tiff_inferred"},
        {"confidence", "candidate"},
        {"closureStatus", "warning"},
        {"productionAcceptance", "not_evaluated"},
        {"repair",
         Json::object({
             {"enabled", config.repair.enabled},
             {"maxGapPx", config.max_gap_px},
             {"attempted", false},
             {"repairedPixels", 0},
         })},
        {"totals",
         Json::object({
             {"layerCount", static_cast<std::uint64_t>(layers.size())},
             {"evaluatedLayerCount", static_cast<std::uint64_t>(layers.size())},
             {"passLayerCount", 0},
             {"warningLayerCount", static_cast<std::uint64_t>(layers.size())},
             {"failLayerCount", 0},
             {"totalGapPixels", totalGapPixels},
             {"colorFillGapPixels", colorFillGapPixels},
             {"modelSupportGapPixels", modelSupportGapPixels},
             {"colorSupportGapPixels", colorSupportGapPixels},
             {"internalVoidGapPixels", 0},
             {"varnishSupportGapPixels", 0},
             {"repairedPixels", 0},
             {"externalBackgroundProtectedPixels", externalBackgroundProtectedPixels},
         })},
        {"worstLayers", Json{std::move(worstLayerItems)}},
        {"layers", Json{std::move(layerItems)}},
        {"diagnostics", Json{std::move(diagnostics)}},
    });
}

Json BuildMaterialClosureSliceSummary(const Json& report)
{
    const Json& totals = report.at("totals");
    const Json& worstLayers = report.at("worstLayers");
    const Json worstLayerIndex = worstLayers.size() == 0U
        ? Json{nullptr}
        : Json{worstLayers.at(0).at("layerIndex")};

    return Json::object({
        {"schema", report.at("schema")},
        {"closureStatus", report.at("closureStatus")},
        {"confidence", report.at("confidence")},
        {"totalGapPixels", totals.at("totalGapPixels")},
        {"repairedPixels", totals.at("repairedPixels")},
        {"worstLayerIndex", worstLayerIndex},
        {"reportPath", "reports/material_closure_report.json"},
    });
}

}  // namespace slicer_core
