#include "slicer_core/diagnostics/TextureFillPartitionPositiveMatrix.h"
#include "slicer_core/geometry/TriangleMeshData.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

slicer_core::AdaptedTriangleMesh MakeAdaptedBox()
{
    slicer_core::AdaptedTriangleMesh adapted;
    adapted.mesh = slicer_core::MakeGeneratedBoxMesh(1.0, 1.0, 1.0);
    adapted.triangle_attributes.resize(adapted.mesh.triangles.size());
    for (std::size_t index{0U};
         index < adapted.triangle_attributes.size();
         ++index)
    {
        adapted.triangle_attributes.at(index).source_triangle_index = index;
    }
    return adapted;
}

slicer_core::TextureFillPartitionPositiveMatrixRequest MakeRequest(
    const slicer_core::AdaptedTriangleMesh& adapted,
    const slicer_core::ModelPreflightResult& preflight)
{
    slicer_core::TextureFillPartitionPositiveMatrixRequest request;
    request.adaptedMesh = &adapted;
    request.preflight = &preflight;
    request.caseId = "generated_closed_box";
    request.modelPath = "generated://closed_box";
    request.sourceHash = "source-hash";
    request.resourceHash = "resource-hash";
    request.preflightStatus = "passed";
    request.voxelMm = 0.10;
    request.paddingVoxels = 1;
    request.fallbackRgb = {7U, 11U, 13U};
    request.modelFillRgb = {17U, 19U, 23U};
    request.modelFillValue = 0U;
    request.profile.enabled = true;
    request.profile.name = "fixture-profile";
    request.profile.white.enabled = true;
    request.profile.white.value = 0U;
    request.requestedRoles = {"c", "m"};

    slicer_core::ModelFillMaterialRoleRegistration cyan;
    cyan.roleId = "c";
    cyan.resolvedMaterial = "rgb";
    cyan.rgb = {29U, 31U, 37U};
    cyan.profileId = "fixture-c-profile";
    request.roleRegistry.push_back(std::move(cyan));
    return request;
}

slicer_core::Json LoadGolden()
{
    const std::filesystem::path path =
        std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "tests"
        / "golden"
        / "expected"
        / "12e_r4_clean_positive_matrix_projection.json";
    std::ifstream input{path};
    if (!input)
    {
        throw std::runtime_error(
            "failed to open positive matrix golden: " + path.string());
    }
    return slicer_core::Json::parse(input);
}

slicer_core::Json BuildProjection(const slicer_core::Json& report)
{
    slicer_core::Json::Array widthSamples;
    for (const slicer_core::Json& sample :
         report.at("widthSweep").at("samples").as_array())
    {
        widthSamples.push_back(slicer_core::Json::object({
            {"allTexture", sample.at("allTexture")},
            {"partitionPass", sample.at("partitionPass")},
            {"invariantPass", sample.at("invariantPass")},
        }));
    }

    slicer_core::Json::Array materialCases;
    for (const slicer_core::Json& material :
         report.at("materialCases").as_array())
    {
        materialCases.push_back(slicer_core::Json::object({
            {"requestedMaterial", material.at("requestedMaterial")},
            {"requestedRole", material.at("requestedRole")},
            {"available", material.at("available")},
            {"resolvedMaterial", material.at("resolvedMaterial")},
            {"resolvedChannels", material.at("resolvedChannels")},
            {"profileId", material.at("profileId")},
            {"reasonCode", material.at("reasonCode")},
            {"compositionEvaluated", material.at("compositionEvaluated")},
            {"compositionPass", material.at("compositionPass")},
        }));
    }

    return slicer_core::Json::object({
        {"schema", report.at("schema")},
        {"diagnosticOnly", report.at("diagnosticOnly")},
        {"productionOutputWritten", report.at("productionOutputWritten")},
        {"requiredRepairPassCount", report.at("requiredRepairPassCount")},
        {"input",
         slicer_core::Json::object({
             {"caseId", report.at("input").at("caseId")},
             {"preflightStatus", report.at("input").at("preflightStatus")},
         })},
        {"widthSweep",
         slicer_core::Json::object({
             {"status", report.at("widthSweep").at("status")},
             {"sampleCount", report.at("widthSweep").at("sampleCount")},
             {"requestedAnchorCount",
              static_cast<std::uint64_t>(
                  report.at("widthSweep")
                      .at("requestedAnchorWidthsMm")
                      .as_array()
                      .size())},
             {"deduplicated", report.at("widthSweep").at("deduplicated")},
             {"monotonicPass", report.at("widthSweep").at("monotonicPass")},
             {"endpointPass", report.at("widthSweep").at("endpointPass")},
             {"samples", slicer_core::Json{std::move(widthSamples)}},
         })},
        {"materialCases", slicer_core::Json{std::move(materialCases)}},
        {"summary", report.at("summary")},
    });
}

bool CleanFixturePassesPositiveMatrix()
{
    const slicer_core::AdaptedTriangleMesh adapted = MakeAdaptedBox();
    slicer_core::ModelPreflightResult preflight;
    preflight.status = slicer_core::ModelPreflightStatus::Passed;
    preflight.identity.sourceHash = "source-hash";
    preflight.identity.resourceHash = "resource-hash";
    const slicer_core::TextureFillPartitionPositiveMatrixRequest request =
        MakeRequest(adapted, preflight);
    const slicer_core::TextureFillPartitionPositiveMatrixResult result =
        slicer_core::RunTextureFillPartitionPositiveMatrix(request);

    bool passed = ExpectTrue(result.evidenceCollected, "evidence is collected")
        && ExpectTrue(result.matrixPass, "clean matrix passes")
        && ExpectTrue(
               !result.productionOutputWritten,
               "positive matrix does not write production output")
        && ExpectTrue(
               result.requiredRepairPassCount == 0U,
               "clean fixtures do not count as repaired assets")
        && ExpectTrue(
               result.widthSweep.monotonicPass,
               "width sweep is monotonic")
        && ExpectTrue(
               result.widthSweep.endpointPass,
               "all-texture endpoint passes")
        && ExpectTrue(
               result.widthSweep.requestedAnchorWidthsMm.size() == 3U,
               "three requested anchors remain auditable before deduplication")
        && ExpectTrue(
               result.widthSweep.samples.size() >= 1U
                   && result.widthSweep.samples.size() <= 3U,
               "width sweep contains deduplicated representative samples")
        && ExpectTrue(
               result.widthSweep.samples.back().allTexture,
               "last sample reaches all texture")
        && ExpectTrue(
               result.widthSweep.samples.back().stats.modelFillVoxels == 0U,
               "all-texture endpoint has no model fill")
        && ExpectTrue(
               result.materialCases.size() == 6U,
               "built-ins, profile, registered and unavailable roles are covered");

    for (const slicer_core::TextureFillPartitionPositiveMaterialCase& material :
         result.materialCases)
    {
        passed = ExpectTrue(
                     material.resolution.available
                         ? material.compositionPass
                         : !material.resolution.reasonCode.empty(),
                     "each material case composes or has a stable unavailable reason")
            && ExpectTrue(
                   material.printVoxels.at(4U) == 0U,
                   "Model Fill never occupies support")
            && passed;
    }

    const slicer_core::Json actual = BuildProjection(result.report);
    const slicer_core::Json expected = LoadGolden();
    if (actual.dump(2) != expected.dump(2))
    {
        std::cerr << "Actual positive matrix projection:\n"
                  << actual.dump(2)
                  << '\n';
        passed = ExpectTrue(false, "positive matrix projection matches golden")
            && passed;
    }
    return passed;
}

bool PreflightBlockerStopsBeforePartition()
{
    const slicer_core::AdaptedTriangleMesh adapted = MakeAdaptedBox();
    slicer_core::ModelPreflightResult preflight;
    preflight.status = slicer_core::ModelPreflightStatus::Blocked;
    preflight.identity.sourceHash = "source-hash";
    preflight.identity.resourceHash = "resource-hash";
    slicer_core::TextureFillPartitionPositiveMatrixRequest request =
        MakeRequest(adapted, preflight);
    request.preflightStatus = "blocked";
    const slicer_core::TextureFillPartitionPositiveMatrixResult result =
        slicer_core::RunTextureFillPartitionPositiveMatrix(request);

    return ExpectTrue(!result.matrixPass, "blocked preflight stops matrix")
        && ExpectTrue(result.widthSweep.samples.empty(), "no partition samples run")
        && ExpectTrue(result.materialCases.empty(), "no material composition runs")
        && ExpectTrue(
               result.report.at("productionOutputWritten").as_bool() == false,
               "blocked matrix still records no production output");
}

}  // namespace

int main()
{
    try
    {
        if (!CleanFixturePassesPositiveMatrix()
            || !PreflightBlockerStopsBeforePartition())
        {
            return 1;
        }
        std::cout << "Texture/fill positive matrix tests complete.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
