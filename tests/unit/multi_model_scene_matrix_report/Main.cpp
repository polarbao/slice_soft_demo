#include "slicer_core/reports/MultiModelSceneMatrixReport.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

slicer_core::MultiModelSceneMatrixReport MakeReport()
{
    slicer_core::MultiModelSceneMatrixReport report;
    report.status = "passed";
    report.buildconfig = "Debug";
    report.compiler = "MSVC";
    report.functionalmatrixpass = true;
    report.productionblockers = {
        "device_build_volume_open",
        "device_axes_open",
        "performance_budget_open"};
    report.knowncoveragegaps = {
        "complex_relief_0_of_3"};

    slicer_core::MultiModelSceneMatrixCase positive;
    positive.caseid = "13B-M01";
    positive.category = "positive";
    positive.status = "passed";
    positive.expectedpass = true;
    positive.passed = true;
    positive.instancecount = 1;
    positive.uniquemodelcount = 1;
    positive.sliceproducerinvocationcount = 1;
    positive.widthpx = 10;
    positive.heightpx = 20;
    positive.layercount = 2;
    positive.packagewritten = true;
    positive.ripstrictpass = true;
    positive.packagebytes = 100U;
    positive.packagedir = "output/package";
    positive.formats = {"obj"};
    positive.modelids = {"model"};
    positive.timing.totalms = 1.0;
    report.cases.push_back(positive);

    slicer_core::MultiModelSceneMatrixCase negative;
    negative.caseid = "13B-N23";
    negative.category = "negative";
    negative.status = "blocked";
    negative.expectedpass = false;
    negative.passed = false;
    negative.instancecount = 23;
    negative.errorcode = "GRID_INSTANCE_CAPACITY_EXCEEDED";
    report.cases.push_back(negative);
    return report;
}

void SerializesFixedProtocolAndCases()
{
    const slicer_core::MultiModelSceneMatrixReport report =
        MakeReport();
    Require(
        slicer_core::ValidateMultiModelSceneMatrixReport(report),
        "complete matrix report should validate");
    const slicer_core::Json document =
        slicer_core::SerializeMultiModelSceneMatrixReport(report);
    Require(
        document.at("schema").as_string()
            == "slicesoft.multimodel_scene_matrix.13b.1",
        "matrix schema should remain stable");
    Require(
        document.at("fixedProtocol").at("schema").as_string()
            == "p0.rgbwsv.2"
            && document.at("fixedProtocol").at("bitDepth").as_int()
                == 8
            && document.at("fixedProtocol").at("polarity").as_string()
                == "black_is_print",
        "fixed RGBWSV protocol should be explicit");
    Require(
        document.at("cases").size() == 2U,
        "positive and negative cases should be retained");
    Require(
        slicer_core::RenderMultiModelSceneMatrixMarkdown(report)
            .find("13B-M01") != std::string::npos,
        "Markdown should include case identities");
}

void RejectsProductionClaimAndFakeNegativePackage()
{
    slicer_core::MultiModelSceneMatrixReport production =
        MakeReport();
    production.productiongo = true;
    Require(
        !slicer_core::ValidateMultiModelSceneMatrixReport(production),
        "open-input report must reject a production GO claim");

    slicer_core::MultiModelSceneMatrixReport negative =
        MakeReport();
    negative.cases.back().packagewritten = true;
    Require(
        !slicer_core::ValidateMultiModelSceneMatrixReport(negative),
        "blocked negative case must not publish a package");
}

void RejectsDuplicateCasesAndAggregateMismatch()
{
    slicer_core::MultiModelSceneMatrixReport duplicate =
        MakeReport();
    duplicate.cases.back().caseid = duplicate.cases.front().caseid;
    Require(
        !slicer_core::ValidateMultiModelSceneMatrixReport(duplicate),
        "case identities must be unique");

    slicer_core::MultiModelSceneMatrixReport aggregate =
        MakeReport();
    aggregate.functionalmatrixpass = false;
    Require(
        !slicer_core::ValidateMultiModelSceneMatrixReport(aggregate),
        "aggregate functional status must match case outcomes");
}

}  // namespace

int main()
{
    SerializesFixedProtocolAndCases();
    RejectsProductionClaimAndFakeNegativePackage();
    RejectsDuplicateCasesAndAggregateMismatch();
    std::cout << "multi_model_scene_matrix_report_unit_tests passed\n";
    return 0;
}

