#include "slicer_core/config.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

std::filesystem::path WriteConfig(
    const std::string& name,
    const std::string& outputFields)
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / "slicesoft_12e_09c_output_resolution";
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / name;
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        throw std::runtime_error(
            "failed to write output resolution fixture: " + path.string());
    }

    output
        << "{\n"
        << "  \"input\": {\n"
        << "    \"modelPath\": \"samples/models/sample.stl\",\n"
        << "    \"format\": \"auto\"\n"
        << "  },\n"
        << "  \"output\": {\n"
        << "    \"packageDir\": \"output/OutputResolutionConfigUnit\""
        << outputFields
        << "\n"
        << "  }\n"
        << "}\n";
    return path;
}

bool ExpectLoadedDpi(
    const std::string& name,
    const std::string& outputFields,
    const int expectedDpiX,
    const int expectedDpiY)
{
    const slicer_core::SliceConfig config =
        slicer_core::load_slice_config(WriteConfig(name, outputFields));
    return ExpectTrue(
               config.output.dpi_x == expectedDpiX,
               name + " preserves expected dpiX")
        && ExpectTrue(
               config.output.dpi_y == expectedDpiY,
               name + " preserves expected dpiY");
}

bool ExpectRejectedDpi(
    const std::string& name,
    const std::string& outputFields,
    const std::string& expectedField)
{
    try
    {
        (void)slicer_core::load_slice_config(WriteConfig(name, outputFields));
    }
    catch (const std::runtime_error& error)
    {
        return ExpectTrue(
            std::string{error.what()}.find(expectedField) != std::string::npos,
            name + " reports the invalid DPI field");
    }
    return ExpectTrue(false, name + " must reject invalid DPI");
}

bool TestDefaultAndCompatibleDpi()
{
    return ExpectLoadedDpi("omitted.json", "", 635, 600)
        && ExpectLoadedDpi(
               "legacy_600.json",
               ",\n    \"dpiX\": 600,\n    \"dpiY\": 600",
               600,
               600)
        && ExpectLoadedDpi(
               "production_default.json",
               ",\n    \"dpiX\": 635,\n    \"dpiY\": 600",
               635,
               600);
}

bool TestSupportedRangeBoundaries()
{
    return ExpectLoadedDpi(
               "minimum.json",
               ",\n    \"dpiX\": 72,\n    \"dpiY\": 72",
               72,
               72)
        && ExpectLoadedDpi(
               "maximum.json",
               ",\n    \"dpiX\": 2400,\n    \"dpiY\": 2400",
               2400,
               2400);
}

bool TestInvalidDpiRejected()
{
    return ExpectRejectedDpi(
               "zero_x.json",
               ",\n    \"dpiX\": 0,\n    \"dpiY\": 600",
               "output.dpiX")
        && ExpectRejectedDpi(
               "negative_y.json",
               ",\n    \"dpiX\": 635,\n    \"dpiY\": -1",
               "output.dpiY")
        && ExpectRejectedDpi(
               "too_large_x.json",
               ",\n    \"dpiX\": 2401,\n    \"dpiY\": 600",
               "output.dpiX")
        && ExpectRejectedDpi(
               "too_large_y.json",
               ",\n    \"dpiX\": 635,\n    \"dpiY\": 2401",
               "output.dpiY");
}

bool ExpectLoadedCompression(
    const std::string& name,
    const std::string& outputFields,
    const std::string& expectedAlgorithm)
{
    const slicer_core::SliceConfig config =
        slicer_core::load_slice_config(WriteConfig(name, outputFields));
    return ExpectTrue(
        config.output.tiff_compression == expectedAlgorithm,
        name + " preserves expected TIFF compression");
}

bool TestTiffCompressionConfiguration()
{
    const bool defaultsToNone = ExpectLoadedCompression(
        "compression_omitted.json",
        "",
        "none");
    const bool acceptsNone = ExpectLoadedCompression(
        "compression_none.json",
        ",\n    \"tiffCompression\": {\"algorithm\": \"none\"}",
        "none");
    const bool acceptsPackBits = ExpectLoadedCompression(
        "compression_packbits.json",
        ",\n    \"tiffCompression\": {\"algorithm\": \"packbits\"}",
        "packbits");

    bool rejectsUnsupported{false};
    try
    {
        (void)slicer_core::load_slice_config(WriteConfig(
            "compression_deflate.json",
            ",\n    \"tiffCompression\": {\"algorithm\": \"deflate\"}"));
    }
    catch (const std::runtime_error& error)
    {
        rejectsUnsupported = ExpectTrue(
            std::string{error.what()}.find("output.tiffCompression.algorithm")
                != std::string::npos,
            "unsupported TIFF compression reports the config field");
    }
    return defaultsToNone
        && acceptsNone
        && acceptsPackBits
        && ExpectTrue(
            rejectsUnsupported,
            "unsupported TIFF compression is rejected");
}

}  // namespace

int main()
{
    const bool ok =
        TestDefaultAndCompatibleDpi()
        && TestSupportedRangeBoundaries()
        && TestInvalidDpiRejected()
        && TestTiffCompressionConfiguration();
    if (!ok)
    {
        return 1;
    }

    std::cout
        << "PASS output_resolution_config_unit_tests "
        << "default=635x600 explicit=600x600 range=72..2400 "
        << "compression=none|packbits\n";
    return 0;
}
