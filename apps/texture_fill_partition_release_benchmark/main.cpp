#include "slicer_core/config.h"
#include "slicer_core/diagnostics/TextureFillPartitionReleaseBenchmark.h"
#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/model.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Clock = std::chrono::steady_clock;

struct Options
{
    std::filesystem::path configPath;
    std::filesystem::path outputPath;
    std::string caseName;
    double voxelMm{0.10};
    double widthMm{0.20};
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
        else if (argument == "--case-name")
        {
            options.caseName = RequireValue(argc, argv, index, argument);
        }
        else if (argument == "--voxel-mm")
        {
            options.voxelMm = std::stod(RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--width-mm")
        {
            options.widthMm = std::stod(RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--padding-voxels")
        {
            options.paddingVoxels = std::stoi(RequireValue(argc, argv, index, argument));
        }
        else if (argument == "--help" || argument == "-h")
        {
            std::cout
                << "texture_fill_partition_release_benchmark --config <config.json> "
                << "--output <report.json> [--case-name <name>] "
                << "[--voxel-mm <value>] [--width-mm <value>] "
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
    if (options.caseName.empty())
    {
        options.caseName = options.configPath.stem().string();
    }
    return options;
}

double ElapsedMilliseconds(const Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(
        Clock::now() - start).count();
}

std::string BuildTypeName()
{
#ifdef NDEBUG
    return "Release";
#else
    return "Debug";
#endif
}

void WriteReport(
    const std::filesystem::path& outputPath,
    const slicer_core::Json& report)
{
    if (!outputPath.parent_path().empty())
    {
        std::filesystem::create_directories(outputPath.parent_path());
    }
    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error("failed to write benchmark report: " + outputPath.string());
    }
    output << report.dump(2) << '\n';
}

int RunBenchmark(const Options& options)
{
    const Clock::time_point configStart = Clock::now();
    const slicer_core::SliceConfig config =
        slicer_core::load_slice_config(options.configPath);
    const double configLoadMs = ElapsedMilliseconds(configStart);

    const std::filesystem::path configDirectory =
        options.configPath.parent_path().empty()
        ? std::filesystem::current_path()
        : options.configPath.parent_path();
    const Clock::time_point modelStart = Clock::now();
    const slicer_core::SceneModel scene =
        slicer_core::load_model_report(config, configDirectory);
    const double modelLoadMs = ElapsedMilliseconds(modelStart);

    const Clock::time_point adaptStart = Clock::now();
    const slicer_core::AdaptedTriangleMesh adapted =
        slicer_core::AdaptSceneModelToTriangleMesh(scene);
    const double meshAdaptMs = ElapsedMilliseconds(adaptStart);

    slicer_core::TextureFillPartitionReleaseBenchmarkRequest request;
    request.mesh = &adapted.mesh;
    request.caseName = options.caseName;
    request.configPath = options.configPath.generic_string();
    request.modelPath = scene.model_path.generic_string();
    request.buildType = BuildTypeName();
    request.voxelMm = options.voxelMm;
    request.widthMm = options.widthMm;
    request.paddingVoxels = options.paddingVoxels;
    request.configLoadMs = configLoadMs;
    request.modelLoadMs = modelLoadMs;
    request.meshAdaptMs = meshAdaptMs;
    request.sourceTriangles = adapted.topology.source_triangles;
    request.acceptedTriangles = adapted.topology.accepted_triangles;
    request.degenerateTriangles = adapted.topology.degenerate_triangles;
    request.boundaryEdges = adapted.topology.boundary_edges;
    request.nonManifoldEdges = adapted.topology.non_manifold_edges;

    const slicer_core::TextureFillPartitionReleaseBenchmarkResult result =
        slicer_core::RunTextureFillPartitionReleaseBenchmark(request);
    WriteReport(options.outputPath, result.report);

    std::cout
        << "texture_fill_partition_release_benchmark: evidence collected\n"
        << "  case: " << options.caseName << '\n'
        << "  status: " << result.partition.status << '\n'
        << "  partitionPass: " << (result.partition.partitionPass ? "true" : "false") << '\n'
        << "  totalCoreMs: " << result.partition.performance.totalCoreMs << '\n'
        << "  peakWorkingSetBytes: "
        << result.partition.performance.processPeakWorkingSetBytes << '\n'
        << "  productionAdmitted: false\n"
        << "  report: " << options.outputPath.generic_string() << '\n';
    return 0;
}

}  // namespace

int main(const int argc, char** argv)
{
    try
    {
        return RunBenchmark(ParseOptions(argc, argv));
    }
    catch (const std::exception& error)
    {
        std::cerr << "texture_fill_partition_release_benchmark error: "
                  << error.what() << '\n';
        return 1;
    }
}
