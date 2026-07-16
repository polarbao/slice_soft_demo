#include "slicer_core/config.h"
#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/reports/MaterialClosureReport.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool EnabledSkeletonReportsUnavailableEvidence()
{
    slicer_core::MaterialClosureConfig config;
    const slicer_core::Json report = slicer_core::BuildMaterialClosureReportSkeleton(config, 17);

    return ExpectTrue(report.at("schema").as_string() == "p0.material_closure.1", "report schema")
        && ExpectTrue(report.at("packageProtocol").as_string() == "p0.rgbwsv.2", "package protocol")
        && ExpectTrue(report.at("enabled").as_bool(), "material closure enabled")
        && ExpectTrue(report.at("mode").as_string() == "diagnostic", "diagnostic mode")
        && ExpectTrue(report.at("source").as_string() == "unavailable", "source unavailable before detector")
        && ExpectTrue(report.at("confidence").as_string() == "unavailable", "confidence unavailable")
        && ExpectTrue(report.at("closureStatus").as_string() == "not_available", "status not available")
        && ExpectTrue(
            report.at("productionAcceptance").as_string() == "not_evaluated",
            "production acceptance not evaluated")
        && ExpectTrue(!report.at("repair").at("enabled").as_bool(), "repair disabled")
        && ExpectTrue(!report.at("repair").at("attempted").as_bool(), "repair not attempted")
        && ExpectTrue(report.at("repair").at("repairedPixels").as_int() == 0, "repair pixels zero")
        && ExpectTrue(report.at("totals").at("layerCount").as_int() == 17, "layer count retained")
        && ExpectTrue(report.at("totals").at("evaluatedLayerCount").as_int() == 0, "no evaluated layers")
        && ExpectTrue(report.at("totals").at("totalGapPixels").as_int() == 0, "gap pixels zero")
        && ExpectTrue(report.at("worstLayers").size() == 0U, "worst layers empty")
        && ExpectTrue(report.at("layers").size() == 0U, "layers empty")
        && ExpectTrue(report.at("diagnostics").size() == 1U, "source unavailable diagnostic emitted")
        && ExpectTrue(
            report.at("diagnostics").at(0).at("code").as_string()
                == "MATERIAL_CLOSURE_SOURCE_UNAVAILABLE",
            "stable unavailable code");
}

bool DisabledSkeletonHasNoFalseFailure()
{
    slicer_core::MaterialClosureConfig config;
    config.enabled = false;
    const slicer_core::Json report = slicer_core::BuildMaterialClosureReportSkeleton(config, 3);

    return ExpectTrue(!report.at("enabled").as_bool(), "disabled state retained")
        && ExpectTrue(report.at("closureStatus").as_string() == "not_available", "disabled status not available")
        && ExpectTrue(
            report.at("productionAcceptance").as_string() == "not_evaluated",
            "disabled production not evaluated")
        && ExpectTrue(report.at("diagnostics").size() == 0U, "disabled report has no false warning");
}

bool SliceSummaryReferencesCanonicalReport()
{
    slicer_core::MaterialClosureConfig config;
    const slicer_core::Json report = slicer_core::BuildMaterialClosureReportSkeleton(config, 5);
    const slicer_core::Json summary = slicer_core::BuildMaterialClosureSliceSummary(report);

    return ExpectTrue(summary.at("schema").as_string() == "p0.material_closure.1", "summary schema")
        && ExpectTrue(summary.at("closureStatus").as_string() == "not_available", "summary status")
        && ExpectTrue(summary.at("confidence").as_string() == "unavailable", "summary confidence")
        && ExpectTrue(summary.at("totalGapPixels").as_int() == 0, "summary gap pixels")
        && ExpectTrue(summary.at("repairedPixels").as_int() == 0, "summary repaired pixels")
        && ExpectTrue(
            summary.at("reportPath").as_string() == "reports/material_closure_report.json",
            "summary report path");
}

bool CandidateReportCannotProduceProductionPass()
{
    slicer_core::MaterialClosureConfig config;
    std::vector<slicer_core::MaterialClosureCandidateLayer> layers(2);
    layers.at(0).layerIndex = 0;
    layers.at(0).zMm = 0.01;
    layers.at(0).gapPixels = 1;
    layers.at(0).colorFillGapPixels = 1;
    layers.at(0).externalBackgroundProtectedPixels = 12;
    layers.at(1).layerIndex = 1;
    layers.at(1).zMm = 0.02;
    layers.at(1).gapPixels = 1;
    layers.at(1).modelSupportGapPixels = 1;
    layers.at(1).colorSupportGapPixels = 1;
    layers.at(1).externalBackgroundProtectedPixels = 10;

    const slicer_core::Json report = slicer_core::BuildMaterialClosureCandidateReport(config, layers);
    const slicer_core::Json summary = slicer_core::BuildMaterialClosureSliceSummary(report);

    return ExpectTrue(report.at("source").as_string() == "rgbwsv_tiff_inferred", "candidate source")
        && ExpectTrue(report.at("confidence").as_string() == "candidate", "candidate confidence")
        && ExpectTrue(report.at("closureStatus").as_string() == "warning", "candidate is never pass")
        && ExpectTrue(
            report.at("productionAcceptance").as_string() == "not_evaluated",
            "candidate production acceptance not evaluated")
        && ExpectTrue(!report.at("repair").at("attempted").as_bool(), "candidate repair not attempted")
        && ExpectTrue(report.at("totals").at("evaluatedLayerCount").as_int() == 2, "all layers evaluated")
        && ExpectTrue(report.at("totals").at("warningLayerCount").as_int() == 2, "all layers warning")
        && ExpectTrue(report.at("totals").at("totalGapPixels").as_int() == 2, "union totals")
        && ExpectTrue(report.at("totals").at("colorFillGapPixels").as_int() == 1, "color-fill totals")
        && ExpectTrue(report.at("totals").at("modelSupportGapPixels").as_int() == 1, "model-support totals")
        && ExpectTrue(report.at("totals").at("colorSupportGapPixels").as_int() == 1, "color-support totals")
        && ExpectTrue(report.at("totals").at("externalBackgroundProtectedPixels").as_int() == 22, "background totals")
        && ExpectTrue(report.at("worstLayers").size() == 2U, "worst layers retained")
        && ExpectTrue(report.at("diagnostics").at(0).at("code").as_string() == "MATERIAL_CLOSURE_CANDIDATE_ONLY", "candidate diagnostic")
        && ExpectTrue(summary.at("closureStatus").as_string() == "warning", "summary warning")
        && ExpectTrue(summary.at("confidence").as_string() == "candidate", "summary candidate")
        && ExpectTrue(summary.at("worstLayerIndex").as_int() == 0, "stable worst layer ordering");
}

bool ExactReportPassesOnlyWithoutGaps()
{
    slicer_core::MaterialClosureConfig config;
    std::vector<slicer_core::MaterialClosureSemanticLayerResult> layers(1);
    layers.at(0).layerIndex = 4;
    layers.at(0).zMm = 0.04;
    layers.at(0).externalBackgroundProtectedPixels = 20;

    const slicer_core::Json report = slicer_core::BuildMaterialClosureExactReport(config, layers);
    return ExpectTrue(report.at("source").as_string() == "semantic_masks", "exact source")
        && ExpectTrue(report.at("confidence").as_string() == "exact", "exact confidence")
        && ExpectTrue(report.at("closureStatus").as_string() == "pass", "gap-free exact report passes")
        && ExpectTrue(report.at("productionAcceptance").as_string() == "passed", "gap-free production accepted")
        && ExpectTrue(!report.at("repair").at("attempted").as_bool(), "exact diagnostic does not repair")
        && ExpectTrue(report.at("totals").at("passLayerCount").as_int() == 1, "pass layer counted")
        && ExpectTrue(report.at("totals").at("totalGapPixels").as_int() == 0, "no exact gaps")
        && ExpectTrue(report.at("diagnostics").size() == 0U, "gap-free exact report has no diagnostics");
}

bool ExactReportFailsWithStableGapDiagnostics()
{
    slicer_core::MaterialClosureConfig config;
    std::vector<slicer_core::MaterialClosureSemanticLayerResult> layers(2);
    layers.at(0).layerIndex = 2;
    layers.at(0).zMm = 0.02;
    layers.at(0).gapPixels = 1;
    layers.at(0).internalVoidGapPixels = 1;
    layers.at(1).layerIndex = 3;
    layers.at(1).zMm = 0.03;
    layers.at(1).gapPixels = 1;
    layers.at(1).varnishSupportGapPixels = 1;

    const slicer_core::Json report = slicer_core::BuildMaterialClosureExactReport(config, layers);
    return ExpectTrue(report.at("closureStatus").as_string() == "fail", "exact gaps fail by default")
        && ExpectTrue(report.at("productionAcceptance").as_string() == "failed", "exact gaps reject production")
        && ExpectTrue(report.at("totals").at("failLayerCount").as_int() == 2, "fail layers counted")
        && ExpectTrue(report.at("totals").at("internalVoidGapPixels").as_int() == 1, "internal gap total")
        && ExpectTrue(report.at("totals").at("varnishSupportGapPixels").as_int() == 1, "varnish gap total")
        && ExpectTrue(report.at("worstLayers").at(0).at("layerIndex").as_int() == 2, "worst layers stably ordered")
        && ExpectTrue(report.at("diagnostics").at(0).at("severity").as_string() == "error", "exact fail diagnostic severity");
}

bool ExactReportCanWarnWithoutAcceptingGaps()
{
    slicer_core::MaterialClosureConfig config;
    config.fail_on_gap = false;
    std::vector<slicer_core::MaterialClosureSemanticLayerResult> layers(1);
    layers.at(0).layerIndex = 1;
    layers.at(0).gapPixels = 1;
    layers.at(0).colorFillGapPixels = 1;

    const slicer_core::Json report = slicer_core::BuildMaterialClosureExactReport(config, layers);
    return ExpectTrue(report.at("closureStatus").as_string() == "warning", "failOnGap false warns")
        && ExpectTrue(report.at("productionAcceptance").as_string() == "failed", "warning gaps still reject production")
        && ExpectTrue(report.at("totals").at("warningLayerCount").as_int() == 1, "warning layer counted")
        && ExpectTrue(report.at("diagnostics").at(0).at("severity").as_string() == "warning", "warning diagnostic severity");
}

bool NegativeLayerCountIsRejected()
{
    slicer_core::MaterialClosureConfig config;
    try
    {
        (void)slicer_core::BuildMaterialClosureReportSkeleton(config, -1);
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    return ExpectTrue(false, "negative layer count must be rejected");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"enabled_skeleton_reports_unavailable_evidence", EnabledSkeletonReportsUnavailableEvidence},
        {"disabled_skeleton_has_no_false_failure", DisabledSkeletonHasNoFalseFailure},
        {"slice_summary_references_canonical_report", SliceSummaryReferencesCanonicalReport},
        {"candidate_report_cannot_produce_production_pass", CandidateReportCannotProduceProductionPass},
        {"exact_report_passes_only_without_gaps", ExactReportPassesOnlyWithoutGaps},
        {"exact_report_fails_with_stable_gap_diagnostics", ExactReportFailsWithStableGapDiagnostics},
        {"exact_report_can_warn_without_accepting_gaps", ExactReportCanWarnWithoutAcceptingGaps},
        {"negative_layer_count_is_rejected", NegativeLayerCountIsRejected},
    };

    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        try
        {
            if (!test.second())
            {
                return 1;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << test.first << " exception=" << error.what() << '\n';
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }

    std::cout << "Material closure report unit tests complete.\n";
    return 0;
}
