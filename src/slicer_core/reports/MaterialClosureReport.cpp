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

struct ReportLayerEvidence
{
    int layerIndex{0};
    double zMm{0.0};
    int gapPixels{0};
    int colorFillGapPixels{0};
    int modelSupportGapPixels{0};
    int colorSupportGapPixels{0};
    int internalVoidGapPixels{0};
    int varnishSupportGapPixels{0};
    int externalBackgroundProtectedPixels{0};
};

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
    const std::string& severity,
    const std::string& code,
    const std::string& message,
    const int layerIndex,
    const int pixelCount)
{
    return Json::object({
        {"severity", severity},
        {"code", code},
        {"message", message},
        {"layerIndex", layerIndex},
        {"pixelCount", pixelCount},
    });
}

Json::Array BuildGapTypes(const ReportLayerEvidence& layer)
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
    if (layer.internalVoidGapPixels > 0)
    {
        types.emplace_back("INTERNAL_VOID_GAP");
    }
    if (layer.varnishSupportGapPixels > 0)
    {
        types.emplace_back("VARNISH_SUPPORT_GAP");
    }
    return types;
}

void ValidateLayerEvidence(const ReportLayerEvidence& layer)
{
    if (layer.layerIndex < 0 || layer.gapPixels < 0 || layer.colorFillGapPixels < 0
        || layer.modelSupportGapPixels < 0 || layer.colorSupportGapPixels < 0
        || layer.internalVoidGapPixels < 0 || layer.varnishSupportGapPixels < 0
        || layer.externalBackgroundProtectedPixels < 0)
    {
        throw std::invalid_argument("material closure report contains negative values");
    }
}

std::string ResolveLayerStatus(
    const MaterialClosureConfig& config,
    const ReportLayerEvidence& layer,
    const bool candidateOnly)
{
    if (candidateOnly)
    {
        return "warning";
    }
    if (layer.gapPixels == 0)
    {
        return "pass";
    }
    return config.fail_on_gap ? "fail" : "warning";
}

void AppendLayerDiagnostics(
    const MaterialClosureConfig& config,
    const ReportLayerEvidence& layer,
    const bool candidateOnly,
    Json::Array& diagnostics)
{
    const std::string severity = candidateOnly || !config.fail_on_gap ? "warning" : "error";
    const std::string evidence = candidateOnly ? "Candidate" : "Exact semantic";
    if (layer.colorFillGapPixels > 0)
    {
        diagnostics.emplace_back(BuildGapDiagnostic(
            severity,
            "COLOR_FILL_GAP",
            evidence + " empty pixels were detected between color and model fill.",
            layer.layerIndex,
            layer.colorFillGapPixels));
    }
    if (layer.modelSupportGapPixels > 0)
    {
        diagnostics.emplace_back(BuildGapDiagnostic(
            severity,
            "MODEL_SUPPORT_GAP",
            evidence + " empty pixels were detected between model material and support.",
            layer.layerIndex,
            layer.modelSupportGapPixels));
    }
    if (layer.colorSupportGapPixels > 0)
    {
        diagnostics.emplace_back(BuildGapDiagnostic(
            severity,
            "COLOR_SUPPORT_GAP",
            evidence + " empty pixels were detected between color and support.",
            layer.layerIndex,
            layer.colorSupportGapPixels));
    }
    if (layer.internalVoidGapPixels > 0)
    {
        diagnostics.emplace_back(BuildGapDiagnostic(
            severity,
            "INTERNAL_VOID_GAP",
            "Exact semantic empty pixels were detected inside the model envelope.",
            layer.layerIndex,
            layer.internalVoidGapPixels));
    }
    if (layer.varnishSupportGapPixels > 0)
    {
        diagnostics.emplace_back(BuildGapDiagnostic(
            severity,
            "VARNISH_SUPPORT_GAP",
            "Exact semantic empty pixels were detected between outer varnish and required support.",
            layer.layerIndex,
            layer.varnishSupportGapPixels));
    }
}

Json BuildDetectedReport(
    const MaterialClosureConfig& config,
    const std::vector<ReportLayerEvidence>& layers,
    const bool candidateOnly)
{
    if (!config.enabled)
    {
        return BuildMaterialClosureReportSkeleton(config, static_cast<int>(layers.size()));
    }

    std::uint64_t totalGapPixels{0U};
    std::uint64_t colorFillGapPixels{0U};
    std::uint64_t modelSupportGapPixels{0U};
    std::uint64_t colorSupportGapPixels{0U};
    std::uint64_t internalVoidGapPixels{0U};
    std::uint64_t varnishSupportGapPixels{0U};
    std::uint64_t externalBackgroundProtectedPixels{0U};
    std::uint64_t passLayerCount{0U};
    std::uint64_t warningLayerCount{0U};
    std::uint64_t failLayerCount{0U};
    Json::Array layerItems;
    Json::Array diagnostics;
    std::vector<ReportLayerEvidence> worstLayers;

    layerItems.reserve(layers.size());
    for (const ReportLayerEvidence& layer : layers)
    {
        ValidateLayerEvidence(layer);
        totalGapPixels += static_cast<std::uint64_t>(layer.gapPixels);
        colorFillGapPixels += static_cast<std::uint64_t>(layer.colorFillGapPixels);
        modelSupportGapPixels += static_cast<std::uint64_t>(layer.modelSupportGapPixels);
        colorSupportGapPixels += static_cast<std::uint64_t>(layer.colorSupportGapPixels);
        internalVoidGapPixels += static_cast<std::uint64_t>(layer.internalVoidGapPixels);
        varnishSupportGapPixels += static_cast<std::uint64_t>(layer.varnishSupportGapPixels);
        externalBackgroundProtectedPixels +=
            static_cast<std::uint64_t>(layer.externalBackgroundProtectedPixels);

        const std::string layerStatus = ResolveLayerStatus(config, layer, candidateOnly);
        passLayerCount += layerStatus == "pass" ? 1U : 0U;
        warningLayerCount += layerStatus == "warning" ? 1U : 0U;
        failLayerCount += layerStatus == "fail" ? 1U : 0U;

        layerItems.emplace_back(Json::object({
            {"layerIndex", layer.layerIndex},
            {"zMm", layer.zMm},
            {"closureStatus", layerStatus},
            {"gapPixels", layer.gapPixels},
            {"gaps",
             Json::object({
                 {"colorFill", layer.colorFillGapPixels},
                 {"modelSupport", layer.modelSupportGapPixels},
                 {"colorSupport", layer.colorSupportGapPixels},
                 {"internalVoid", layer.internalVoidGapPixels},
                 {"varnishSupport", layer.varnishSupportGapPixels},
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

        AppendLayerDiagnostics(config, layer, candidateOnly, diagnostics);
        if (layer.gapPixels > 0)
        {
            worstLayers.push_back(layer);
        }
    }

    if (candidateOnly)
    {
        diagnostics.insert(diagnostics.begin(), BuildCandidateOnlyDiagnostic(totalGapPixels));
    }
    std::stable_sort(
        worstLayers.begin(),
        worstLayers.end(),
        [](const ReportLayerEvidence& first, const ReportLayerEvidence& second)
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
        const ReportLayerEvidence& layer = worstLayers.at(index);
        worstLayerItems.emplace_back(Json::object({
            {"layerIndex", layer.layerIndex},
            {"zMm", layer.zMm},
            {"gapPixels", layer.gapPixels},
            {"types", Json{BuildGapTypes(layer)}},
        }));
    }

    const bool hasGaps = totalGapPixels > 0U;
    const std::string closureStatus = candidateOnly
        ? "warning"
        : (hasGaps ? (config.fail_on_gap ? "fail" : "warning") : "pass");
    const std::string productionAcceptance = candidateOnly
        ? "not_evaluated"
        : (hasGaps ? "failed" : "passed");

    return Json::object({
        {"schema", "p0.material_closure.1"},
        {"packageProtocol", "p0.rgbwsv.2"},
        {"enabled", true},
        {"mode", config.mode},
        {"source", candidateOnly ? "rgbwsv_tiff_inferred" : "semantic_masks"},
        {"confidence", candidateOnly ? "candidate" : "exact"},
        {"closureStatus", closureStatus},
        {"productionAcceptance", productionAcceptance},
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
             {"passLayerCount", passLayerCount},
             {"warningLayerCount", warningLayerCount},
             {"failLayerCount", failLayerCount},
             {"totalGapPixels", totalGapPixels},
             {"colorFillGapPixels", colorFillGapPixels},
             {"modelSupportGapPixels", modelSupportGapPixels},
             {"colorSupportGapPixels", colorSupportGapPixels},
             {"internalVoidGapPixels", internalVoidGapPixels},
             {"varnishSupportGapPixels", varnishSupportGapPixels},
             {"repairedPixels", 0},
             {"externalBackgroundProtectedPixels", externalBackgroundProtectedPixels},
         })},
        {"worstLayers", Json{std::move(worstLayerItems)}},
        {"layers", Json{std::move(layerItems)}},
        {"diagnostics", Json{std::move(diagnostics)}},
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

Json BuildMaterialClosureCandidateReport(
    const MaterialClosureConfig& config,
    const std::vector<MaterialClosureCandidateLayer>& layers)
{
    std::vector<ReportLayerEvidence> evidence;
    evidence.reserve(layers.size());
    for (const MaterialClosureCandidateLayer& layer : layers)
    {
        evidence.push_back(ReportLayerEvidence{
            layer.layerIndex,
            layer.zMm,
            layer.gapPixels,
            layer.colorFillGapPixels,
            layer.modelSupportGapPixels,
            layer.colorSupportGapPixels,
            0,
            0,
            layer.externalBackgroundProtectedPixels,
        });
    }
    return BuildDetectedReport(config, evidence, true);
}

Json BuildMaterialClosureExactReport(
    const MaterialClosureConfig& config,
    const std::vector<MaterialClosureSemanticLayerResult>& layers)
{
    std::vector<ReportLayerEvidence> evidence;
    evidence.reserve(layers.size());
    for (const MaterialClosureSemanticLayerResult& layer : layers)
    {
        evidence.push_back(ReportLayerEvidence{
            layer.layerIndex,
            layer.zMm,
            layer.gapPixels,
            layer.colorFillGapPixels,
            layer.modelSupportGapPixels,
            layer.colorSupportGapPixels,
            layer.internalVoidGapPixels,
            layer.varnishSupportGapPixels,
            layer.externalBackgroundProtectedPixels,
        });
    }
    return BuildDetectedReport(config, evidence, false);
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
