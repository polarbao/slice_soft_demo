#include "slicer_core/config.h"
#include "slicer_core/diagnostics/TextureFillPartitionPositiveMatrix.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/materials/process_profile/MaterialProcessProfile.h"
#include "slicer_core/model.h"
#include "slicer_core/preflight/ModelPreflightService.h"
#include "slicer_core/preflight/ModelPreflightTypes.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

struct Options
{
    std::filesystem::path configPath;
    std::filesystem::path outputPath;
    std::string caseId;
    double voxelMm{0.20};
    int paddingVoxels{1};
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
        if (argument == "--config")
        {
            options.configPath = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--output")
        {
            options.outputPath = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--case-id")
        {
            options.caseId = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--voxel-mm")
        {
            options.voxelMm = std::stod(
                RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--padding-voxels")
        {
            options.paddingVoxels = std::stoi(
                RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--help" || argument == "-h")
        {
            std::cout
                << "texture_fill_partition_positive_matrix "
                << "--config <config.json> --output <report.json> "
                << "--case-id <id> [--voxel-mm <value>] "
                << "[--padding-voxels <count>]\n";
            std::exit(0);
        }
        else
        {
            throw std::runtime_error("unknown argument: " + argument);
        }
    }
    if (options.configPath.empty())
    {
        throw std::runtime_error("--config is required");
    }
    if (options.outputPath.empty())
    {
        throw std::runtime_error("--output is required");
    }
    if (options.caseId.empty())
    {
        throw std::runtime_error("--case-id is required");
    }
    if (options.voxelMm <= 0.0)
    {
        throw std::runtime_error("--voxel-mm must be positive");
    }
    if (options.paddingVoxels < 0)
    {
        throw std::runtime_error("--padding-voxels must not be negative");
    }
    return options;
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
            "failed to write R4-05 report: " + outputPath.string());
    }
    output << report.dump(2) << '\n';
}

int Run(const Options& options)
{
    slicer_core::ModelPreflightRequest preflightRequest;
    preflightRequest.configPath = options.configPath;
    preflightRequest.options.voxelMm = options.voxelMm;
    slicer_core::ModelPreflightService preflightService;
    const slicer_core::ModelPreflightExecutionResult preflight =
        preflightService.Run(preflightRequest);

    slicer_core::TextureFillPartitionPositiveMatrixRequest request;
    request.preflight = &preflight.result;
    request.caseId = options.caseId;
    request.sourceHash = preflight.result.identity.sourceHash;
    request.resourceHash = preflight.result.identity.resourceHash;
    request.preflightStatus =
        slicer_core::ModelPreflightStatusName(preflight.result.status);
    request.voxelMm = options.voxelMm;
    request.paddingVoxels = options.paddingVoxels;
    if (request.preflightStatus != "passed")
    {
        const slicer_core::TextureFillPartitionPositiveMatrixResult result =
            slicer_core::RunTextureFillPartitionPositiveMatrix(request);
        WriteReport(options.outputPath, result.report);
        std::cout
            << "texture_fill_partition_positive_matrix: preflight blocked\n"
            << "  caseId: " << options.caseId << '\n'
            << "  preflight: " << request.preflightStatus << '\n'
            << "  productionOutputWritten: false\n"
            << "  report: " << options.outputPath.generic_string() << '\n';
        return 2;
    }

    const slicer_core::SliceConfig config =
        slicer_core::load_slice_config(options.configPath);
    const std::filesystem::path configDirectory =
        options.configPath.parent_path().empty()
        ? std::filesystem::current_path()
        : options.configPath.parent_path();
    const slicer_core::SceneModel scene =
        slicer_core::load_model_report(config, configDirectory);
    const slicer_core::AdaptedTriangleMesh adapted =
        slicer_core::AdaptSceneModelToTriangleMesh(scene);

    request.adaptedMesh = &adapted;
    request.modelPath = scene.model_path.generic_string();
    request.textureSample.sampler = config.texture.sampler;
    request.textureSample.uv_address_mode = config.texture.uv_address_mode;
    request.textureSample.flip_v = config.texture.flip_v;
    request.fallbackRgb = config.texture.fallback_rgb;
    request.missingTexturePolicy = config.texture.missing_texture_policy;
    request.modelFillRgb = config.material.rgb;
    request.modelFillValue = config.model_fill.value;
    request.profile = slicer_core::MakeMaterialProcessProfileBoundary(
        config.material_process_profile);

    const slicer_core::TextureFillPartitionPositiveMatrixResult result =
        slicer_core::RunTextureFillPartitionPositiveMatrix(request);
    WriteReport(options.outputPath, result.report);

    std::cout
        << "texture_fill_partition_positive_matrix: evidence collected\n"
        << "  caseId: " << options.caseId << '\n'
        << "  preflight: " << request.preflightStatus << '\n'
        << "  matrixPass: " << (result.matrixPass ? "true" : "false")
        << '\n'
        << "  widthSamples: " << result.widthSweep.samples.size() << '\n'
        << "  materialCases: " << result.materialCases.size() << '\n'
        << "  productionOutputWritten: false\n"
        << "  requiredRepairPassCount: 0\n"
        << "  report: " << options.outputPath.generic_string() << '\n';
    return result.matrixPass ? 0 : 2;
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
        std::cerr
            << "texture_fill_partition_positive_matrix error: "
            << error.what()
            << '\n';
        return 1;
    }
}
