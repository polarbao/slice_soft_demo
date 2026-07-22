#include "slicer_core/diagnostics/RepairedAssetIntakeReport.h"
#include "slicer_core/geometry/repair/MeshRepairHash.h"
#include "slicer_core/preflight/RepairedAssetIntakeService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct TestCase
{
    std::string name;
    std::function<bool()> run;
};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

std::filesystem::path MakeTestDirectory()
{
    const auto suffix = std::chrono::steady_clock::now()
        .time_since_epoch()
        .count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("slicesoft_intake_" + std::to_string(suffix));
    std::filesystem::create_directories(directory);
    return directory;
}

void WriteText(const std::filesystem::path& path, const std::string& content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    if (!output)
    {
        throw std::runtime_error("failed to write fixture: " + path.string());
    }
    output << content;
}

std::string ClosedBoxObj(const std::string& header = {})
{
    return header
        + "v 0 0 0\n"
          "v 1 0 0\n"
          "v 1 1 0\n"
          "v 0 1 0\n"
          "v 0 0 1\n"
          "v 1 0 1\n"
          "v 1 1 1\n"
          "v 0 1 1\n"
          "f 1 3 2\n"
          "f 1 4 3\n"
          "f 5 6 7\n"
          "f 5 7 8\n"
          "f 1 2 6\n"
          "f 1 6 5\n"
          "f 2 3 7\n"
          "f 2 7 6\n"
          "f 3 4 8\n"
          "f 3 8 7\n"
          "f 4 1 5\n"
          "f 4 5 8\n";
}

std::string OpenBoxObj()
{
    std::string value = ClosedBoxObj();
    const std::string face = "f 4 5 8\n";
    value.erase(value.rfind(face), face.size());
    return value;
}

std::string MaterialBoxObj(const std::string& materialLibrary)
{
    return "mtllib " + materialLibrary + "\n"
        + "usemtl surface\n"
        + ClosedBoxObj();
}

std::string OverlappingBoxesObj()
{
    return ClosedBoxObj("# first box\n")
        + "v 0.5 0.5 0.5\n"
          "v 1.5 0.5 0.5\n"
          "v 1.5 1.5 0.5\n"
          "v 0.5 1.5 0.5\n"
          "v 0.5 0.5 1.5\n"
          "v 1.5 0.5 1.5\n"
          "v 1.5 1.5 1.5\n"
          "v 0.5 1.5 1.5\n"
          "f 9 11 10\n"
          "f 9 12 11\n"
          "f 13 14 15\n"
          "f 13 15 16\n"
          "f 9 10 14\n"
          "f 9 14 13\n"
          "f 10 11 15\n"
          "f 10 15 14\n"
          "f 11 12 16\n"
          "f 11 16 15\n"
          "f 12 9 13\n"
          "f 12 13 16\n";
}

std::filesystem::path WriteConfig(
    const std::filesystem::path& directory,
    const std::string& name,
    const std::filesystem::path& modelPath)
{
    const std::filesystem::path configPath = directory / name;
    WriteText(
        configPath,
        "{\n"
        "  \"input\": {\"modelPath\": \""
            + modelPath.generic_string()
            + "\", \"format\": \"obj\"},\n"
              "  \"texture\": {\"missingTexturePolicy\": \"fail_fast\"},\n"
              "  \"autoOrient\": {\"enabled\": false}\n"
              "}\n");
    return configPath;
}

slicer_core::RepairedAssetIntakeRequest MakeStrictOriginalRequest(
    const std::filesystem::path& configPath,
    const std::string& sourceContent)
{
    slicer_core::RepairedAssetIntakeRequest request;
    request.family_id = "required_aishen_family";
    request.candidate_id = "generated_aishen_box";
    request.candidate_kind =
        slicer_core::RepairedAssetCandidateKind::StrictPassOriginal;
    request.original_config_path = configPath;
    request.candidate_config_path = configPath;
    request.expected_original_source_hash =
        slicer_core::ComputeMeshRepairSha256(sourceContent);
    request.expected_candidate_source_hash =
        request.expected_original_source_hash;
    return request;
}

bool HasIssue(
    const slicer_core::RepairedAssetIntakeResult& result,
    const std::string& code)
{
    for (const slicer_core::RepairedAssetIntakeIssue& issue : result.issues)
    {
        if (issue.code == code)
        {
            return true;
        }
    }
    return false;
}

slicer_core::Json LoadGolden()
{
    const std::filesystem::path path =
        std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "tests/golden/expected/12e_r4_repaired_asset_intake_projection.json";
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error("failed to read intake golden: " + path.string());
    }
    return slicer_core::Json::parse(input);
}

slicer_core::Json BuildProjection(const slicer_core::Json& report)
{
    return slicer_core::Json::object({
        {"schema", report.at("schema")},
        {"diagnosticOnly", report.at("diagnosticOnly")},
        {"productionOutputWritten", report.at("productionOutputWritten")},
        {"familyId", report.at("familyId")},
        {"candidateKind", report.at("candidateKind")},
        {"manifestAccepted", report.at("manifestAccepted")},
        {"status", report.at("status")},
        {"admitted", report.at("admitted")},
        {"requiredFamilyPassCount", report.at("requiredFamilyPassCount")},
        {"repeatability", report.at("repeatability")},
        {"original",
         slicer_core::Json::object({
             {"preflightStatus", report.at("original").at("preflightStatus")},
             {"fullAuditComplete", report.at("original").at("fullAuditComplete")},
             {"strictPass", report.at("original").at("strictPass")},
             {"vertexCount", report.at("original").at("vertexCount")},
             {"triangleCount", report.at("original").at("triangleCount")},
             {"componentCount", report.at("original").at("componentCount")},
         })},
        {"candidate",
         slicer_core::Json::object({
             {"preflightStatus", report.at("candidate").at("preflightStatus")},
             {"fullAuditComplete", report.at("candidate").at("fullAuditComplete")},
             {"strictPass", report.at("candidate").at("strictPass")},
             {"vertexCount", report.at("candidate").at("vertexCount")},
             {"triangleCount", report.at("candidate").at("triangleCount")},
             {"componentCount", report.at("candidate").at("componentCount")},
         })},
        {"delta",
         slicer_core::Json::object({
             {"maxAbsDimensionDeltaMm",
              report.at("delta").at("maxAbsDimensionDeltaMm")},
             {"geometryChanged", report.at("delta").at("geometryChanged")},
             {"attributesChanged", report.at("delta").at("attributesChanged")},
             {"resourcesChanged", report.at("delta").at("resourcesChanged")},
         })},
        {"reasonCodes", report.at("reasonCodes")},
    });
}

bool StrictOriginalIsAdmitted()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path familyDirectory =
        directory / "aishen_fudiao";
    const std::string source = ClosedBoxObj();
    const std::filesystem::path modelPath = familyDirectory / "box.obj";
    WriteText(modelPath, source);
    const std::filesystem::path configPath = WriteConfig(
        directory,
        "box.json",
        modelPath);

    const slicer_core::RepairedAssetIntakeService service;
    const slicer_core::RepairedAssetIntakeResult result = service.Run(
        MakeStrictOriginalRequest(configPath, source));
    const slicer_core::Json report =
        slicer_core::BuildRepairedAssetIntakeReport(result);
    const bool goldenMatches = BuildProjection(report).dump(2)
        == LoadGolden().dump(2);

    return ExpectTrue(result.manifest_accepted, "manifest is accepted")
        && ExpectTrue(result.admitted, "strict original is admitted")
        && ExpectTrue(result.repeatability_pass, "repeatability passes")
        && ExpectTrue(
            result.required_family_pass_count == 1U,
            "admitted family count is one")
        && ExpectTrue(!result.production_output_written, "no production output")
        && ExpectTrue(goldenMatches, "report projection matches golden");
}

bool CrossFamilyControlIsRejected()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::string source = ClosedBoxObj();
    const std::filesystem::path modelPath = directory / "xiao_ma" / "box.obj";
    WriteText(modelPath, source);
    const std::filesystem::path configPath = WriteConfig(
        directory,
        "box.json",
        modelPath);

    const slicer_core::RepairedAssetIntakeService service;
    const slicer_core::RepairedAssetIntakeResult result = service.Run(
        MakeStrictOriginalRequest(configPath, source));
    return ExpectTrue(!result.admitted, "cross-family control is blocked")
        && ExpectTrue(
            HasIssue(result, "E_12E_INTAKE_FAMILY_PATH_MISMATCH"),
            "family mismatch has stable code")
        && ExpectTrue(
            result.required_family_pass_count == 0U,
            "cross-family control does not count");
}

bool DevelopmentModelPoolStrictOriginalIsAdmitted()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelDirectory =
        directory / "model" / "clean_fixture";
    const std::string source = ClosedBoxObj();
    const std::filesystem::path modelPath = modelDirectory / "box.obj";
    WriteText(modelPath, source);
    const std::filesystem::path configPath = WriteConfig(
        directory,
        "box.json",
        modelPath);

    slicer_core::RepairedAssetIntakeRequest request =
        MakeStrictOriginalRequest(configPath, source);
    request.family_id = "development_model_pool";
    request.candidate_id = "generated_development_box";

    const slicer_core::RepairedAssetIntakeService service;
    const slicer_core::RepairedAssetIntakeResult result = service.Run(request);
    return ExpectTrue(result.manifest_accepted, "development manifest is accepted")
        && ExpectTrue(result.admitted, "development model is admitted")
        && ExpectTrue(result.repeatability_pass, "development repeatability passes")
        && ExpectTrue(
            result.required_family_pass_count == 0U,
            "development intake does not count as a required family pass")
        && ExpectTrue(
            !result.production_output_written,
            "development intake writes no production output");
}

bool ChangedCandidateRequiresProvenance()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path familyDirectory =
        directory / "aishen_fudiao";
    const std::string originalSource = ClosedBoxObj("# original\n");
    const std::string candidateSource = ClosedBoxObj("# repaired\n");
    const std::filesystem::path originalPath = familyDirectory / "original.obj";
    const std::filesystem::path candidatePath = directory / "repaired.obj";
    WriteText(originalPath, originalSource);
    WriteText(candidatePath, candidateSource);

    slicer_core::RepairedAssetIntakeRequest request;
    request.family_id = "required_aishen_family";
    request.candidate_id = "repaired_without_provenance";
    request.candidate_kind =
        slicer_core::RepairedAssetCandidateKind::ExternalRepaired;
    request.original_config_path = WriteConfig(
        directory,
        "original.json",
        originalPath);
    request.candidate_config_path = WriteConfig(
        directory,
        "candidate.json",
        candidatePath);
    request.expected_original_source_hash =
        slicer_core::ComputeMeshRepairSha256(originalSource);
    request.expected_candidate_source_hash =
        slicer_core::ComputeMeshRepairSha256(candidateSource);

    const slicer_core::RepairedAssetIntakeService service;
    const slicer_core::RepairedAssetIntakeResult result = service.Run(request);
    return ExpectTrue(!result.manifest_accepted, "missing provenance rejects manifest")
        && ExpectTrue(
            HasIssue(result, "E_12E_INTAKE_PROVENANCE_MISSING"),
            "missing provenance has stable code");
}

bool PostStrictFailureBlocksCandidate()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path familyDirectory =
        directory / "titian_fudiao";
    const std::string originalSource = ClosedBoxObj("# original\n");
    const std::string candidateSource = OpenBoxObj();
    const std::filesystem::path originalPath = familyDirectory / "original.obj";
    const std::filesystem::path candidatePath = directory / "candidate.obj";
    WriteText(originalPath, originalSource);
    WriteText(candidatePath, candidateSource);

    slicer_core::RepairedAssetIntakeRequest request;
    request.family_id = "required_titian_family";
    request.candidate_id = "open_candidate";
    request.candidate_kind =
        slicer_core::RepairedAssetCandidateKind::IndependentlyRebuilt;
    request.original_config_path = WriteConfig(
        directory,
        "original.json",
        originalPath);
    request.candidate_config_path = WriteConfig(
        directory,
        "candidate.json",
        candidatePath);
    request.expected_original_source_hash =
        slicer_core::ComputeMeshRepairSha256(originalSource);
    request.expected_candidate_source_hash =
        slicer_core::ComputeMeshRepairSha256(candidateSource);
    request.provenance = {
        "fixture",
        "fixture-builder",
        "1",
        "remove one face",
        "2026-07-22T00:00:00Z",
        "unit-test"};
    request.approval.allow_attribute_changes = true;
    request.approval.attribute_change_reason = "negative strict fixture";

    const slicer_core::RepairedAssetIntakeService service;
    const slicer_core::RepairedAssetIntakeResult result = service.Run(request);
    return ExpectTrue(result.manifest_accepted, "negative candidate manifest is accepted")
        && ExpectTrue(!result.admitted, "open candidate is blocked")
        && ExpectTrue(
            HasIssue(result, "E_12E_INTAKE_POST_STRICT_FAILED"),
            "post-strict failure has stable code");
}

bool HashMismatchBlocksCandidate()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path familyDirectory =
        directory / "meigui_fudiao";
    const std::string originalSource = ClosedBoxObj("# original\n");
    const std::string candidateSource = ClosedBoxObj("# changed\n");
    const std::filesystem::path originalPath = familyDirectory / "original.obj";
    const std::filesystem::path candidatePath = directory / "candidate.obj";
    WriteText(originalPath, originalSource);
    WriteText(candidatePath, candidateSource);

    slicer_core::RepairedAssetIntakeRequest request;
    request.family_id = "required_meigui_family";
    request.candidate_id = "hash_mismatch";
    request.candidate_kind =
        slicer_core::RepairedAssetCandidateKind::ExternalRepaired;
    request.original_config_path = WriteConfig(
        directory,
        "original.json",
        originalPath);
    request.candidate_config_path = WriteConfig(
        directory,
        "candidate.json",
        candidatePath);
    request.expected_original_source_hash = "wrong-hash";
    request.expected_candidate_source_hash =
        slicer_core::ComputeMeshRepairSha256(candidateSource);
    request.provenance = {
        "fixture",
        "fixture-builder",
        "1",
        "rewrite source",
        "2026-07-22T00:00:00Z",
        "unit-test"};

    const slicer_core::RepairedAssetIntakeService service;
    const slicer_core::RepairedAssetIntakeResult result = service.Run(request);
    return ExpectTrue(!result.manifest_accepted, "hash mismatch rejects manifest")
        && ExpectTrue(
            HasIssue(result, "E_12E_INTAKE_ORIGINAL_HASH_MISMATCH"),
            "hash mismatch has stable code");
}

bool AttributeMismatchBlocksCandidate()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path familyDirectory =
        directory / "meigui_fudiao";
    const std::string originalSource = ClosedBoxObj("# original\n");
    const std::string candidateSource = MaterialBoxObj("candidate.mtl");
    const std::filesystem::path originalPath = familyDirectory / "original.obj";
    const std::filesystem::path candidatePath = directory / "candidate.obj";
    WriteText(originalPath, originalSource);
    WriteText(candidatePath, candidateSource);
    WriteText(directory / "candidate.mtl", "newmtl surface\nKd 0.2 0.4 0.6\n");

    slicer_core::RepairedAssetIntakeRequest request;
    request.family_id = "required_meigui_family";
    request.candidate_id = "attribute_mismatch";
    request.candidate_kind =
        slicer_core::RepairedAssetCandidateKind::ExternalRepaired;
    request.original_config_path = WriteConfig(
        directory,
        "original.json",
        originalPath);
    request.candidate_config_path = WriteConfig(
        directory,
        "candidate.json",
        candidatePath);
    request.expected_original_source_hash =
        slicer_core::ComputeMeshRepairSha256(originalSource);
    request.expected_candidate_source_hash =
        slicer_core::ComputeMeshRepairSha256(candidateSource);
    request.provenance = {
        "fixture",
        "fixture-builder",
        "1",
        "add material",
        "2026-07-22T00:00:00Z",
        "unit-test"};

    const slicer_core::RepairedAssetIntakeService service;
    const slicer_core::RepairedAssetIntakeResult result = service.Run(request);
    return ExpectTrue(result.manifest_accepted, "attribute manifest is accepted")
        && ExpectTrue(!result.admitted, "unapproved attribute change is blocked")
        && ExpectTrue(
            HasIssue(result, "E_12E_INTAKE_ATTRIBUTE_MISMATCH"),
            "attribute mismatch has stable code");
}

bool MissingTextureBlocksCandidate()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path familyDirectory =
        directory / "aishen_fudiao";
    const std::string originalSource = ClosedBoxObj("# original\n");
    const std::string candidateSource = MaterialBoxObj("missing.mtl");
    const std::filesystem::path originalPath = familyDirectory / "original.obj";
    const std::filesystem::path candidatePath = directory / "candidate.obj";
    WriteText(originalPath, originalSource);
    WriteText(candidatePath, candidateSource);
    WriteText(
        directory / "missing.mtl",
        "newmtl surface\nKd 1 1 1\nmap_Kd absent.png\n");

    slicer_core::RepairedAssetIntakeRequest request;
    request.family_id = "required_aishen_family";
    request.candidate_id = "missing_texture";
    request.candidate_kind =
        slicer_core::RepairedAssetCandidateKind::ExternalRepaired;
    request.original_config_path = WriteConfig(
        directory,
        "original.json",
        originalPath);
    request.candidate_config_path = WriteConfig(
        directory,
        "candidate.json",
        candidatePath);
    request.expected_original_source_hash =
        slicer_core::ComputeMeshRepairSha256(originalSource);
    request.expected_candidate_source_hash =
        slicer_core::ComputeMeshRepairSha256(candidateSource);
    request.provenance = {
        "fixture",
        "fixture-builder",
        "1",
        "add missing texture",
        "2026-07-22T00:00:00Z",
        "unit-test"};
    request.approval.allow_attribute_changes = true;
    request.approval.attribute_change_reason = "negative resource fixture";

    const slicer_core::RepairedAssetIntakeService service;
    const slicer_core::RepairedAssetIntakeResult result = service.Run(request);
    return ExpectTrue(result.manifest_accepted, "resource manifest is accepted")
        && ExpectTrue(!result.admitted, "missing texture is blocked")
        && ExpectTrue(
            HasIssue(result, "E_12E_INTAKE_RESOURCE_MISSING"),
            "missing texture has stable code");
}

bool IncompleteAuditBlocksCandidate()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path familyDirectory =
        directory / "titian_fudiao";
    const std::string originalSource = ClosedBoxObj("# original\n");
    const std::string candidateSource = OverlappingBoxesObj();
    const std::filesystem::path originalPath = familyDirectory / "original.obj";
    const std::filesystem::path candidatePath = directory / "candidate.obj";
    WriteText(originalPath, originalSource);
    WriteText(candidatePath, candidateSource);

    slicer_core::RepairedAssetIntakeRequest request;
    request.family_id = "required_titian_family";
    request.candidate_id = "incomplete_audit";
    request.candidate_kind =
        slicer_core::RepairedAssetCandidateKind::IndependentlyRebuilt;
    request.original_config_path = WriteConfig(
        directory,
        "original.json",
        originalPath);
    request.candidate_config_path = WriteConfig(
        directory,
        "candidate.json",
        candidatePath);
    request.expected_original_source_hash =
        slicer_core::ComputeMeshRepairSha256(originalSource);
    request.expected_candidate_source_hash =
        slicer_core::ComputeMeshRepairSha256(candidateSource);
    request.provenance = {
        "fixture",
        "fixture-builder",
        "1",
        "overlap two boxes",
        "2026-07-22T00:00:00Z",
        "unit-test"};
    request.preflight_options.maxCompleteSelfIntersectionCandidatePairs = 1U;
    request.approval.max_dimension_delta_mm = 1.0;

    const slicer_core::RepairedAssetIntakeService service;
    const slicer_core::RepairedAssetIntakeResult result = service.Run(request);
    return ExpectTrue(result.manifest_accepted, "audit manifest is accepted")
        && ExpectTrue(!result.admitted, "incomplete audit is blocked")
        && ExpectTrue(!result.candidate.full_audit_complete, "audit remains incomplete")
        && ExpectTrue(
            HasIssue(result, "E_12E_INTAKE_POST_STRICT_FAILED"),
            "incomplete audit has stable post-strict code");
}

}  // namespace

int main()
{
    const std::vector<TestCase> tests{
        {"strict_original_is_admitted", StrictOriginalIsAdmitted},
        {"cross_family_control_is_rejected", CrossFamilyControlIsRejected},
        {"development_model_pool_strict_original_is_admitted",
         DevelopmentModelPoolStrictOriginalIsAdmitted},
        {"changed_candidate_requires_provenance", ChangedCandidateRequiresProvenance},
        {"post_strict_failure_blocks_candidate", PostStrictFailureBlocksCandidate},
        {"hash_mismatch_blocks_candidate", HashMismatchBlocksCandidate},
        {"attribute_mismatch_blocks_candidate", AttributeMismatchBlocksCandidate},
        {"missing_texture_blocks_candidate", MissingTextureBlocksCandidate},
        {"incomplete_audit_blocks_candidate", IncompleteAuditBlocksCandidate},
    };

    bool passed{true};
    for (const TestCase& test : tests)
    {
        const bool current = test.run();
        std::cout << (current ? "PASS: " : "FAIL: ") << test.name << '\n';
        passed = current && passed;
    }
    return passed ? 0 : 1;
}
