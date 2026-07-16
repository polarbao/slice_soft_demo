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
    bool repairAttempted{false};
    int repairedPixels{0};
    int repairedColorFillPixels{0};
    int repairedModelSupportPixels{0};
    int repairedInternalVoidPixels{0};
    int repairedVarnishSupportPixels{0};
    int remainingGapPixels{0};
    int remainingColorFillGapPixels{0};
    int remainingModelSupportGapPixels{0};
    int remainingColorSupportGapPixels{0};
    int remainingInternalVoidGapPixels{0};
    int remainingVarnishSupportGapPixels{0};
    int repairRejectedTooWidePixels{0};
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
        || layer.externalBackgroundProtectedPixels < 0 || layer.repairedPixels < 0
        || layer.repairedColorFillPixels < 0 || layer.repairedModelSupportPixels < 0
        || layer.repairedInternalVoidPixels < 0 || layer.repairedVarnishSupportPixels < 0
        || layer.remainingGapPixels < 0 || layer.remainingColorFillGapPixels < 0
        || layer.remainingModelSupportGapPixels < 0 || layer.remainingColorSupportGapPixels < 0
        || layer.remainingInternalVoidGapPixels < 0 || layer.remainingVarnishSupportGapPixels < 0
        || layer.repairRejectedTooWidePixels < 0)
    {
        throw std::invalid_argument("material closure report contains negative values");
    }
}

ReportLayerEvidence ResolveRemainingEvidence(const ReportLayerEvidence& layer)
{
    if (!layer.repairAttempted)
    {
        return layer;
    }
    ReportLayerEvidence remaining = layer;
    remaining.gapPixels = layer.remainingGapPixels;
    remaining.colorFillGapPixels = layer.remainingColorFillGapPixels;
    remaining.modelSupportGapPixels = layer.remainingModelSupportGapPixels;
    remaining.colorSupportGapPixels = layer.remainingColorSupportGapPixels;
    remaining.internalVoidGapPixels = layer.remainingInternalVoidGapPixels;
    remaining.varnishSupportGapPixels = layer.remainingVarnishSupportGapPixels;
    return remaining;
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
    std::uint64_t repairedPixels{0U};
    std::uint64_t repairedColorFillPixels{0U};
    std::uint64_t repairedModelSupportPixels{0U};
    std::uint64_t repairedInternalVoidPixels{0U};
    std::uint64_t repairedVarnishSupportPixels{0U};
    std::uint64_t remainingGapPixels{0U};
    std::uint64_t repairRejectedTooWidePixels{0U};
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
        repairedPixels += static_cast<std::uint64_t>(layer.repairedPixels);
        repairedColorFillPixels += static_cast<std::uint64_t>(layer.repairedColorFillPixels);
        repairedModelSupportPixels += static_cast<std::uint64_t>(layer.repairedModelSupportPixels);
        repairedInternalVoidPixels += static_cast<std::uint64_t>(layer.repairedInternalVoidPixels);
        repairedVarnishSupportPixels += static_cast<std::uint64_t>(layer.repairedVarnishSupportPixels);
        repairRejectedTooWidePixels +=
            static_cast<std::uint64_t>(layer.repairRejectedTooWidePixels);

        const ReportLayerEvidence remaining = ResolveRemainingEvidence(layer);
        remainingGapPixels += static_cast<std::uint64_t>(remaining.gapPixels);
        const std::string layerStatus = ResolveLayerStatus(config, remaining, candidateOnly);
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
                 {"attempted", !candidateOnly && layer.repairAttempted},
                 {"repairedPixels", layer.repairedPixels},
                 {"repairedColorFillPixels", layer.repairedColorFillPixels},
                 {"repairedModelSupportPixels", layer.repairedModelSupportPixels},
                 {"repairedInternalVoidPixels", layer.repairedInternalVoidPixels},
                 {"repairedVarnishSupportPixels", layer.repairedVarnishSupportPixels},
                 {"remainingGapPixels", remaining.gapPixels},
                 {"rejectedTooWidePixels", layer.repairRejectedTooWidePixels},
             })},
            {"externalBackgroundProtectedPixels", layer.externalBackgroundProtectedPixels},
            {"gapPreviewPath", ""},
        }));

        AppendLayerDiagnostics(config, remaining, candidateOnly, diagnostics);
        if (layer.repairRejectedTooWidePixels > 0)
        {
            diagnostics.emplace_back(BuildGapDiagnostic(
                config.fail_on_gap ? "error" : "warning",
                "REPAIR_GAP_TOO_WIDE",
                "Material closure repair rejected a component wider than one pixel.",
                layer.layerIndex,
                layer.repairRejectedTooWidePixels));
        }
        if (remaining.gapPixels > 0)
        {
            worstLayers.push_back(remaining);
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

    const bool hasGaps = remainingGapPixels > 0U;
    const bool repairAttempted = !candidateOnly
        && config.mode == "repair_then_report"
        && config.repair.enabled;
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
             {"attempted", repairAttempted},
             {"repairedPixels", repairedPixels},
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
             {"repairedPixels", repairedPixels},
             {"repairedColorFillPixels", repairedColorFillPixels},
             {"repairedModelSupportPixels", repairedModelSupportPixels},
             {"repairedInternalVoidPixels", repairedInternalVoidPixels},
             {"repairedVarnishSupportPixels", repairedVarnishSupportPixels},
             {"remainingGapPixels", remainingGapPixels},
             {"repairRejectedTooWidePixels", repairRejectedTooWidePixels},
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
             {"repairedColorFillPixels", 0},
             {"repairedModelSupportPixels", 0},
             {"repairedInternalVoidPixels", 0},
             {"repairedVarnishSupportPixels", 0},
             {"remainingGapPixels", 0},
             {"repairRejectedTooWidePixels", 0},
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
            false,
            0,
            0,
            0,
            0,
            0,
            layer.gapPixels,
            layer.colorFillGapPixels,
            layer.modelSupportGapPixels,
            layer.colorSupportGapPixels,
            0,
            0,
            0,
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
            layer.repairAttempted,
            layer.repairedPixels,
            layer.repairedColorFillPixels,
            layer.repairedModelSupportPixels,
            layer.repairedInternalVoidPixels,
            layer.repairedVarnishSupportPixels,
            layer.remainingGapPixels,
            layer.remainingColorFillGapPixels,
            layer.remainingModelSupportGapPixels,
            layer.remainingColorSupportGapPixels,
            layer.remainingInternalVoidGapPixels,
            layer.remainingVarnishSupportGapPixels,
            layer.repairRejectedTooWidePixels,
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
        {"remainingGapPixels", totals.at("remainingGapPixels")},
        {"worstLayerIndex", worstLayerIndex},
        {"reportPath", "reports/material_closure_report.json"},
    });
}

}  // namespace slicer_core
