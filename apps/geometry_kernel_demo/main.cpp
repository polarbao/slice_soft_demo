#include "slicer_core/geometry/DistanceField2D.h"
#include "slicer_core/geometry/GeometryKernelReport.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/geometry/ShellMask.h"

#include "miniz.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct DemoOptions
{
    std::string caseName{"heightfield-sdf"};
    std::filesystem::path outputDir{"output/GeometryKernelDemo"};
    double shellMm{0.05};
    double thicknessMm{0.05};
};

std::size_t MaskIndex(const int width, const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

DemoOptions ParseOptions(const int argc, char** argv)
{
    DemoOptions options;
    for (int i{1}; i < argc; ++i)
    {
        const std::string arg{argv[i]};
        const auto RequireValue = [&](const std::string& name) -> std::string
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error(name + " requires a value");
            }
            ++i;
            return argv[i];
        };
        if (arg == "--case")
        {
            options.caseName = RequireValue(arg);
        }
        else if (arg == "--output")
        {
            options.outputDir = RequireValue(arg);
        }
        else if (arg == "--shell-mm")
        {
            options.shellMm = std::stod(RequireValue(arg));
        }
        else if (arg == "--thickness-mm")
        {
            options.thicknessMm = std::stod(RequireValue(arg));
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout
                << "geometry_kernel_demo --case <heightfield-sdf|surface-shell|openvdb-smoke|compensated-varnish>"
                << " --output <dir> --shell-mm <value>\n";
            std::exit(0);
        }
        else
        {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    return options;
}

slicer_core::BinaryMask2D MakeFixtureMask()
{
    slicer_core::BinaryMask2D mask;
    mask.width = 64;
    mask.height = 48;
    mask.pixel_size_mm = 0.01;
    mask.inside.resize(static_cast<std::size_t>(mask.width) * static_cast<std::size_t>(mask.height), 0);

    const double centerX = 31.5;
    const double centerY = 23.5;
    for (int y{0}; y < mask.height; ++y)
    {
        for (int x{0}; x < mask.width; ++x)
        {
            const double dx = (static_cast<double>(x) - centerX) / 24.0;
            const double dy = (static_cast<double>(y) - centerY) / 14.0;
            if (dx * dx + dy * dy <= 1.0)
            {
                mask.inside.at(MaskIndex(mask.width, x, y)) = 1;
            }
        }
    }
    return mask;
}

std::vector<std::uint8_t> MakePreviewPixels(
    const slicer_core::BinaryMask2D& mask,
    const slicer_core::ShellMaskResult& shell)
{
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(mask.width) * static_cast<std::size_t>(mask.height) * 3U,
        255);
    for (int y{0}; y < mask.height; ++y)
    {
        for (int x{0}; x < mask.width; ++x)
        {
            const std::size_t index = MaskIndex(mask.width, x, y);
            const std::size_t base = index * 3U;
            if (shell.interior_mask.at(index) != 0)
            {
                pixels.at(base + 0U) = 30;
                pixels.at(base + 1U) = 30;
                pixels.at(base + 2U) = 30;
            }
            if (shell.shell_mask.at(index) != 0)
            {
                pixels.at(base + 0U) = 0;
                pixels.at(base + 1U) = 160;
                pixels.at(base + 2U) = 255;
            }
            if (shell.boundary_mask.at(index) != 0)
            {
                pixels.at(base + 0U) = 255;
                pixels.at(base + 1U) = 80;
                pixels.at(base + 2U) = 0;
            }
        }
    }
    return pixels;
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

void WriteTextFile(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error("failed to write file: " + path.string());
    }
    output << content;
}

int RunDemo(const DemoOptions& options)
{
    std::filesystem::create_directories(options.outputDir / "reports");
    std::filesystem::create_directories(options.outputDir / "preview");

    std::vector<std::string> warnings;
    slicer_core::OpenVdbStatus openVdbStatus = slicer_core::GetOpenVdbStatus();
    slicer_core::BinaryMask2D mask = MakeFixtureMask();
    double shellThicknessMm = options.shellMm;

    if (options.caseName == "openvdb-smoke")
    {
        const slicer_core::OpenVdbSmokeResult smokeResult = slicer_core::RunOpenVdbSmokeCase();
        openVdbStatus = smokeResult.status;
        warnings.insert(warnings.end(), smokeResult.warnings.begin(), smokeResult.warnings.end());
        if (smokeResult.skipped)
        {
            warnings.push_back("openvdb-smoke graceful skip");
        }
    }
    else if (options.caseName == "compensated-varnish")
    {
        warnings.push_back("compensated-varnish is a 09 graceful stub; production varnish is not modified");
        shellThicknessMm = std::max(options.shellMm, options.thicknessMm);
    }
    else if (options.caseName != "heightfield-sdf" && options.caseName != "surface-shell")
    {
        throw std::runtime_error("unsupported geometry kernel case: " + options.caseName);
    }

    const slicer_core::DistanceField2D field = slicer_core::BuildDistanceField2D(mask);
    const slicer_core::DistanceFieldStats distanceStats = slicer_core::ComputeDistanceFieldStats(field);
    const slicer_core::ShellMaskResult shell = slicer_core::BuildShellMask(field, shellThicknessMm);
    const slicer_core::Json report =
        slicer_core::MakeGeometryKernelReport(options.caseName, openVdbStatus, mask, distanceStats, shell, warnings);

    WriteTextFile(options.outputDir / "reports" / "geometry_kernel_report.json", report.dump(2));
    WritePng(
        options.outputDir / "preview" / (options.caseName + ".png"),
        mask.width,
        mask.height,
        MakePreviewPixels(mask, shell));

    std::cout << "geometry_kernel_demo: generated output\n"
              << "  outputDir: " << options.outputDir.generic_string() << "\n"
              << "  case: " << options.caseName << "\n"
              << "  shellPixels: " << shell.shell_pixels << "\n"
              << "  interiorPixels: " << shell.interior_pixels << "\n"
              << "  boundaryPixels: " << shell.boundary_pixels << "\n"
              << "  openvdbCompiled: " << (openVdbStatus.compiled_with_openvdb ? "true" : "false") << "\n";
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
        std::cerr << "geometry_kernel_demo: " << error.what() << '\n';
        return 1;
    }
}
