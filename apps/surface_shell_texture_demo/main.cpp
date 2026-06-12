#include "slicer_core/materials/texture_application/SurfaceShellTexturePrototype.h"
#include "slicer_core/materials/texture_application/SurfaceShellTextureReport.h"

#include "miniz.h"

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
    slicer_core::SurfaceShellTextureOptions prototype;
    std::filesystem::path outputDir{"output/SurfaceShellTextureBox"};
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
        if (arg == "--case")
        {
            options.prototype.case_name = RequireValue(argc, argv, i, arg);
        }
        else if (arg == "--output")
        {
            options.outputDir = RequireValue(argc, argv, i, arg);
        }
        else if (arg == "--voxel-mm")
        {
            options.prototype.voxel_size_mm = std::stod(RequireValue(argc, argv, i, arg));
        }
        else if (arg == "--shell-mm")
        {
            options.prototype.shell_thickness_mm = std::stod(RequireValue(argc, argv, i, arg));
        }
        else if (arg == "--texture-source")
        {
            options.prototype.texture_source =
                slicer_core::ParseSurfaceShellTextureSource(RequireValue(argc, argv, i, arg));
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout
                << "surface_shell_texture_demo --case generated-box --voxel-mm 0.05 --shell-mm 0.10 "
                << "--texture-source constant|checker --output <dir>\n";
            std::exit(0);
        }
        else
        {
            throw std::runtime_error("unknown argument: " + arg);
        }
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
    void* pngData = tdefl_write_image_to_png_file_in_memory(
        pixels.data(),
        width,
        height,
        3,
        &pngSize);
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

std::string LayerName(const std::string& prefix, const int layer)
{
    std::ostringstream stream;
    stream << prefix << "_layer_" << std::setw(4) << std::setfill('0') << layer << ".png";
    return stream.str();
}

void WritePreviewSet(
    const std::filesystem::path& previewDir,
    const slicer_core::SurfaceShellTextureResult& result)
{
    const int centerLayer = result.shell.depth / 2;
    WritePng(
        previewDir / LayerName("shell", centerLayer),
        result.shell.width,
        result.shell.height,
        slicer_core::BuildSurfaceShellPreviewPixels(result, centerLayer, "shell"));
    WritePng(
        previewDir / LayerName("interior", centerLayer),
        result.shell.width,
        result.shell.height,
        slicer_core::BuildSurfaceShellPreviewPixels(result, centerLayer, "interior"));
    WritePng(
        previewDir / LayerName("composite", centerLayer),
        result.shell.width,
        result.shell.height,
        slicer_core::BuildSurfaceShellPreviewPixels(result, centerLayer, "composite"));
}

int RunDemo(const DemoOptions& options)
{
    std::filesystem::create_directories(options.outputDir / "reports");
    std::filesystem::create_directories(options.outputDir / "preview");

    const slicer_core::SurfaceShellTextureResult result =
        slicer_core::RunSurfaceShellTexturePrototype(options.prototype);
    if (!result.error.empty())
    {
        throw std::runtime_error(result.error);
    }

    const slicer_core::Json report = slicer_core::MakeSurfaceShellTextureReport(result);
    WriteTextFile(options.outputDir / "reports" / "surface_shell_texture_report.json", report.dump(2));
    WritePreviewSet(options.outputDir / "preview", result);

    std::cout << "surface_shell_texture_demo: generated output\n"
              << "  outputDir: " << options.outputDir.generic_string() << "\n"
              << "  case: " << options.prototype.case_name << "\n"
              << "  voxelSizeMm: " << options.prototype.voxel_size_mm << "\n"
              << "  shellThicknessMm: " << options.prototype.shell_thickness_mm << "\n"
              << "  activeVoxels: " << result.level_set.active_voxels << "\n"
              << "  insideVoxels: " << result.shell.inside_voxels << "\n"
              << "  shellVoxels: " << result.shell.shell_voxels << "\n"
              << "  interiorVoxels: " << result.shell.interior_voxels << "\n"
              << "  outsideColoredVoxels: " << result.outside_colored_voxels << "\n";
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
        std::cerr << "surface_shell_texture_demo: " << error.what() << '\n';
        return 1;
    }
}
