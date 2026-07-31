#include "slicer_core/output/tiff/TiffWriterFactory.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct TagEntry
{
    std::uint16_t tag{0};
    std::uint16_t type{0};
    std::uint32_t count{0};
    std::uint32_t valueoffset{0};
};

struct ParsedTiff
{
    std::vector<std::uint8_t> bytes;
    std::map<std::uint16_t, TagEntry> entries;
    std::uint32_t nextifdoffset{0};
};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

template <typename T>
bool ExpectEqual(
    const T& actual,
    const T& expected,
    const std::string& message)
{
    if (actual != expected)
    {
        std::cerr
            << "FAIL: " << message
            << " expected=" << expected
            << " actual=" << actual << '\n';
        return false;
    }
    return true;
}

std::uint16_t ReadU16(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset)
{
    if (offset + 2U > bytes.size())
    {
        throw std::runtime_error("test TIFF uint16 read outside file");
    }
    return static_cast<std::uint16_t>(
        bytes.at(offset)
        | (static_cast<std::uint16_t>(bytes.at(offset + 1U)) << 8U));
}

std::uint32_t ReadU32(
    const std::vector<std::uint8_t>& bytes,
    const std::size_t offset)
{
    if (offset + 4U > bytes.size())
    {
        throw std::runtime_error("test TIFF uint32 read outside file");
    }
    return static_cast<std::uint32_t>(bytes.at(offset))
        | (static_cast<std::uint32_t>(bytes.at(offset + 1U)) << 8U)
        | (static_cast<std::uint32_t>(bytes.at(offset + 2U)) << 16U)
        | (static_cast<std::uint32_t>(bytes.at(offset + 3U)) << 24U);
}

std::size_t TiffTypeSize(const std::uint16_t type)
{
    switch (type)
    {
        case 1:
        case 2:
            return 1U;
        case 3:
            return 2U;
        case 4:
            return 4U;
        default:
            throw std::runtime_error(
                "unsupported test TIFF field type: "
                + std::to_string(type));
    }
}

ParsedTiff ParseTiff(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error(
            "failed to open test TIFF: " + path.string());
    }

    ParsedTiff parsed;
    parsed.bytes = {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    if (parsed.bytes.size() < 8U
        || parsed.bytes.at(0) != 'I'
        || parsed.bytes.at(1) != 'I'
        || ReadU16(parsed.bytes, 2U) != 42U)
    {
        throw std::runtime_error("unexpected test TIFF header");
    }

    const std::uint32_t ifdOffset = ReadU32(parsed.bytes, 4U);
    const std::uint16_t entryCount =
        ReadU16(parsed.bytes, ifdOffset);
    for (std::uint16_t index{0}; index < entryCount; ++index)
    {
        const std::size_t offset =
            static_cast<std::size_t>(ifdOffset)
            + 2U
            + static_cast<std::size_t>(index) * 12U;
        TagEntry entry;
        entry.tag = ReadU16(parsed.bytes, offset);
        entry.type = ReadU16(parsed.bytes, offset + 2U);
        entry.count = ReadU32(parsed.bytes, offset + 4U);
        entry.valueoffset = ReadU32(parsed.bytes, offset + 8U);
        if (!parsed.entries.emplace(entry.tag, entry).second)
        {
            throw std::runtime_error(
                "duplicate test TIFF tag: "
                + std::to_string(entry.tag));
        }
    }

    const std::size_t nextIfdOffsetPosition =
        static_cast<std::size_t>(ifdOffset)
        + 2U
        + static_cast<std::size_t>(entryCount) * 12U;
    parsed.nextifdoffset =
        ReadU32(parsed.bytes, nextIfdOffsetPosition);
    return parsed;
}

const TagEntry& RequireTag(
    const ParsedTiff& parsed,
    const std::uint16_t tag)
{
    const auto found = parsed.entries.find(tag);
    if (found == parsed.entries.end())
    {
        throw std::runtime_error(
            "missing test TIFF tag: " + std::to_string(tag));
    }
    return found->second;
}

std::vector<std::uint8_t> ReadTagBytes(
    const ParsedTiff& parsed,
    const std::uint16_t tag)
{
    const TagEntry& entry = RequireTag(parsed, tag);
    const std::size_t byteCount =
        static_cast<std::size_t>(entry.count)
        * TiffTypeSize(entry.type);
    std::vector<std::uint8_t> values;
    values.reserve(byteCount);
    if (byteCount <= 4U)
    {
        for (std::size_t index{0}; index < byteCount; ++index)
        {
            values.push_back(
                static_cast<std::uint8_t>(
                    (entry.valueoffset >> (index * 8U)) & 0xffU));
        }
        return values;
    }

    const std::size_t valueOffset = entry.valueoffset;
    if (valueOffset + byteCount > parsed.bytes.size())
    {
        throw std::runtime_error(
            "test TIFF tag data outside file: "
            + std::to_string(tag));
    }
    values.insert(
        values.end(),
        parsed.bytes.begin()
            + static_cast<std::ptrdiff_t>(valueOffset),
        parsed.bytes.begin()
            + static_cast<std::ptrdiff_t>(valueOffset + byteCount));
    return values;
}

std::vector<std::uint16_t> ReadTagShorts(
    const ParsedTiff& parsed,
    const std::uint16_t tag)
{
    const TagEntry& entry = RequireTag(parsed, tag);
    if (entry.type != 3U)
    {
        throw std::runtime_error(
            "test TIFF tag is not SHORT: " + std::to_string(tag));
    }
    const std::vector<std::uint8_t> bytes =
        ReadTagBytes(parsed, tag);
    std::vector<std::uint16_t> values;
    values.reserve(entry.count);
    for (std::uint32_t index{0}; index < entry.count; ++index)
    {
        values.push_back(
            ReadU16(bytes, static_cast<std::size_t>(index) * 2U));
    }
    return values;
}

std::vector<std::uint32_t> ReadTagUnsignedIntegers(
    const ParsedTiff& parsed,
    const std::uint16_t tag)
{
    const TagEntry& entry = RequireTag(parsed, tag);
    if (entry.type != 3U && entry.type != 4U)
    {
        throw std::runtime_error(
            "test TIFF tag is not SHORT or LONG: "
            + std::to_string(tag));
    }
    const std::vector<std::uint8_t> bytes =
        ReadTagBytes(parsed, tag);
    std::vector<std::uint32_t> values;
    values.reserve(entry.count);
    for (std::uint32_t index{0}; index < entry.count; ++index)
    {
        const std::size_t valueOffset =
            static_cast<std::size_t>(index)
            * TiffTypeSize(entry.type);
        values.push_back(
            entry.type == 3U
                ? ReadU16(bytes, valueOffset)
                : ReadU32(bytes, valueOffset));
    }
    return values;
}

std::string ReadTagAscii(
    const ParsedTiff& parsed,
    const std::uint16_t tag)
{
    const TagEntry& entry = RequireTag(parsed, tag);
    if (entry.type != 2U)
    {
        throw std::runtime_error(
            "test TIFF tag is not ASCII: " + std::to_string(tag));
    }
    std::vector<std::uint8_t> bytes = ReadTagBytes(parsed, tag);
    if (bytes.empty() || bytes.back() != 0U)
    {
        throw std::runtime_error(
            "test TIFF ASCII tag lacks terminator: "
            + std::to_string(tag));
    }
    bytes.pop_back();
    return std::string(bytes.begin(), bytes.end());
}

std::vector<std::uint8_t> MakePixels(
    const std::uint32_t width,
    const std::uint32_t height)
{
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width)
            * height
            * slicer_core::rgbwsv_channel_count,
        255U);
    for (std::uint32_t y{0}; y < height; ++y)
    {
        for (std::uint32_t x{0}; x < width; ++x)
        {
            const std::size_t base =
                (static_cast<std::size_t>(y) * width + x)
                * slicer_core::rgbwsv_channel_count;
            pixels.at(base) =
                static_cast<std::uint8_t>((x * 17U + y * 3U) % 251U);
            pixels.at(base + 1U) =
                static_cast<std::uint8_t>((x * 5U + y * 29U) % 253U);
            pixels.at(base + 2U) =
                static_cast<std::uint8_t>((x * 11U + y * 7U) % 255U);
            pixels.at(base + 3U) =
                ((x + y) % 3U == 0U) ? 0U : 255U;
            pixels.at(base + 4U) =
                ((x + y) % 5U == 0U) ? 127U : 255U;
            pixels.at(base + 5U) =
                ((x + y) % 7U == 0U) ? 64U : 255U;
        }
    }
    return pixels;
}

bool ValidateCommonTags(
    const ParsedTiff& parsed,
    const std::uint32_t width,
    const std::uint32_t height)
{
    const std::vector<std::uint16_t> bits =
        ReadTagShorts(parsed, 258U);
    const std::vector<std::uint16_t> sampleFormats =
        ReadTagShorts(parsed, 339U);
    const std::vector<std::uint16_t> extraSamples =
        ReadTagShorts(parsed, 338U);
    return ExpectEqual(
               ReadTagUnsignedIntegers(parsed, 256U).at(0),
               width,
               "ImageWidth tag")
        && ExpectEqual(
            ReadTagUnsignedIntegers(parsed, 257U).at(0),
            height,
            "ImageLength tag")
        && ExpectTrue(
            bits == std::vector<std::uint16_t>(6U, 8U),
            "BitsPerSample is 8 x 6")
        && ExpectEqual(
            ReadTagShorts(parsed, 259U).at(0),
            static_cast<std::uint16_t>(1U),
            "Compression is none")
        && ExpectEqual(
            ReadTagShorts(parsed, 262U).at(0),
            static_cast<std::uint16_t>(2U),
            "Photometric is RGB")
        && ExpectEqual(
            ReadTagShorts(parsed, 277U).at(0),
            static_cast<std::uint16_t>(6U),
            "SamplesPerPixel is RGBWSV")
        && ExpectEqual(
            ReadTagShorts(parsed, 284U).at(0),
            static_cast<std::uint16_t>(1U),
            "PlanarConfig is contiguous")
        && ExpectTrue(
            extraSamples
                == std::vector<std::uint16_t>{0U, 0U, 0U},
            "ExtraSamples are three unspecified channels")
        && ExpectTrue(
            sampleFormats == std::vector<std::uint16_t>(6U, 1U),
            "SampleFormat is unsigned integer x 6")
        && ExpectEqual(
            ReadTagAscii(parsed, 270U),
            std::string{"RGBWSV"},
            "ImageDescription contract")
        && ExpectEqual(
            ReadTagAscii(parsed, 305U),
            std::string{"slice_soft_demo p0"},
            "Software contract")
        && ExpectEqual(
            parsed.nextifdoffset,
            static_cast<std::uint32_t>(0U),
            "single IFD contract");
}

bool ValidateTilePadding(
    const ParsedTiff& parsed,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint32_t tileWidth,
    const std::uint32_t tileHeight)
{
    const std::vector<std::uint32_t> tileOffsets =
        ReadTagUnsignedIntegers(parsed, 324U);
    const std::vector<std::uint32_t> tileByteCounts =
        ReadTagUnsignedIntegers(parsed, 325U);
    const std::uint32_t tilesX =
        (width + tileWidth - 1U) / tileWidth;
    const std::uint32_t tilesY =
        (height + tileHeight - 1U) / tileHeight;
    if (!ExpectEqual(
            tileOffsets.size(),
            static_cast<std::size_t>(tilesX * tilesY),
            "tile offset count")
        || !ExpectEqual(
            tileByteCounts.size(),
            tileOffsets.size(),
            "tile byte-count count"))
    {
        return false;
    }

    for (std::uint32_t tileY{0}; tileY < tilesY; ++tileY)
    {
        for (std::uint32_t tileX{0}; tileX < tilesX; ++tileX)
        {
            const std::size_t tileIndex =
                static_cast<std::size_t>(tileY) * tilesX + tileX;
            const std::size_t tileOffset = tileOffsets.at(tileIndex);
            const std::size_t tileBytes =
                tileByteCounts.at(tileIndex);
            if (!ExpectEqual(
                    tileBytes,
                    static_cast<std::size_t>(
                        tileWidth
                        * tileHeight
                        * slicer_core::rgbwsv_channel_count),
                    "full tile byte count"))
            {
                return false;
            }

            for (std::uint32_t localY{0};
                 localY < tileHeight;
                 ++localY)
            {
                for (std::uint32_t localX{0};
                     localX < tileWidth;
                     ++localX)
                {
                    const std::uint32_t imageX =
                        tileX * tileWidth + localX;
                    const std::uint32_t imageY =
                        tileY * tileHeight + localY;
                    if (imageX < width && imageY < height)
                    {
                        continue;
                    }
                    const std::size_t pixelOffset =
                        tileOffset
                        + (static_cast<std::size_t>(localY)
                               * tileWidth
                           + localX)
                            * slicer_core::rgbwsv_channel_count;
                    for (std::size_t channel{0};
                         channel
                         < slicer_core::rgbwsv_channel_count;
                         ++channel)
                    {
                        if (!ExpectEqual(
                                parsed.bytes.at(
                                    pixelOffset + channel),
                                static_cast<std::uint8_t>(255U),
                                "out-of-image tile padding"))
                        {
                            return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool StrippedContractAndPixelsAreFrozen(
    const std::filesystem::path& directory)
{
    constexpr std::uint32_t width{19U};
    constexpr std::uint32_t height{7U};
    const std::vector<std::uint8_t> pixels =
        MakePixels(width, height);
    slicer_core::TiffImageSpec spec;
    spec.width = width;
    spec.height = height;
    spec.rows_per_strip = 3U;
    spec.storage_mode = slicer_core::TiffStorageMode::Stripped;
    const std::filesystem::path path =
        directory / "contract_stripped.tiff";

    slicer_core::write_rgbwsv_tiff(path, spec, pixels);
    const slicer_core::TiffReadResult decoded =
        slicer_core::read_rgbwsv_tiff(path);
    const ParsedTiff parsed = ParseTiff(path);
    const std::vector<std::uint32_t> stripBytes =
        ReadTagUnsignedIntegers(parsed, 279U);
    return ExpectTrue(
               decoded.spec.storage_mode
                   == slicer_core::TiffStorageMode::Stripped,
               "decoded storage is stripped")
        && ExpectTrue(
            decoded.pixels == pixels,
            "stripped decoded pixels are exact")
        && ValidateCommonTags(parsed, width, height)
        && ExpectEqual(
            ReadTagUnsignedIntegers(parsed, 278U).at(0),
            static_cast<std::uint32_t>(3U),
            "RowsPerStrip contract")
        && ExpectTrue(
            stripBytes
                == std::vector<std::uint32_t>{
                    342U,
                    342U,
                    114U},
            "last stripped block is partial")
        && ExpectTrue(
            parsed.entries.contains(273U)
                && parsed.entries.contains(279U)
                && !parsed.entries.contains(324U)
                && !parsed.entries.contains(325U),
            "stripped storage tags are exclusive");
}

bool TiledContractAndPixelsAreFrozen(
    const std::filesystem::path& directory)
{
    constexpr std::uint32_t width{19U};
    constexpr std::uint32_t height{7U};
    const std::vector<std::uint8_t> pixels =
        MakePixels(width, height);
    slicer_core::TiffImageSpec spec;
    spec.width = width;
    spec.height = height;
    spec.tile_width = 8U;
    spec.tile_height = 4U;
    spec.storage_mode = slicer_core::TiffStorageMode::Tiled;
    const std::filesystem::path path =
        directory / "contract_tiled.tiff";

    slicer_core::write_rgbwsv_tiff(path, spec, pixels);
    const slicer_core::TiffReadResult decoded =
        slicer_core::read_rgbwsv_tiff(path);
    const ParsedTiff parsed = ParseTiff(path);
    const std::vector<std::uint32_t> tileBytes =
        ReadTagUnsignedIntegers(parsed, 325U);
    return ExpectTrue(
               decoded.spec.storage_mode
                   == slicer_core::TiffStorageMode::Tiled,
               "decoded storage is tiled")
        && ExpectTrue(
            decoded.pixels == pixels,
            "tiled decoded pixels and 255 padding are exact")
        && ValidateCommonTags(parsed, width, height)
        && ValidateTilePadding(
            parsed,
            width,
            height,
            spec.tile_width,
            spec.tile_height)
        && ExpectEqual(
            ReadTagUnsignedIntegers(parsed, 322U).at(0),
            static_cast<std::uint32_t>(8U),
            "TileWidth contract")
        && ExpectEqual(
            ReadTagUnsignedIntegers(parsed, 323U).at(0),
            static_cast<std::uint32_t>(4U),
            "TileLength contract")
        && ExpectTrue(
            tileBytes
                == std::vector<std::uint32_t>(6U, 192U),
            "six full-size tiles are stored")
        && ExpectTrue(
            parsed.entries.contains(324U)
                && parsed.entries.contains(325U)
                && !parsed.entries.contains(273U)
                && !parsed.entries.contains(279U),
            "tiled storage tags are exclusive");
}

bool ExpectThrowsExact(
    const std::function<void()>& action,
    const std::string& expected,
    const std::string& message)
{
    try
    {
        action();
    }
    catch (const std::exception& error)
    {
        return ExpectEqual(
            std::string{error.what()},
            expected,
            message);
    }
    return ExpectTrue(false, message + " did not throw");
}

bool CurrentErrorsAreFrozen(
    const std::filesystem::path& directory)
{
    const std::unique_ptr<slicer_core::ITiffWriter> writer =
        slicer_core::CreateTiffWriter(
            slicer_core::TiffWriterBackend::Handwritten);
    slicer_core::TiffImageSpec stripped;
    stripped.width = 2U;
    stripped.height = 2U;
    stripped.rows_per_strip = 1U;
    stripped.storage_mode =
        slicer_core::TiffStorageMode::Stripped;
    const std::vector<std::uint8_t> validPixels(24U, 255U);

    slicer_core::TiffImageSpec invalidDimensions = stripped;
    invalidDimensions.width = 0U;
    slicer_core::TiffImageSpec invalidChannels = stripped;
    invalidChannels.samples_per_pixel = 4U;
    const std::vector<std::uint8_t> shortPixels(23U, 255U);

    slicer_core::TiffImageSpec tiled = stripped;
    tiled.storage_mode = slicer_core::TiffStorageMode::Tiled;
    tiled.tile_width = 0U;
    tiled.tile_height = 4U;
    slicer_core::TiffImageSpec invalidTiledChannels = tiled;
    invalidTiledChannels.tile_width = 4U;
    invalidTiledChannels.samples_per_pixel = 4U;

    return ExpectThrowsExact(
               [&]
               {
                   writer->Write(
                       directory / "invalid_dimensions.tiff",
                       invalidDimensions,
                       validPixels);
               },
               "invalid TIFF dimensions",
               "invalid stripped dimensions error")
        && ExpectThrowsExact(
            [&]
            {
                writer->Write(
                    directory / "invalid_channels.tiff",
                    invalidChannels,
                    validPixels);
            },
            "P0 03B TIFF writer only supports RGBWSV uint8 contiguous pixels",
            "invalid stripped layout error")
        && ExpectThrowsExact(
            [&]
            {
                writer->Write(
                    directory / "invalid_buffer.tiff",
                    stripped,
                    shortPixels);
            },
            "pixel buffer size does not match TIFF dimensions",
            "invalid stripped buffer error")
        && ExpectThrowsExact(
            [&]
            {
                writer->Write(
                    directory / "invalid_tile.tiff",
                    tiled,
                    validPixels);
            },
            "invalid TIFF dimensions",
            "invalid tiled dimensions error")
        && ExpectThrowsExact(
            [&]
            {
                writer->Write(
                    directory / "invalid_tiled_channels.tiff",
                    invalidTiledChannels,
                    validPixels);
            },
            "P0 00B TIFF writer only supports RGBWSV uint8 contiguous pixels",
            "invalid tiled layout error");
}

std::filesystem::path MakeTemporaryDirectory()
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("slicesoft_03d_tiff_contract_"
           + std::to_string(suffix));
    std::filesystem::create_directories(directory);
    return directory;
}

}  // namespace

int main()
{
    const std::filesystem::path directory =
        MakeTemporaryDirectory();
    try
    {
        const bool passed =
            StrippedContractAndPixelsAreFrozen(directory)
            && TiledContractAndPixelsAreFrozen(directory)
            && CurrentErrorsAreFrozen(directory);
        std::filesystem::remove_all(directory);
        if (!passed)
        {
            return 1;
        }
        std::cout
            << "PASS tiff_writer_contract_unit_tests\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(directory);
        std::cerr
            << "tiff_writer_contract_unit_tests error: "
            << error.what() << '\n';
        return 1;
    }
}
