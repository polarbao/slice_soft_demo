#include "slicer_core/pipeline/TextureFillPartitionDiagnosticFacade.h"

#include <iostream>
#include <string>

namespace
{

using slicer_core::Json;
using slicer_core::TextureFillPartitionDiagnosticFacade;
using slicer_core::TextureFillPartitionDiagnosticState;

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

Json MakeWidth(const bool evaluated)
{
    return Json::object({
        {"requestedWidthMm", 0.10},
        {"widthStepMm", 0.01},
        {"effectiveMinimumWidthMm", evaluated ? Json{0.10} : Json{nullptr}},
        {"effectiveWidthMm", evaluated ? Json{0.20} : Json{nullptr}},
        {"maxInteriorDistanceMm", evaluated ? Json{0.30} : Json{nullptr}},
        {"allTextureThresholdMm", evaluated ? Json{0.60} : Json{nullptr}},
        {"allTexture", false},
    });
}

Json MakePartition(const bool passed)
{
    return Json::object({
        {"modelVoxels", 100},
        {"textureSurfaceVoxels", 40},
        {"modelFillVoxels", 60},
        {"overlapTextureFillVoxels", 0},
        {"unassignedModelVoxels", 0},
        {"textureCoverageRatio", 0.4},
        {"modelFillCoverageRatio", 0.6},
        {"partitionPass", passed},
    });
}

Json MakeRasterMapping(const bool evaluated)
{
    return Json::object({
        {"availability", evaluated ? "available" : "unavailable"},
        {"status", evaluated ? "diagnostic" : "not_evaluated"},
        {"productionOutputWritten", false},
        {"rasterVoxelCount", 100},
        {"modelRasterVoxels", 100},
        {"textureSurfaceRasterVoxels", 40},
        {"modelFillRasterVoxels", 60},
        {"overlapRasterVoxels", 0},
        {"unassignedModelRasterVoxels", 0},
        {"partitionPass", evaluated},
        {"layerCount", 10},
    });
}

Json MakeFullClosure(
    const bool evaluated,
    const bool productionOutputWritten,
    const bool closurePass = true)
{
    return Json::object({
        {"availability", evaluated ? "available" : "unavailable"},
        {"status", evaluated ? "diagnostic" : "not_evaluated"},
        {"modelClosureStatus", evaluated ? "pass" : "not_evaluated"},
        {"supportClosureStatus", evaluated ? "pass" : "not_evaluated"},
        {"varnishClosureStatus", evaluated ? "pass" : "not_evaluated"},
        {"fullClosurePass", evaluated && closurePass},
        {"expectedDomainGapPixels", 0},
        {"modelDomainGapPixels", 0},
        {"supportRequiredGapPixels", 0},
        {"outerVarnishGapPixels", 0},
        {"unexpectedOccupiedPixels", 0},
        {"productionOutputWritten", productionOutputWritten},
    });
}

Json MakePerformance(const bool evaluated)
{
    return Json::object({
        {"preflightMs", evaluated ? Json{1.0} : Json{nullptr}},
        {"topologyMs", evaluated ? Json{2.0} : Json{nullptr}},
        {"levelSetMs", evaluated ? Json{3.0} : Json{nullptr}},
        {"gridSampleMs", evaluated ? Json{4.0} : Json{nullptr}},
        {"occupancyMs", evaluated ? Json{5.0} : Json{nullptr}},
        {"distanceMs", evaluated ? Json{6.0} : Json{nullptr}},
        {"partitionMs", evaluated ? Json{7.0} : Json{nullptr}},
        {"textureTransferMs", evaluated ? Json{8.0} : Json{nullptr}},
        {"rasterMappingMs", evaluated ? Json{9.0} : Json{nullptr}},
        {"fullClosureMs", evaluated ? Json{10.0} : Json{nullptr}},
        {"totalCoreMs", evaluated ? Json{55.0} : Json{nullptr}},
        {"gridVoxelCount", evaluated ? Json{100} : Json{nullptr}},
        {"peakWorkingSetBytes", evaluated ? Json{2048} : Json{nullptr}},
    });
}

Json MakeReport(
    const std::string& availability,
    const std::string& status,
    const bool evaluated,
    const bool productionOutputWritten = false)
{
    return Json::object({
        {"schema", "slicesoft.texture_fill_partition.12e.1"},
        {"availability", availability},
        {"status", status},
        {"backend", evaluated ? "legacy_cpu_global_distance" : "none"},
        {"backendRole", evaluated ? "conformance_candidate" : "unavailable"},
        {"productionAcceptance", "not_evaluated"},
        {"width", MakeWidth(evaluated)},
        {"partition", MakePartition(evaluated)},
        {"rasterMapping", MakeRasterMapping(evaluated)},
        {"fullClosureLinkage", MakeFullClosure(evaluated, productionOutputWritten)},
        {"closureLinkage", Json::object({{"productionOutputWritten", false}})},
        {"diagnosticComposer", Json::object({{"productionOutputWritten", false}})},
        {"performance", MakePerformance(evaluated)},
        {"issues", Json::array({})},
    });
}

bool TestPending()
{
    const auto dto = TextureFillPartitionDiagnosticFacade::Pending();
    return ExpectTrue(
               dto.state == TextureFillPartitionDiagnosticState::Pending,
               "pending state")
        && ExpectTrue(!dto.reportavailable, "pending report unavailable")
        && ExpectTrue(!dto.partitionstats.evaluated, "pending partition unevaluated")
        && ExpectTrue(!dto.performance.evaluated, "pending performance unevaluated")
        && ExpectTrue(
            std::string{TextureFillPartitionDiagnosticFacade::StateName(dto.state)}
                == "pending",
            "pending state name");
}

bool TestUnavailable()
{
    const auto dto = TextureFillPartitionDiagnosticFacade::Unavailable(
        "report has not been generated");
    return ExpectTrue(
               dto.state == TextureFillPartitionDiagnosticState::Unavailable,
               "unavailable state")
        && ExpectTrue(!dto.issues.empty(), "unavailable issue")
        && ExpectTrue(
            dto.issues.front().code == "E_12E_DIAGNOSTIC_REPORT_UNAVAILABLE",
            "unavailable stable code");
}

bool TestUnavailableReportDoesNotExposePlaceholderZeroes()
{
    const auto dto = TextureFillPartitionDiagnosticFacade::Inspect(
        MakeReport("unavailable", "blocked", false));
    return ExpectTrue(
               dto.state == TextureFillPartitionDiagnosticState::Unavailable,
               "report unavailable state")
        && ExpectTrue(dto.schemavalid, "unavailable report schema valid")
        && ExpectTrue(
            dto.widthmetrics.requestedwidthmm.has_value(),
            "requested width remains available")
        && ExpectTrue(
            !dto.widthmetrics.effectiveminimumwidthmm.has_value(),
            "derived width remains unevaluated")
        && ExpectTrue(
            !dto.partitionstats.modelvoxels.has_value(),
            "placeholder partition zero hidden")
        && ExpectTrue(
            !dto.performance.totalcorems.has_value(),
            "placeholder performance hidden");
}

bool TestBlockedReportPreservesTopologyIssue()
{
    Json report = Json::object({
        {"schema", "slicesoft.texture_fill_partition.12e.1"},
        {"availability", "available"},
        {"status", "blocked"},
        {"backend", "legacy_cpu_global_distance"},
        {"backendRole", "conformance_candidate"},
        {"productionAcceptance", "not_evaluated"},
        {"width", MakeWidth(false)},
        {"partition", MakePartition(false)},
        {"rasterMapping", MakeRasterMapping(false)},
        {"fullClosureLinkage", MakeFullClosure(false, false)},
        {"closureLinkage", Json::object({{"productionOutputWritten", false}})},
        {"diagnosticComposer", Json::object({{"productionOutputWritten", false}})},
        {"performance", MakePerformance(false)},
        {"issues", Json::array({Json::object({
             {"code", "MESH_SELF_INTERSECTION_CONFIRMED"},
             {"severity", "error"},
             {"message", "confirmed self-intersection"},
             {"context", Json::object({{"pairCount", 3}})},
         })})},
    });
    const auto dto = TextureFillPartitionDiagnosticFacade::Inspect(report);
    return ExpectTrue(
               dto.state == TextureFillPartitionDiagnosticState::Blocked,
               "blocked report state")
        && ExpectTrue(dto.issues.size() == 1U, "topology issue count")
        && ExpectTrue(
            dto.issues.front().code == "MESH_SELF_INTERSECTION_CONFIRMED",
            "topology issue preserved")
        && ExpectTrue(
            dto.issues.front().context.at("pairCount").as_int() == 3,
            "topology context preserved")
        && ExpectTrue(
            !dto.partitionstats.evaluated,
            "blocked partition remains unevaluated");
}

bool TestDiagnosticReport()
{
    const auto dto = TextureFillPartitionDiagnosticFacade::Inspect(
        MakeReport("available", "diagnostic", true));
    return ExpectTrue(
               dto.state == TextureFillPartitionDiagnosticState::Diagnostic,
               "diagnostic state")
        && ExpectTrue(dto.schemavalid, "diagnostic schema valid")
        && ExpectTrue(dto.partitionstats.evaluated, "partition evaluated")
        && ExpectTrue(
            dto.partitionstats.modelfillvoxels == std::optional<std::uint64_t>{60U},
            "model fill count")
        && ExpectTrue(dto.rastermapping.evaluated, "raster mapping evaluated")
        && ExpectTrue(
            dto.fullclosurelinkage.evaluated,
            "full closure evaluated")
        && ExpectTrue(dto.performance.evaluated, "performance evaluated")
        && ExpectTrue(
            dto.performance.totalcorems == std::optional<double>{55.0},
            "core timing")
        && ExpectTrue(!dto.productionoutputwritten, "no production output");
}

bool TestDiagnosticPartitionFailureFailsClosed()
{
    Json report = MakeReport("available", "diagnostic", true);
    Json::Object root = report.as_object();
    root["partition"] = MakePartition(false);
    const auto dto = TextureFillPartitionDiagnosticFacade::Inspect(Json{root});
    return ExpectTrue(
               dto.state == TextureFillPartitionDiagnosticState::Blocked,
               "diagnostic partition failure blocked")
        && ExpectTrue(
            !dto.partitionstats.evaluated,
            "failed diagnostic partition remains unevaluated")
        && ExpectTrue(
            dto.issues.back().code == "E_12E_DIAGNOSTIC_REPORT_INVALID",
            "partition failure stable code");
}

bool TestDiagnosticFullClosureFailureFailsClosed()
{
    Json report = MakeReport("available", "diagnostic", true);
    Json::Object root = report.as_object();
    root["fullClosureLinkage"] = MakeFullClosure(true, false, false);
    const auto dto = TextureFillPartitionDiagnosticFacade::Inspect(Json{root});
    return ExpectTrue(
               dto.state == TextureFillPartitionDiagnosticState::Blocked,
               "diagnostic full closure failure blocked")
        && ExpectTrue(
            !dto.fullclosurelinkage.evaluated,
            "failed full closure remains unevaluated")
        && ExpectTrue(
            dto.issues.back().code == "E_12E_DIAGNOSTIC_REPORT_INVALID",
            "full closure failure stable code");
}

bool TestInvalidSchemaFailsClosed()
{
    Json report = MakeReport("available", "diagnostic", true);
    Json::Object root = report.as_object();
    root["schema"] = "slicesoft.texture_fill_partition.release_matrix.12e_08c.1";
    const auto dto = TextureFillPartitionDiagnosticFacade::Inspect(Json{root});
    return ExpectTrue(
               dto.state == TextureFillPartitionDiagnosticState::Blocked,
               "release matrix rejected")
        && ExpectTrue(!dto.schemavalid, "release matrix schema invalid")
        && ExpectTrue(!dto.issues.empty(), "invalid schema issue");
}

bool TestProductionOutputFailsClosed()
{
    const auto dto = TextureFillPartitionDiagnosticFacade::Inspect(
        MakeReport("available", "diagnostic", true, true));
    return ExpectTrue(
               dto.state == TextureFillPartitionDiagnosticState::Blocked,
               "production output blocked")
        && ExpectTrue(dto.productionoutputwritten, "production output surfaced")
        && ExpectTrue(
            dto.issues.back().code == "E_12E_DIAGNOSTIC_REPORT_PRODUCTION_OUTPUT",
            "production output stable code");
}

}  // namespace

int main()
{
    const bool passed = TestPending()
        && TestUnavailable()
        && TestUnavailableReportDoesNotExposePlaceholderZeroes()
        && TestBlockedReportPreservesTopologyIssue()
        && TestDiagnosticReport()
        && TestDiagnosticPartitionFailureFailsClosed()
        && TestDiagnosticFullClosureFailureFailsClosed()
        && TestInvalidSchemaFailsClosed()
        && TestProductionOutputFailsClosed();
    if (!passed)
    {
        return 1;
    }
    std::cout << "PASS texture_fill_partition_diagnostic_facade_unit_tests\n";
    return 0;
}
