#include "slicer_core/diagnostics/RepairedAssetIntakeReport.h"
#include "slicer_core/json_value.h"
#include "slicer_core/preflight/RepairedAssetIntakeService.h"

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

struct Options
{
    std::filesystem::path manifest_path;
    std::filesystem::path output_path;
};

std::string RequireValue(
    const int argc,
    char** argv,
    int& index,
    const std::string& argument)
{
    if (index + 1 >= argc)
    {
        throw std::runtime_error(argument + " requires a value");
    }
    ++index;
    return argv[index];
}

Options ParseOptions(const int argc, char** argv)
{
    Options options;
    for (int index{1}; index < argc; ++index)
    {
        const std::string argument{argv[index]};
        if (argument == "--manifest")
        {
            options.manifest_path = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--output")
        {
            options.output_path = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--help" || argument == "-h")
        {
            std::cout
                << "repaired_asset_intake --manifest <intake.json> "
                << "--output <report.json>\n";
            std::exit(0);
        }
        else
        {
            throw std::runtime_error("unknown argument: " + argument);
        }
    }
    if (options.manifest_path.empty())
    {
        throw std::runtime_error("--manifest is required");
    }
    if (options.output_path.empty())
    {
        throw std::runtime_error("--output is required");
    }
    return options;
}

std::filesystem::path ResolvePath(
    const std::filesystem::path& manifestPath,
    const std::string& value)
{
    const std::filesystem::path path{value};
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }
    const std::filesystem::path directory = manifestPath.parent_path().empty()
        ? std::filesystem::current_path()
        : manifestPath.parent_path();
    return std::filesystem::absolute(directory / path).lexically_normal();
}

slicer_core::RepairedAssetProvenance ParseProvenance(
    const slicer_core::Json& manifest)
{
    slicer_core::RepairedAssetProvenance provenance;
    if (!manifest.contains("provenance"))
    {
        return provenance;
    }
    const slicer_core::Json& value = manifest.at("provenance");
    provenance.provider = value.value("provider", std::string{});
    provenance.tool = value.value("tool", std::string{});
    provenance.tool_version = value.value("toolVersion", std::string{});
    provenance.operation_summary = value.value(
        "operationSummary",
        std::string{});
    provenance.timestamp = value.value("timestamp", std::string{});
    provenance.operator_id = value.value("operator", std::string{});
    return provenance;
}

slicer_core::RepairedAssetApproval ParseApproval(
    const slicer_core::Json& manifest)
{
    slicer_core::RepairedAssetApproval approval;
    if (!manifest.contains("approval"))
    {
        return approval;
    }
    const slicer_core::Json& value = manifest.at("approval");
    approval.max_dimension_delta_mm = value.value(
        "maxDimensionDeltaMm",
        approval.max_dimension_delta_mm);
    approval.allow_attribute_changes = value.value(
        "allowAttributeChanges",
        approval.allow_attribute_changes);
    approval.attribute_change_reason = value.value(
        "attributeChangeReason",
        std::string{});
    return approval;
}

slicer_core::ModelPreflightOptions ParsePreflightOptions(
    const slicer_core::Json& manifest)
{
    slicer_core::ModelPreflightOptions options;
    if (!manifest.contains("preflight"))
    {
        return options;
    }
    const slicer_core::Json& value = manifest.at("preflight");
    options.voxelMm = value.value("voxelMm", options.voxelMm);
    options.maxCompleteSelfIntersectionCandidatePairs =
        static_cast<std::uint64_t>(value.value(
            "maxCompleteSelfIntersectionCandidatePairs",
            static_cast<int>(
                options.maxCompleteSelfIntersectionCandidatePairs)));
    return options;
}

slicer_core::RepairedAssetIntakeRequest ParseManifest(
    const std::filesystem::path& manifestPath)
{
    std::ifstream input{manifestPath, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error(
            "failed to read intake manifest: " + manifestPath.string());
    }
    const slicer_core::Json manifest = slicer_core::Json::parse(input);
    if (manifest.value("schema", std::string{})
        != "slicesoft.repaired_asset_intake_manifest.12e_08c_r4.1")
    {
        throw std::runtime_error("unsupported intake manifest schema");
    }

    slicer_core::RepairedAssetIntakeRequest request;
    request.family_id = manifest.value("familyId", std::string{});
    request.candidate_id = manifest.value("candidateId", std::string{});
    request.candidate_kind = slicer_core::ParseRepairedAssetCandidateKind(
        manifest.value("candidateKind", std::string{}));
    request.original_config_path = ResolvePath(
        manifestPath,
        manifest.value("originalConfig", std::string{}));
    request.candidate_config_path = ResolvePath(
        manifestPath,
        manifest.value("candidateConfig", std::string{}));
    request.expected_original_source_hash = manifest.value(
        "expectedOriginalSourceHash",
        std::string{});
    request.expected_candidate_source_hash = manifest.value(
        "expectedCandidateSourceHash",
        std::string{});
    request.provenance = ParseProvenance(manifest);
    request.approval = ParseApproval(manifest);
    request.preflight_options = ParsePreflightOptions(manifest);
    return request;
}

void WriteReport(
    const std::filesystem::path& outputPath,
    const slicer_core::Json& report)
{
    if (!outputPath.parent_path().empty())
    {
        std::filesystem::create_directories(outputPath.parent_path());
    }
    std::ofstream output{outputPath, std::ios::binary};
    if (!output)
    {
        throw std::runtime_error(
            "failed to write intake report: " + outputPath.string());
    }
    output << report.dump(2) << '\n';
}

int Run(const Options& options)
{
    const slicer_core::RepairedAssetIntakeRequest request = ParseManifest(
        options.manifest_path);
    const slicer_core::RepairedAssetIntakeService service;
    const slicer_core::RepairedAssetIntakeResult result = service.Run(request);
    WriteReport(
        options.output_path,
        slicer_core::BuildRepairedAssetIntakeReport(result));

    std::cout
        << "repaired_asset_intake: evidence collected\n"
        << "  familyId: " << result.family_id << '\n'
        << "  candidateId: " << result.candidate_id << '\n'
        << "  status: " << (result.admitted ? "admitted" : "blocked") << '\n'
        << "  requiredFamilyPassCount: "
        << result.required_family_pass_count << '\n'
        << "  productionOutputWritten: false\n"
        << "  report: " << options.output_path.generic_string() << '\n';
    return result.admitted ? 0 : 2;
}

}  // namespace

int main(const int argc, char** argv)
{
    try
    {
        return Run(ParseOptions(argc, argv));
    }
    catch (const std::exception& error)
    {
        std::cerr << "repaired_asset_intake error: " << error.what() << '\n';
        return 1;
    }
}
