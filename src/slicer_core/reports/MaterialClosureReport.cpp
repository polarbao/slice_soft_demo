#include "slicer_core/reports/MaterialClosureReport.h"

#include <stdexcept>
#include <utility>

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
