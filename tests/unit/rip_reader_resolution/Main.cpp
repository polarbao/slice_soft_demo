#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/rip_reader.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>

namespace
{

constexpr int kWidth{4};
constexpr int kHeight{3};
constexpr int kLayerCount{1};
constexpr std::size_t kChannelCount{6U};

class TemporaryDirectory final
{
public:
    explicit TemporaryDirectory(const std::string& name)
    {
        const auto suffix =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path()
            / ("slicesoft_rip_resolution_" + name + "_"
               + std::to_string(suffix));
        std::filesystem::create_directories(m_path);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

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
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error("failed to read JSON: " + path.string());
    }
    return slicer_core::Json::parse(input);
}

void WriteJson(
    const std::filesystem::path& path,
    const slicer_core::Json& document)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error("failed to write JSON: " + path.string());
    }
    output << document.dump(2) << '\n';
}

slicer_core::RgbwsvProductionPackageWriteRequest MakeRequest(
    const std::filesystem::path& packageDir,
    const int dpiX,
    const int dpiY)
{
    slicer_core::RgbwsvProductionPackageWriteRequest request;
    request.packageDir = packageDir;
    request.sourceConfigPath = "fixture/dpi.json";
    request.sourceModelPath = "fixture/model.obj";
    request.sourceFormat = "obj";
    request.requestedPipelineMode = "legacy";
    request.effectivePipelineMode = "legacy";
    request.productionAcceptance = "admitted";
    request.grid.widthPx = kWidth;
    request.grid.heightPx = kHeight;
    request.grid.layerCount = kLayerCount;
    request.grid.dpiX = dpiX;
    request.grid.dpiY = dpiY;
    request.grid.pixelSizeXmm = 25.4 / static_cast<double>(dpiX);
    request.grid.pixelSizeYmm = 25.4 / static_cast<double>(dpiY);
    request.grid.layerThicknessMm = 0.01;
    request.preview.enabled = false;

    slicer_core::RgbwsvProductionLayer layer;
    layer.layerIndex = 0;
    layer.zMm = 0.005;
    layer.widthPx = kWidth;
    layer.heightPx = kHeight;
    layer.channels.assign(
        static_cast<std::size_t>(kWidth * kHeight) * kChannelCount,
        255U);
    layer.channels.at(0U) = 0U;
    request.layers.push_back(std::move(layer));
    return request;
}

void GeneratePackage(
    const std::filesystem::path& packageDir,
    const int dpiX = 635,
    const int dpiY = 600)
{
    (void)slicer_core::WriteRgbwsvProductionPackage(
        MakeRequest(packageDir, dpiX, dpiY));
}

void MutateGrid(
    const std::filesystem::path& packageDir,
    const std::function<void(slicer_core::Json::Object&)>& mutation)
{
    const std::filesystem::path manifestPath =
        packageDir / "manifest.json";
    slicer_core::Json::Object root = ReadJson(manifestPath).as_object();
    slicer_core::Json::Object grid = root.at("grid").as_object();
    mutation(grid);
    root["grid"] = slicer_core::Json{std::move(grid)};
    WriteJson(manifestPath, slicer_core::Json{std::move(root)});
}

bool ExpectGridInvalid(
    const std::string& name,
    const std::function<void(slicer_core::Json::Object&)>& mutation,
    const std::string& expectedField)
{
    TemporaryDirectory directory{name};
    const std::filesystem::path packageDir = directory.Path() / "package";
    GeneratePackage(packageDir);
    MutateGrid(packageDir, mutation);

    try
    {
        (void)slicer_core::validate_slice_package(packageDir);
    }
    catch (const slicer_core::ValidationError& error)
    {
        return ExpectTrue(
                   error.code() == slicer_core::ValidationErrorCode::GridInvalid,
                   name + " returns E_GRID_INVALID")
            && ExpectTrue(
                std::string{error.what()}.find(expectedField)
                    != std::string::npos,
                name + " identifies " + expectedField);
    }
    return ExpectTrue(false, name + " must fail closed");
}

bool TestNonSquareDpiPackagePasses()
{
    TemporaryDirectory directory{"positive_635_600"};
    const std::filesystem::path packageDir = directory.Path() / "package";
    try
    {
        GeneratePackage(packageDir);
        const slicer_core::RipValidationResult result =
            slicer_core::validate_slice_package(packageDir);
        return ExpectTrue(result.dpi_x == 635, "RIP preserves dpiX=635")
            && ExpectTrue(result.dpi_y == 600, "RIP preserves dpiY=600")
            && ExpectTrue(
                slicer_core::IsOutputPixelSizeConsistent(
                    result.dpi_x,
                    result.pixel_size_x_mm),
                "RIP validates X pixel size")
            && ExpectTrue(
                slicer_core::IsOutputPixelSizeConsistent(
                    result.dpi_y,
                    result.pixel_size_y_mm),
                "RIP validates Y pixel size");
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL non-square package: " << error.what() << '\n';
        return false;
    }
}

bool TestLegacyDpiPackagePasses()
{
    TemporaryDirectory directory{"positive_600_600"};
    const std::filesystem::path packageDir = directory.Path() / "package";
    try
    {
        GeneratePackage(packageDir, 600, 600);
        const slicer_core::RipValidationResult result =
            slicer_core::validate_slice_package(packageDir);
        return ExpectTrue(
            result.dpi_x == 600 && result.dpi_y == 600,
            "RIP keeps explicit legacy 600/600");
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL legacy package: " << error.what() << '\n';
        return false;
    }
}

bool TestOptionalRedundantArraysMayBeAbsent()
{
    TemporaryDirectory directory{"optional_arrays"};
    const std::filesystem::path packageDir = directory.Path() / "package";
    try
    {
        GeneratePackage(packageDir);
        MutateGrid(
            packageDir,
            [](slicer_core::Json::Object& grid)
            {
                grid.erase("dpi");
                grid.erase("pixelSizeMm");
            });
        const slicer_core::RipValidationResult result =
            slicer_core::validate_slice_package(packageDir);
        return ExpectTrue(
            result.dpi_x == 635 && result.dpi_y == 600,
            "RIP accepts absent redundant arrays");
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL optional arrays: " << error.what() << '\n';
        return false;
    }
}

bool TestInvalidGridPackagesFailClosed()
{
    return ExpectGridInvalid(
               "missing_dpix",
               [](slicer_core::Json::Object& grid)
               {
                   grid.erase("dpiX");
               },
               "manifest.grid.dpiX")
        && ExpectGridInvalid(
            "zero_dpix",
            [](slicer_core::Json::Object& grid)
            {
                grid["dpiX"] = 0;
            },
            "manifest.grid.dpiX")
        && ExpectGridInvalid(
            "high_dpiy",
            [](slicer_core::Json::Object& grid)
            {
                grid["dpiY"] = slicer_core::kMaximumOutputDpi + 1;
            },
            "manifest.grid.dpiY")
        && ExpectGridInvalid(
            "fractional_dpix",
            [](slicer_core::Json::Object& grid)
            {
                grid["dpiX"] = 635.5;
            },
            "manifest.grid.dpiX")
        && ExpectGridInvalid(
            "inconsistent_dpi_array",
            [](slicer_core::Json::Object& grid)
            {
                grid["dpi"] = slicer_core::Json::array({600, 600});
            },
            "manifest.grid.dpi")
        && ExpectGridInvalid(
            "missing_pixel_size_x",
            [](slicer_core::Json::Object& grid)
            {
                grid.erase("pixelSizeXmm");
            },
            "manifest.grid.pixelSizeXmm")
        && ExpectGridInvalid(
            "inconsistent_pixel_size_x",
            [](slicer_core::Json::Object& grid)
            {
                grid["pixelSizeXmm"] = 25.4 / 600.0;
            },
            "manifest.grid.pixelSizeXmm")
        && ExpectGridInvalid(
            "inconsistent_pixel_size_array",
            [](slicer_core::Json::Object& grid)
            {
                grid["pixelSizeMm"] = slicer_core::Json::array(
                    {25.4 / 600.0, 25.4 / 600.0});
            },
            "manifest.grid.pixelSizeMm");
}

}  // namespace

int main()
{
    const bool passed = TestNonSquareDpiPackagePasses()
        && TestLegacyDpiPackagePasses()
        && TestOptionalRedundantArraysMayBeAbsent()
        && TestInvalidGridPackagesFailClosed();
    if (!passed)
    {
        return 1;
    }
    std::cout
        << "PASS rip_reader_resolution_unit_tests dpi=635x600 "
           "legacy=600x600 bad-grid=8\n";
    return 0;
}
