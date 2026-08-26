#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"
#include "slicer_core/output/tiff/TiffWriterImplementations.h"
#include "slicer_core/tiff_io.h"
#include "slicer_core/json_value.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

std::filesystem::path TestPath(const std::string& name)
{
    return std::filesystem::temp_directory_path() / ("slicesoft_" + name + ".tiff");
}

std::vector<std::uint8_t> Pixels(const std::uint32_t width, const std::uint32_t height)
{
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width) * height * 7U, 255U);
    pixels[0U] = 10U;
    pixels[1U] = 20U;
    pixels[2U] = 30U;
    pixels[13U] = 0U;
    return pixels;
}

bool RoundTrips(
    const slicer_core::TiffStorageMode storageMode,
    const slicer_core::TiffCompressionMode compressionMode)
{
    const std::filesystem::path path = TestPath(
        std::string{"rgbwsvt_"}
        + (storageMode == slicer_core::TiffStorageMode::Tiled ? "tiled_" : "stripped_")
        + (compressionMode == slicer_core::TiffCompressionMode::PackBits
                ? "packbits"
                : "none"));
    std::error_code error;
    std::filesystem::remove(path, error);
    slicer_core::TiffImageSpec spec;
    spec.width = 17U;
    spec.height = 5U;
    spec.samples_per_pixel = 7U;
    spec.storage_mode = storageMode;
    spec.compression_mode = compressionMode;
    spec.rows_per_strip = 2U;
    spec.tile_width = 16U;
    spec.tile_height = 16U;
    const std::vector<std::uint8_t> pixels = Pixels(spec.width, spec.height);
    slicer_core::write_rgbwsvt_tiff(path, spec, pixels);
    const slicer_core::RgbwsvtTiffReadResult decoded =
        slicer_core::ReadRgbwsvtTiff(path);
    const bool passed = ExpectTrue(decoded.spec.samples_per_pixel == 7U, "TIFF has seven samples")
        && ExpectTrue(
            decoded.spec.compression_mode == compressionMode,
            "RGBWSVT TIFF preserves compression mode")
        && ExpectTrue(decoded.pixels == pixels, "RGBWSVT TIFF round-trips exact bytes")
        && ExpectTrue(decoded.channelStats[6U].print_pixels == 1U, "T statistics see one print pixel")
        && ExpectTrue(decoded.channelStats[6U].empty_pixels == 84U, "T statistics see empty pixels");
    std::filesystem::remove(path, error);
    return passed;
}

bool HandwrittenBackendRejectsSevenSamples()
{
    slicer_core::TiffImageSpec spec;
    spec.width = 1U;
    spec.height = 1U;
    spec.samples_per_pixel = 7U;
    const std::vector<std::uint8_t> pixels(7U, 255U);
    try
    {
        const std::unique_ptr<slicer_core::ITiffWriter> writer =
            slicer_core::detail::CreateHandwrittenTiffWriter();
        writer->Write(TestPath("rgbwsvt_handwritten_reject"), spec, pixels);
    }
    catch (const std::exception&)
    {
        return true;
    }
    return ExpectTrue(false, "handwritten backend must not silently write RGBWSVT");
}

bool ContractSchemaFreezesSevenChannels()
{
#ifdef SLICESOFT_SOURCE_DIR
    const std::filesystem::path path = std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "contracts" / "p0.rgbwsvt.1.schema.json";
#else
    const std::filesystem::path path = std::filesystem::path{"contracts"}
        / "p0.rgbwsvt.1.schema.json";
#endif
    std::ifstream input(path, std::ios::binary);
    if (!ExpectTrue(input.is_open(), "RGBWSVT contract schema is present"))
    {
        return false;
    }
    const slicer_core::Json schema = slicer_core::Json::parse(input);
    const slicer_core::Json& definitions = schema.at("$defs");
    const slicer_core::Json& tiffProperties = definitions.at("tiff").at("properties");
    const auto& channelItems = definitions.at("channelOrder").at("prefixItems").as_array();
    bool passed = ExpectTrue(
        schema.at("properties").at("schema").at("const").as_string()
            == "p0.rgbwsvt.1",
        "schema freezes p0.rgbwsvt.1");
    passed = ExpectTrue(
                 tiffProperties.at("channelCount").at("const").as_int() == 7,
                 "schema freezes seven channels")
        && passed;
    passed = ExpectTrue(
                 channelItems.size() == 7U
                     && channelItems.back().at("const").as_string() == "T",
                 "schema freezes T at channel index 6")
        && passed;
    passed = ExpectTrue(
                 schema.at("properties")
                         .at("productionAcceptance")
                         .at("enum")
                         .size() == 2U,
                 "schema freezes admitted and candidate acceptance values")
        && passed;
    return passed;
}

}  // namespace

int main()
{
    int failures{0};
    failures += RoundTrips(
                    slicer_core::TiffStorageMode::Stripped,
                    slicer_core::TiffCompressionMode::None)
        ? 0
        : 1;
    failures += RoundTrips(
                    slicer_core::TiffStorageMode::Stripped,
                    slicer_core::TiffCompressionMode::PackBits)
        ? 0
        : 1;
    failures += RoundTrips(
                    slicer_core::TiffStorageMode::Tiled,
                    slicer_core::TiffCompressionMode::None)
        ? 0
        : 1;
    failures += RoundTrips(
                    slicer_core::TiffStorageMode::Tiled,
                    slicer_core::TiffCompressionMode::PackBits)
        ? 0
        : 1;
    failures += HandwrittenBackendRejectsSevenSamples() ? 0 : 1;
    failures += ContractSchemaFreezesSevenChannels() ? 0 : 1;
    if (failures != 0)
    {
        std::cerr << "FAIL RgbwsvtTiffIoTests " << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS RgbwsvtTiffIoTests 6/6\n";
    return 0;
}
