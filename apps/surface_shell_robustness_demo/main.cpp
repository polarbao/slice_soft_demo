#include "slicer_core/config.h"
#include "slicer_core/materials/texture_application/SurfaceShellBenchmarkReport.h"
#include "slicer_core/materials/texture_application/SurfaceShellRealModelPrototype.h"
#include "slicer_core/materials/texture_application/SurfaceShellRealModelReport.h"
#include "slicer_core/model.h"

#include "miniz.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct DemoOptions
{
    std::filesystem::path configPath;
    std::filesystem::path outputDir{"output/SurfaceShellRobustness"};
    std::string fixtureId{"manual"};
    std::string buildConfig{"Debug"};
    double voxelMm{0.05};
    double shellMm{0.10};
    double maxTransferMm{0.0};
    slicer_core::MeshValidationPolicy meshPolicy{slicer_core::MeshValidationPolicy::StrictClosed};
};

std::string RequireValue(const int argc, char** argv, int& index, const std::string& name)
{
    if (index + 1 >= argc)
    {
        throw std::runtime_error(name + " requires a value");
    }
    ++index;
    return argv[index];
}

DemoOptions ParseOptions(const int argc, char** argv)
{
    DemoOptions options;
    for (int i{1}; i < argc; ++i)
    {
        const std::string arg{argv[i]};
        if (arg == "--config")
        {
            options.configPath = RequireValue(argc, argv, i, arg);
        }
        else if (arg == "--output")
        {
            options.outputDir = RequireValue(argc, argv, i, arg);
        }
        else if (arg == "--fixture-id")
        {
            options.fixtureId = RequireValue(argc, argv, i, arg);
        }
        else if (arg == "--build-config")
        {
            options.buildConfig = RequireValue(argc, argv, i, arg);
        }
        else if (arg == "--voxel-mm")
        {
            options.voxelMm = std::stod(RequireValue(argc, argv, i, arg));
        }
        else if (arg == "--shell-mm")
        {
            options.shellMm = std::stod(RequireValue(argc, argv, i, arg));
        }
        else if (arg == "--max-transfer-mm")
        {
            options.maxTransferMm = std::stod(RequireValue(argc, argv, i, arg));
        }
        else if (arg == "--mesh-policy")
        {
            options.meshPolicy = slicer_core::ParseMeshValidationPolicy(RequireValue(argc, argv, i, arg));
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout
                << "surface_shell_robustness_demo --config <config.json> --output <dir> "
                << "--fixture-id <id> --voxel-mm <value> --shell-mm <value> "
                << "--mesh-policy strict_closed|warn_and_attempt --build-config <name>\n";
            std::exit(0);
        }
        else
        {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    if (options.configPath.empty())
    {
        throw std::runtime_error("--config is required");
    }
    return options;
}

void WriteTextFile(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error("failed to write file: " + path.string());
    }
    output << content;
}

void WritePng(
    const std::filesystem::path& path,
    const int width,
    const int height,
    const std::vector<std::uint8_t>& pixels)
{
    std::size_t pngSize{0};
    void* pngData = tdefl_write_image_to_png_file_in_memory(pixels.data(), width, height, 3, &pngSize);
    if (pngData == nullptr)
    {
        throw std::runtime_error("failed to encode PNG preview");
    }
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        mz_free(pngData);
        throw std::runtime_error("failed to open preview path: " + path.string());
    }
    output.write(static_cast<const char*>(pngData), static_cast<std::streamsize>(pngSize));
    mz_free(pngData);
}

int SelectPreviewLayer(const slicer_core::OpenVdbSurfaceShellResult& shell)
{
    int selectedLayer{0};
    int selectedCount{-1};
    for (int z{0}; z < shell.depth; ++z)
    {
        int count{0};
        for (int y{0}; y < shell.height; ++y)
        {
            for (int x{0}; x < shell.width; ++x)
            {
                count += shell.shell_mask.at(slicer_core::MaskIndex3D(shell.width, shell.height, x, y, z)) != 0 ? 1 : 0;
            }
        }
        if (count > selectedCount)
        {
            selectedCount = count;
            selectedLayer = z;
        }
    }
    return selectedLayer;
}

std::string LayerName(const std::string& prefix, const int layer)
{
    std::ostringstream stream;
    stream << prefix << "_layer_" << std::setw(4) << std::setfill('0') << layer << ".png";
    return stream.str();
}

void WritePreviewSet(
    const std::filesystem::path& previewDir,
    slicer_core::SurfaceShellRealModelResult& result)
{
    const auto start = std::chrono::steady_clock::now();
    const int layer = SelectPreviewLayer(result.shell);
    for (const std::string& mode : {std::string{"shell"}, std::string{"interior"}, std::string{"composite"}})
    {
        const std::vector<std::uint8_t> pixels =
            slicer_core::BuildSurfaceShellRealModelPreviewPixels(result, layer, mode);
        result.performance.preview_buffer_bytes = std::max<std::uint64_t>(
            result.performance.preview_buffer_bytes,
            static_cast<std::uint64_t>(pixels.size()));
        WritePng(previewDir / LayerName(mode, layer), result.shell.width, result.shell.height, pixels);
    }
    result.performance.preview_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    result.performance.peak_estimated_bytes += result.performance.preview_buffer_bytes;
}

int RunDemo(const DemoOptions& options)
{
    std::filesystem::create_directories(options.outputDir / "reports");
    std::filesystem::create_directories(options.outputDir / "preview");

    const auto importStart = std::chrono::steady_clock::now();
    slicer_core::SliceConfig config = slicer_core::load_slice_config(options.configPath);
    const slicer_core::SceneModel scene = slicer_core::load_model_report(config, options.configPath.parent_path());
    const double importMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - importStart).count();

    slicer_core::SurfaceShellRealModelOptions prototypeOptions;
    prototypeOptions.voxel_size_mm = options.voxelMm;
    prototypeOptions.shell_thickness_mm = options.shellMm;
    prototypeOptions.max_transfer_distance_mm = options.maxTransferMm > 0.0
        ? options.maxTransferMm
        : options.shellMm + std::sqrt(3.0) * options.voxelMm;
    prototypeOptions.mesh_policy = options.meshPolicy;
    prototypeOptions.fallback_rgb = config.texture.fallback_rgb;
    prototypeOptions.texture_sample.sampler = config.texture.sampler;
    prototypeOptions.texture_sample.uv_address_mode = config.texture.uv_address_mode;
    prototypeOptions.texture_sample.flip_v = config.texture.flip_v;

    slicer_core::SurfaceShellRealModelResult result =
        slicer_core::RunSurfaceShellRealModelPrototype(scene, config, prototypeOptions);
    result.config_path = options.configPath.generic_string();
    result.performance.import_ms = importMs;

    if (result.errors.empty())
    {
        WritePreviewSet(options.outputDir / "preview", result);
    }

    WriteTextFile(
        options.outputDir / "reports" / "surface_shell_texture_report.json",
        slicer_core::MakeSurfaceShellRealModelReport(result).dump(2));
    WriteTextFile(
        options.outputDir / "reports" / "surface_shell_benchmark_report.json",
        slicer_core::MakeSurfaceShellBenchmarkReport(result, options.fixtureId, options.buildConfig).dump(2));

    if (!result.errors.empty())
    {
        std::cerr << "surface_shell_robustness_demo: " << result.errors.front() << '\n';
        return 2;
    }

    std::cout << "surface_shell_robustness_demo: generated output\n"
              << "  fixture: " << options.fixtureId << "\n"
              << "  format: " << result.input_format << "\n"
              << "  triangles: " << result.adapted_mesh.topology.accepted_triangles << "\n"
              << "  components: " << result.robustness.connected_components << "\n"
              << "  duplicateFaces: " << result.robustness.duplicate_faces << "\n"
              << "  selfIntersectionPairs: " << result.robustness.self_intersection_pairs << "\n"
              << "  activeVoxels: " << result.level_set.active_voxels << "\n"
              << "  shellVoxels: " << result.shell.shell_voxels << "\n"
              << "  sampledTextureVoxels: " << result.transfer.stats.sampled_texture_voxels << "\n"
              << "  bvhVisitedNodes: " << result.transfer.stats.nearest_query_stats.visited_nodes << "\n";
    return 0;
}

}  // namespace

int main(const int argc, char** argv)
{
    try
    {
        return RunDemo(ParseOptions(argc, argv));
    }
    catch (const std::exception& error)
    {
        std::cerr << "surface_shell_robustness_demo: " << error.what() << '\n';
        return 1;
    }
}
