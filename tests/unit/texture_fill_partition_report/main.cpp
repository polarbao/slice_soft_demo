#include "slicer_core/config.h"
#include "slicer_core/diagnostics/TextureFillPartitionClosureAdapter.h"
#include "slicer_core/json_value.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTextureTransfer.h"
#include "slicer_core/pipeline/TextureFillPartitionDiagnosticComposer.h"
#include "slicer_core/raster/TextureFillPartitionRasterMapper.h"
#include "slicer_core/reports/TextureFillPartitionReport.h"

#include <filesystem>
#include <fstream>
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

slicer_core::Json LoadGolden(const std::string& fileName)
{
    const std::filesystem::path path =
        std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "tests" / "golden" / "expected" / fileName;
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("failed to open golden: " + path.string());
    }
    return slicer_core::Json::parse(input);
}

slicer_core::SliceConfig MakeConfig()
{
    slicer_core::SliceConfig config;
    config.texture.enabled = true;
    config.texture.apply_mode = "global_surface_shell";
    config.texture.surface_shell.width_mm = 0.10;
    config.texture.surface_shell.width_step_mm = 0.01;
    config.model_fill.enabled = true;
    config.model_fill.material = "white";
    config.model_fill.scope = "complement_of_global_texture_shell";
    return config;
}

slicer_core::GlobalTextureFillPartitionResult MakeResult()
{
    slicer_core::GlobalTextureFillPartitionResult result;
    result.available = true;
    result.partitionPass = true;
    result.status = "diagnostic";
    result.backend = "report_fixture";
    result.backendRole = "conformance_candidate";
    result.options.requestedWidthMm = 0.10;
    result.grid.width = 2;
    result.grid.height = 1;
    result.grid.depth = 2;
    result.grid.originXMm = 0.0;
    result.grid.originYMm = 0.0;
    result.grid.originZMm = 0.0;
    result.grid.spacingXMm = 0.05;
    result.grid.spacingYMm = 0.05;
    result.grid.spacingZMm = 0.01;
    result.modelMask.grid = result.grid;
    result.textureSurfaceMask.grid = result.grid;
    result.modelFillMask.grid = result.grid;
    result.modelMask.values = {1U, 1U, 1U, 0U};
    result.textureSurfaceMask.values = {1U, 0U, 0U, 0U};
    result.modelFillMask.values = {0U, 1U, 1U, 0U};
    result.stats.modelVoxels = 3U;
    result.stats.textureSurfaceVoxels = 1U;
    result.stats.modelFillVoxels = 2U;
    result.widthMetrics.classificationResolutionMm = 0.05;
    result.widthMetrics.epsilonMm = 1.0e-9;
    result.widthMetrics.effectiveMinimumWidthMm = 0.10;
    result.widthMetrics.effectiveWidthMm = 0.10;
    result.widthMetrics.maxInteriorDistanceMm = 0.20;
    result.widthMetrics.allTextureThresholdMm = 0.20;
    result.performance.topologyMs = 1.0;
    result.performance.levelSetMs = 2.0;
    result.performance.gridSampleMs = 3.0;
    result.performance.occupancyBuildMs = 5.0;
    result.performance.distanceQueryMs = 7.0;
    result.performance.partitionMs = 11.0;
    result.performance.totalCoreMs = 24.0;
    result.performance.gridVoxelCount = 4U;
    result.performance.maskBytes = 12U;
    result.performance.closestReferenceBytes = 96U;
    result.performance.openVdbGridBytes = 128U;
    result.queryStats.sdfSampleCount = 4U;
    return result;
}

slicer_core::Json MakeStableReportProjection(const slicer_core::Json& report)
{
    return slicer_core::Json::object({
        {"schema", report.at("schema")},
        {"packageProtocol", report.at("packageProtocol")},
        {"availability", report.at("availability")},
        {"status", report.at("status")},
        {"productionAcceptance", report.at("productionAcceptance")},
        {"backend", report.at("backend")},
        {"backendRole", report.at("backendRole")},
        {"grid", report.at("grid")},
        {"width", report.at("width")},
        {"partition", report.at("partition")},
        {"performance", report.at("performance")},
        {"layers", report.at("layers")},
    });
}

bool SuccessReportMatchesGolden()
{
    const slicer_core::Json report = slicer_core::BuildTextureFillPartitionReport(
        MakeConfig(),
        MakeResult());
    const slicer_core::Json expected = LoadGolden(
        "12e_texture_fill_partition_report_schema.json");
    return ExpectTrue(
               MakeStableReportProjection(report).dump(2) == expected.dump(2),
               "success report matches stable golden")
        && ExpectTrue(report.at("layers").size() == 2U, "report has two real grid layers")
        && ExpectTrue(
               report.at("layers").at(0).at("layerIndex").as_int() == 0,
               "layers use true ascending indices")
        && ExpectTrue(
               report.at("layers").at(1).at("zMm").as_double() == 0.015,
               "layer Z uses grid cell center");
}

bool ConformanceIsOptionalAndDiagnostic()
{
    slicer_core::TextureFillPartitionConformanceResult conformance;
    conformance.cpuAvailable = true;
    conformance.openVdbAvailable = true;
    conformance.sameGrid = true;
    conformance.cpuPartitionInvariantPass = true;
    conformance.openVdbPartitionInvariantPass = true;
    conformance.cpuStatus = "diagnostic";
    conformance.openVdbStatus = "diagnostic";
    conformance.cpuBackendRole = "conformance_candidate";
    conformance.openVdbBackendRole = "conformance_candidate";
    conformance.conformanceStatus = "diagnostic";
    conformance.commonDistanceSamples = 3U;
    conformance.maxDistanceDeltaMm = 0.01;
    const slicer_core::Json without = slicer_core::BuildTextureFillPartitionReport(
        MakeConfig(),
        MakeResult());
    const slicer_core::Json with = slicer_core::BuildTextureFillPartitionReport(
        MakeConfig(),
        MakeResult(),
        &conformance);
    return ExpectTrue(!without.contains("conformance"), "conformance is omitted when absent")
        && ExpectTrue(with.contains("conformance"), "conformance is serialized when supplied")
        && ExpectTrue(
               with.at("conformance").at("productionAcceptance").as_string()
                   == "not_evaluated",
               "conformance never invents production acceptance");
}

bool UnavailableMeasurementsRemainNull()
{
    slicer_core::GlobalTextureFillPartitionResult result = MakeResult();
    result.performance.processMemoryAvailable = false;
    const slicer_core::Json report = slicer_core::BuildTextureFillPartitionReport(
        MakeConfig(),
        result);
    return ExpectTrue(
               !report.at("performance").at("workingSetBytes").is_number(),
               "unavailable working set is null")
        && ExpectTrue(
               !report.at("performance").at("peakWorkingSetBytes").is_number(),
               "unavailable peak working set is null")
        && ExpectTrue(
               !report.at("performance").at("textureTransferMs").is_number(),
               "unimplemented texture transfer timing is null")
        && ExpectTrue(
               !report.at("performance").at("rasterMappingMs").is_number(),
               "unimplemented raster mapping timing is null")
        && ExpectTrue(
               report.at("textureTransfer").at("availability").as_string()
                   == "unavailable",
               "missing texture transfer has explicit availability")
        && ExpectTrue(
               report.at("diagnosticComposer").at("channelOrder").size() == 6U,
               "missing diagnostic composer retains fixed channel order")
        && ExpectTrue(
               report.at("diagnosticComposer").at("emptyVoxels").as_int() == 0,
               "missing diagnostic composer retains complete counters")
        && ExpectTrue(
               report.at("rasterMapping").at("availability").as_string()
                   == "unavailable",
               "missing raster mapping has explicit availability");
}

bool LayerTotalsMatchPartitionTotals()
{
    const slicer_core::Json report = slicer_core::BuildTextureFillPartitionReport(
        MakeConfig(),
        MakeResult());
    std::uint64_t model{0U};
    std::uint64_t texture{0U};
    std::uint64_t fill{0U};
    for (const slicer_core::Json& layer : report.at("layers").as_array())
    {
        model += static_cast<std::uint64_t>(layer.at("modelVoxels").as_double());
        texture += static_cast<std::uint64_t>(layer.at("textureSurfaceVoxels").as_double());
        fill += static_cast<std::uint64_t>(layer.at("modelFillVoxels").as_double());
    }
    return ExpectTrue(model == 3U, "layer model totals match")
        && ExpectTrue(texture == 1U, "layer texture totals match")
        && ExpectTrue(fill == 2U, "layer fill totals match");
}

bool TransferAndComposerEvidenceIsSerialized()
{
    slicer_core::TextureFillPartitionTextureTransferResult transfer;
    transfer.available = true;
    transfer.status = "diagnostic";
    transfer.stats.textureSurfaceVoxels = 1U;
    transfer.stats.sampledTextureCount = 1U;
    transfer.stats.reusedReferenceCount = 1U;
    transfer.stats.maxTransferDistanceMm = 0.05;
    transfer.stats.loadedTextureCount = 1U;
    transfer.stats.transferMs = 2.5;

    slicer_core::TextureFillPartitionDiagnosticComposerResult composer;
    composer.available = true;
    composer.status = "diagnostic";
    composer.width = 2;
    composer.height = 1;
    composer.depth = 2;
    composer.layers.resize(2U);
    composer.stats.textureSurfaceVoxels = 1U;
    composer.stats.modelFillVoxels = 2U;
    composer.stats.modelFillWhiteVoxels = 2U;

    const slicer_core::Json report = slicer_core::BuildTextureFillPartitionReport(
        MakeConfig(),
        MakeResult(),
        nullptr,
        &transfer,
        &composer);
    const slicer_core::Json projection = slicer_core::Json::object({
        {"textureTransfer", report.at("textureTransfer")},
        {"diagnosticComposer", report.at("diagnosticComposer")},
        {"textureTransferMs", report.at("performance").at("textureTransferMs")},
    });
    const slicer_core::Json expected = LoadGolden(
        "12e_texture_transfer_diagnostic_composer.json");
    return ExpectTrue(
               projection.dump(2) == expected.dump(2),
               "texture transfer and diagnostic composer match golden")
        && ExpectTrue(
               report.at("textureTransfer").at("status").as_string()
                   == "diagnostic",
               "texture transfer status is serialized")
        && ExpectTrue(
               report.at("textureTransfer").at("reusedReferenceCount").as_int()
                   == 1,
               "reference reuse evidence is serialized")
        && ExpectTrue(
               report.at("performance").at("textureTransferMs").as_double()
                   == 2.5,
               "texture transfer timing is serialized")
        && ExpectTrue(
               report.at("diagnosticComposer").at("modelFillWhiteVoxels").as_int()
                   == 2,
               "diagnostic composer material count is serialized")
        && ExpectTrue(
               report.at("diagnosticComposer").at("supportPrintVoxels").as_int()
                   == 0,
               "diagnostic composer keeps support empty");
}

bool ClosureLinkageEvidenceIsSerialized()
{
    slicer_core::TextureFillPartitionClosureAdapterResult closure;
    closure.available = true;
    closure.status = "diagnostic";
    closure.source = "semantic_masks";
    closure.confidence = "exact";
    closure.allTexture = false;
    closure.colorFillApplicability = "applicable";
    closure.totalColorFillGapVoxels = 0U;
    closure.totalModelDomainGapVoxels = 0U;
    closure.layers.resize(2U);
    closure.layers.at(0).layerIndex = 0;
    closure.layers.at(0).zMm = 0.005;
    closure.layers.at(1).layerIndex = 1;
    closure.layers.at(1).zMm = 0.015;

    const slicer_core::Json report = slicer_core::BuildTextureFillPartitionReport(
        MakeConfig(),
        MakeResult(),
        nullptr,
        nullptr,
        nullptr,
        &closure);
    const slicer_core::Json expected = LoadGolden(
        "12e_texture_fill_partition_closure_linkage.json");
    return ExpectTrue(
               report.at("closureLinkage").dump(2) == expected.dump(2),
               "closure linkage matches golden")
        && ExpectTrue(
               report.at("closureLinkage").at("source").as_string()
                   == "semantic_masks",
               "closure source is exact semantic masks")
        && ExpectTrue(
               report.at("closureLinkage").at("supportClosureStatus").as_string()
                   == "not_evaluated",
               "support closure is not fabricated")
        && ExpectTrue(
               !report.at("closureLinkage").at("productionOutputWritten").as_bool(),
               "closure linkage writes no production output");
}

bool RasterMappingEvidenceIsSerialized()
{
    slicer_core::TextureFillPartitionRasterMappingResult mapping;
    mapping.available = true;
    mapping.status = "diagnostic";
    mapping.allTexture = false;
    mapping.grid.width = 2;
    mapping.grid.height = 1;
    mapping.grid.depth = 2;
    mapping.grid.originXMm = 0.0;
    mapping.grid.originYMm = 0.0;
    mapping.grid.originZMm = 0.0;
    mapping.grid.pixelPitchXMm = 0.05;
    mapping.grid.pixelPitchYMm = 0.05;
    mapping.grid.layerThicknessMm = 0.01;
    mapping.layers.resize(2U);
    mapping.layers.at(0).layerIndex = 0;
    mapping.layers.at(0).zMm = 0.005;
    mapping.layers.at(0).modelMask = {1U, 1U};
    mapping.layers.at(0).textureSurfaceMask = {1U, 0U};
    mapping.layers.at(0).modelFillMask = {0U, 1U};
    mapping.layers.at(1) = mapping.layers.at(0);
    mapping.layers.at(1).layerIndex = 1;
    mapping.layers.at(1).zMm = 0.015;
    mapping.stats.rasterVoxelCount = 4U;
    mapping.stats.mappedSourceGridVoxels = 4U;
    mapping.stats.uniqueSourceVoxelCount = 4U;
    mapping.stats.modelRasterVoxels = 4U;
    mapping.stats.textureSurfaceRasterVoxels = 2U;
    mapping.stats.modelFillRasterVoxels = 2U;
    mapping.stats.textureRgbRasterVoxels = 2U;
    mapping.stats.sourceModelCoverage = 1.0;
    mapping.stats.rasterModelCoverage = 1.0;
    mapping.stats.modelCoverageDelta = 0.0;
    mapping.stats.maxCenterQuantizationErrorMm = 0.02;
    mapping.stats.mappingMs = 1.25;
    mapping.stats.partitionPass = true;

    const slicer_core::Json report = slicer_core::BuildTextureFillPartitionReport(
        MakeConfig(),
        MakeResult(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &mapping);
    const slicer_core::Json expected = LoadGolden(
        "12e_texture_fill_partition_raster_mapping.json");
    return ExpectTrue(
               report.at("rasterMapping").dump(2) == expected.dump(2),
               "raster mapping matches golden")
        && ExpectTrue(
               report.at("performance").at("rasterMappingMs").as_double() == 1.25,
               "raster mapping timing is serialized")
        && ExpectTrue(
               !report.at("rasterMapping").at("productionOutputWritten").as_bool(),
               "raster mapping writes no production output");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"success_report_matches_golden", SuccessReportMatchesGolden},
        {"conformance_is_optional_and_diagnostic", ConformanceIsOptionalAndDiagnostic},
        {"unavailable_measurements_remain_null", UnavailableMeasurementsRemainNull},
        {"layer_totals_match_partition_totals", LayerTotalsMatchPartitionTotals},
        {"transfer_and_composer_evidence_is_serialized", TransferAndComposerEvidenceIsSerialized},
        {"closure_linkage_evidence_is_serialized", ClosureLinkageEvidenceIsSerialized},
        {"raster_mapping_evidence_is_serialized", RasterMappingEvidenceIsSerialized},
    };
    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        if (!test.second())
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }
    std::cout << "Texture/fill partition report unit tests complete.\n";
    return 0;
}
