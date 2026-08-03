#include "slicer_core/json_value.h"
#include "slicer_core/pipeline/SlicePipeline.h"
#include "slicer_core/rip_reader.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        const auto suffix =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path()
            / ("slicesoft_non_square_raster_" + std::to_string(suffix));
        std::filesystem::create_directories(m_path);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    /**
     * @brief Return the owned temporary directory.
     * @return Temporary directory path.
     */
    const std::filesystem::path& Path() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input{path};
    return slicer_core::Json::parse(input);
}

std::filesystem::path WriteLegacyConfig(
    const std::filesystem::path& directory)
{
    const std::filesystem::path modelPath = std::filesystem::absolute(
        "samples/models/support/internal_void_frame.obj");
    const std::filesystem::path packageDir = directory / "package";
    const std::filesystem::path configPath = directory / "config.json";
    const slicer_core::Json config = slicer_core::Json::object({
        {"input",
         slicer_core::Json::object({
             {"modelPath", modelPath.generic_string()},
             {"format", "obj"},
         })},
        {"output",
         slicer_core::Json::object({
             {"packageDir", packageDir.generic_string()},
             {"dpiX", 635},
             {"dpiY", 600},
             {"layerThicknessMm", 0.1},
             {"storageMode", "stripped"},
             {"tiffCompression",
              slicer_core::Json::object({{"algorithm", "packbits"}})},
             {"rowsPerStrip", 8},
         })},
        {"modelMaterial",
         slicer_core::Json::object({
             {"materialChannel", "RGB"},
             {"applyMode", "solid_volume"},
             {"rgb", slicer_core::Json::array({0, 0, 0})},
         })},
        {"outerVarnish",
         slicer_core::Json::object({
             {"enabled", true},
             {"thicknessMm", 0.081},
             {"allowXYExpansion", true},
             {"conflictPolicy", "varnish_shell_wins"},
             {"value", 0},
         })},
        {"preview",
         slicer_core::Json::object({
             {"enabled", false},
         })},
    });
    std::ofstream output{configPath};
    output << config.dump(2) << '\n';
    return configPath;
}

bool LegacyPipelineUsesIndependentRasterPitches()
{
    TemporaryDirectory directory;
    try
    {
        const slicer_core::SliceRunResult result =
            slicer_core::RunSlicePipeline(
                WriteLegacyConfig(directory.Path()),
                slicer_core::SliceRunOptions{});
        const slicer_core::Json manifest =
            ReadJson(result.package_dir / "manifest.json");
        const slicer_core::Json report =
            ReadJson(result.package_dir / "reports" / "slice_report.json");
        const slicer_core::Json& grid = manifest.at("grid");
        const slicer_core::Json& outerVarnish =
            report.at("totals")
                .at("materialSemantics")
                .at("outerVarnish");
        const slicer_core::RipValidationResult rip =
            slicer_core::validate_slice_package(result.package_dir);

        return ExpectTrue(
                   grid.at("dpiX").as_int() == 635
                       && grid.at("dpiY").as_int() == 600,
                   "Legacy manifest preserves independent DPI")
            && ExpectTrue(
                outerVarnish.at("radiusXPx").as_int() == 3
                    && outerVarnish.at("radiusYPx").as_int() == 2,
                "Legacy report records independent varnish radii")
            && ExpectTrue(
                outerVarnish.at("pixelPitchSource").as_string()
                    == "output_dpi",
                "Legacy varnish uses output DPI as physical source")
            && ExpectTrue(
                rip.dpi_x == 635 && rip.dpi_y == 600,
                "RIP validates the Legacy non-square package")
            && ExpectTrue(
                rip.compression == "packbits"
                    && manifest.at("tiff")
                           .at("compression")
                           .as_string()
                        == "packbits",
                "Legacy package propagates configured PackBits compression");
    }
    catch (const std::exception& error)
    {
        return ExpectTrue(
            false,
            "Legacy non-square raster failed: "
                + std::string{error.what()});
    }
}

}  // namespace

int main()
{
    const bool passed = LegacyPipelineUsesIndependentRasterPitches();
    std::cout << (passed ? "PASS: " : "FAIL: ")
              << "legacy_pipeline_uses_independent_raster_pitches\n";
    return passed ? 0 : 1;
}
