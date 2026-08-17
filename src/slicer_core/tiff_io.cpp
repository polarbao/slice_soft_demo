#include "slicer_core/TiffReadApi.h"

#include "slicer_core/TiffPackBitsReadInternal.h"
#include "slicer_core/TiffReadStructureInternal.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace slicer_core
{
namespace
{

using tiff_read_internal::DecodePackBitsBlock;
using tiff_read_internal::FindOptionalEntry;
using tiff_read_internal::FindRequiredEntry;
using tiff_read_internal::ParseIfdEntries;
using tiff_read_internal::ParsedTiffEntries;
using tiff_read_internal::ReadU16Array;
using tiff_read_internal::ReadU32Array;

void ValidateCommonSpec(
    TiffReadResult& result,
    const std::vector<std::uint16_t>& bitsPerSample,
    const std::vector<std::uint16_t>& sampleFormats,
    const std::filesystem::path& path)
{
    if (result.spec.samples_per_pixel != rgbwsv_channel_count
        || bitsPerSample.size() != rgbwsv_channel_count
        || sampleFormats.size() != rgbwsv_channel_count)
    {
        throw std::runtime_error(
            "TIFF is not a six-channel RGBWSV image: " + path.string());
    }
    for (std::size_t index{0U}; index < bitsPerSample.size(); ++index)
    {
        if (bitsPerSample.at(index) != 8U || sampleFormats.at(index) != 1U)
        {
            throw std::runtime_error(
                "TIFF channel is not uint8: " + path.string());
        }
    }
    if (result.spec.planar_config != 1U)
    {
        throw std::runtime_error(
            "TIFF planar config is not contiguous: " + path.string());
    }
}

void UpdateChannelStats(
    TiffReadResult& result,
    const std::uint16_t channel,
    const std::uint8_t value)
{
    result.channel_checksums.at(channel) += value;
    TiffChannelStats& stats = result.channel_stats.at(channel);
    stats.min_value = std::min(stats.min_value, static_cast<int>(value));
    stats.max_value = std::max(stats.max_value, static_cast<int>(value));
    if (value == 255U)
    {
        ++stats.empty_pixels;
        return;
    }
    ++stats.print_pixels;
    if (value == 0U)
    {
        ++stats.full_print_pixels;
    }
    else
    {
        ++stats.partial_print_pixels;
    }
}

void AccumulateContiguousChannelStats(
    TiffReadResult& result,
    const std::span<const std::uint8_t> pixels)
{
    if ((pixels.size() % rgbwsv_channel_count) != 0U)
    {
        throw std::runtime_error(
            "TIFF contiguous RGBWSV payload is not pixel aligned");
    }
    for (std::size_t offset{0U};
         offset < pixels.size();
         offset += rgbwsv_channel_count)
    {
        for (std::size_t channel{0U};
             channel < rgbwsv_channel_count;
             ++channel)
        {
            const std::uint8_t value = pixels[offset + channel];
            result.channel_checksums[channel] += value;
            TiffChannelStats& stats = result.channel_stats[channel];
            stats.min_value = std::min(
                stats.min_value,
                static_cast<int>(value));
            stats.max_value = std::max(
                stats.max_value,
                static_cast<int>(value));
            stats.empty_pixels += value == 255U;
            stats.print_pixels += value != 255U;
            stats.full_print_pixels += value == 0U;
            stats.partial_print_pixels += value != 0U && value != 255U;
        }
    }
}

void InitializeReadPixels(TiffReadResult& result)
{
    const std::size_t byteCount = static_cast<std::size_t>(result.spec.width)
        * result.spec.height * result.spec.samples_per_pixel;
    result.pixels.assign(byteCount, 255U);
}

TiffCompressionMode ReadTiffCompressionMode(
    const std::vector<std::uint8_t>& data,
    const ParsedTiffEntries& entries,
    const std::filesystem::path& path)
{
    const std::uint16_t value =
        ReadU16Array(data, FindRequiredEntry(entries, 259U), 259U).at(0U);
    if (value == 1U)
    {
        return TiffCompressionMode::None;
    }
    if (value == 32773U)
    {
        return TiffCompressionMode::PackBits;
    }
    throw std::runtime_error(
        "unsupported TIFF compression: " + std::to_string(value)
        + ": " + path.string());
}

std::vector<std::uint8_t> ReadFile(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input)
    {
        throw std::runtime_error(
            "failed to open TIFF for reading: " + path.string());
    }
    const std::streampos end = input.tellg();
    if (end < 0)
    {
        throw std::runtime_error(
            "failed to determine TIFF byte count: " + path.string());
    }
    const auto byteCount = static_cast<std::uintmax_t>(end);
    if (byteCount
        > static_cast<std::uintmax_t>(
            std::numeric_limits<std::streamsize>::max()))
    {
        throw std::runtime_error(
            "TIFF is too large for one buffered read: " + path.string());
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(byteCount));
    input.seekg(0, std::ios::beg);
    if (!data.empty()
        && !input.read(
            reinterpret_cast<char*>(data.data()),
            static_cast<std::streamsize>(data.size())))
    {
        throw std::runtime_error(
            "failed to read complete TIFF payload: " + path.string());
    }
    return data;
}

void DecodeTile(
    TiffReadResult& result,
    const std::span<const std::uint8_t> tile,
    const std::uint32_t tileX,
    const std::uint32_t tileY,
    const std::filesystem::path& path)
{
    for (std::uint32_t y{0U}; y < result.spec.tile_height; ++y)
    {
        const std::uint32_t imageY{tileY * result.spec.tile_height + y};
        for (std::uint32_t x{0U}; x < result.spec.tile_width; ++x)
        {
            const std::uint32_t imageX{tileX * result.spec.tile_width + x};
            for (std::uint16_t channel{0U};
                 channel < result.spec.samples_per_pixel;
                 ++channel)
            {
                const std::size_t sourceIndex =
                    (static_cast<std::size_t>(y) * result.spec.tile_width + x)
                        * result.spec.samples_per_pixel
                    + channel;
                const std::uint8_t value = tile[sourceIndex];
                if (imageY >= result.spec.height || imageX >= result.spec.width)
                {
                    if (value != 255U)
                    {
                        throw std::runtime_error(
                            "TIFF tile padding is not 255: " + path.string());
                    }
                    continue;
                }
                const std::size_t targetIndex =
                    (static_cast<std::size_t>(imageY) * result.spec.width
                     + imageX)
                        * result.spec.samples_per_pixel
                    + channel;
                result.pixels.at(targetIndex) = value;
                UpdateChannelStats(result, channel, value);
            }
        }
    }
}

void DecodeStrip(
    TiffReadResult& result,
    const std::span<const std::uint8_t> strip,
    const std::uint32_t startRow,
    const std::uint32_t rows)
{
    const std::size_t expectedBytes = static_cast<std::size_t>(rows)
        * result.spec.width * result.spec.samples_per_pixel;
    const std::size_t targetOffset = static_cast<std::size_t>(startRow)
        * result.spec.width * result.spec.samples_per_pixel;
    if (strip.size() != expectedBytes
        || targetOffset + expectedBytes > result.pixels.size())
    {
        throw std::runtime_error(
            "TIFF strip payload does not match the decoded image range");
    }
    std::copy(
        strip.begin(),
        strip.end(),
        result.pixels.begin() + static_cast<std::ptrdiff_t>(targetOffset));
    AccumulateContiguousChannelStats(result, strip);
}

}  // namespace

std::string tiff_storage_mode_string(const TiffStorageMode mode)
{
    switch (mode)
    {
        case TiffStorageMode::Stripped:
            return "stripped";
        case TiffStorageMode::Tiled:
            return "tiled";
    }
    return "unknown";
}

std::string TiffCompressionModeString(const TiffCompressionMode mode)
{
    switch (mode)
    {
        case TiffCompressionMode::None:
            return "none";
        case TiffCompressionMode::PackBits:
            return "packbits";
    }
    return "unknown";
}

TiffCompressionMode ParseTiffCompressionMode(const std::string_view name)
{
    if (name == "none")
    {
        return TiffCompressionMode::None;
    }
    if (name == "packbits")
    {
        return TiffCompressionMode::PackBits;
    }
    throw std::invalid_argument(
        "TIFF compression must be none or packbits: " + std::string{name});
}

TiffReadResult read_rgbwsv_tiled_tiff(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> data = ReadFile(path);
    const ParsedTiffEntries entries = ParseIfdEntries(data, path);
    TiffReadResult result;
    result.spec.storage_mode = TiffStorageMode::Tiled;
    result.spec.width =
        ReadU32Array(data, FindRequiredEntry(entries, 256U), 256U).at(0U);
    result.spec.height =
        ReadU32Array(data, FindRequiredEntry(entries, 257U), 257U).at(0U);
    result.spec.compression_mode = ReadTiffCompressionMode(data, entries, path);
    const auto bitsPerSample =
        ReadU16Array(data, FindRequiredEntry(entries, 258U), 258U);
    result.spec.samples_per_pixel =
        ReadU16Array(data, FindRequiredEntry(entries, 277U), 277U).at(0U);
    result.spec.planar_config =
        ReadU16Array(data, FindRequiredEntry(entries, 284U), 284U).at(0U);
    result.spec.tile_width =
        ReadU32Array(data, FindRequiredEntry(entries, 322U), 322U).at(0U);
    result.spec.tile_height =
        ReadU32Array(data, FindRequiredEntry(entries, 323U), 323U).at(0U);
    const auto sampleFormats =
        ReadU16Array(data, FindRequiredEntry(entries, 339U), 339U);
    ValidateCommonSpec(result, bitsPerSample, sampleFormats, path);
    InitializeReadPixels(result);

    const auto tileOffsets =
        ReadU32Array(data, FindRequiredEntry(entries, 324U), 324U);
    const auto tileByteCounts =
        ReadU32Array(data, FindRequiredEntry(entries, 325U), 325U);
    if (tileOffsets.size() != tileByteCounts.size())
    {
        throw std::runtime_error(
            "TIFF tile offset/count mismatch: " + path.string());
    }
    const std::uint32_t tilesX =
        (result.spec.width + result.spec.tile_width - 1U)
        / result.spec.tile_width;
    const std::uint32_t tilesY =
        (result.spec.height + result.spec.tile_height - 1U)
        / result.spec.tile_height;
    if (tileOffsets.size() != static_cast<std::size_t>(tilesX * tilesY))
    {
        throw std::runtime_error(
            "TIFF tile count does not match dimensions: " + path.string());
    }
    const std::size_t expectedBytes =
        static_cast<std::size_t>(result.spec.tile_width)
        * result.spec.tile_height * result.spec.samples_per_pixel;
    for (std::uint32_t tileY{0U}; tileY < tilesY; ++tileY)
    {
        for (std::uint32_t tileX{0U}; tileX < tilesX; ++tileX)
        {
            const std::uint32_t index{tileY * tilesX + tileX};
            const std::size_t offset{tileOffsets.at(index)};
            const std::size_t byteCount{tileByteCounts.at(index)};
            if (offset + byteCount > data.size())
            {
                throw std::runtime_error(
                    "TIFF tile data outside file: " + path.string());
            }
            if (result.spec.compression_mode == TiffCompressionMode::None
                && byteCount != expectedBytes)
            {
                throw std::runtime_error(
                    "TIFF tile byte count does not match tile dimensions: "
                    + path.string());
            }
            std::span<const std::uint8_t> payload{data.data() + offset, byteCount};
            std::vector<std::uint8_t> decoded;
            if (result.spec.compression_mode == TiffCompressionMode::PackBits)
            {
                decoded = DecodePackBitsBlock(payload, expectedBytes, path);
                payload = decoded;
            }
            DecodeTile(result, payload, tileX, tileY, path);
        }
    }
    return result;
}

TiffReadResult read_rgbwsv_stripped_tiff(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> data = ReadFile(path);
    const ParsedTiffEntries entries = ParseIfdEntries(data, path);
    TiffReadResult result;
    result.spec.storage_mode = TiffStorageMode::Stripped;
    result.spec.width =
        ReadU32Array(data, FindRequiredEntry(entries, 256U), 256U).at(0U);
    result.spec.height =
        ReadU32Array(data, FindRequiredEntry(entries, 257U), 257U).at(0U);
    result.spec.compression_mode = ReadTiffCompressionMode(data, entries, path);
    const auto bitsPerSample =
        ReadU16Array(data, FindRequiredEntry(entries, 258U), 258U);
    result.spec.samples_per_pixel =
        ReadU16Array(data, FindRequiredEntry(entries, 277U), 277U).at(0U);
    result.spec.rows_per_strip =
        ReadU32Array(data, FindRequiredEntry(entries, 278U), 278U).at(0U);
    result.spec.planar_config =
        ReadU16Array(data, FindRequiredEntry(entries, 284U), 284U).at(0U);
    const auto sampleFormats =
        ReadU16Array(data, FindRequiredEntry(entries, 339U), 339U);
    ValidateCommonSpec(result, bitsPerSample, sampleFormats, path);
    InitializeReadPixels(result);
    if (result.spec.rows_per_strip == 0U)
    {
        throw std::runtime_error(
            "TIFF rowsPerStrip is invalid: " + path.string());
    }

    const auto stripOffsets =
        ReadU32Array(data, FindRequiredEntry(entries, 273U), 273U);
    const auto stripByteCounts =
        ReadU32Array(data, FindRequiredEntry(entries, 279U), 279U);
    if (stripOffsets.size() != stripByteCounts.size())
    {
        throw std::runtime_error(
            "TIFF strip offset/count mismatch: " + path.string());
    }
    const std::uint32_t stripCount =
        (result.spec.height + result.spec.rows_per_strip - 1U)
        / result.spec.rows_per_strip;
    if (stripOffsets.size() != static_cast<std::size_t>(stripCount))
    {
        throw std::runtime_error(
            "TIFF strip count does not match dimensions: " + path.string());
    }
    for (std::uint32_t index{0U}; index < stripCount; ++index)
    {
        const std::uint32_t startRow{index * result.spec.rows_per_strip};
        const std::uint32_t rows = std::min(
            result.spec.rows_per_strip,
            static_cast<std::uint32_t>(result.spec.height - startRow));
        const std::size_t expectedBytes = static_cast<std::size_t>(rows)
            * result.spec.width * result.spec.samples_per_pixel;
        const std::size_t offset{stripOffsets.at(index)};
        const std::size_t byteCount{stripByteCounts.at(index)};
        if (offset + byteCount > data.size())
        {
            throw std::runtime_error(
                "TIFF strip data outside file: " + path.string());
        }
        if (result.spec.compression_mode == TiffCompressionMode::None
            && byteCount != expectedBytes)
        {
            throw std::runtime_error(
                "TIFF strip byte count does not match dimensions: "
                + path.string());
        }
        std::span<const std::uint8_t> payload{data.data() + offset, byteCount};
        std::vector<std::uint8_t> decoded;
        if (result.spec.compression_mode == TiffCompressionMode::PackBits)
        {
            decoded = DecodePackBitsBlock(payload, expectedBytes, path);
            payload = decoded;
        }
        DecodeStrip(result, payload, startRow, rows);
    }
    return result;
}

TiffReadResult read_rgbwsv_tiff(const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> data = ReadFile(path);
    const ParsedTiffEntries entries = ParseIfdEntries(data, path);
    const bool hasTiles = FindOptionalEntry(entries, 324U).has_value()
        || FindOptionalEntry(entries, 325U).has_value();
    const bool hasStrips = FindOptionalEntry(entries, 273U).has_value()
        || FindOptionalEntry(entries, 279U).has_value();
    if (hasTiles && !hasStrips)
    {
        return read_rgbwsv_tiled_tiff(path);
    }
    if (hasStrips && !hasTiles)
    {
        return read_rgbwsv_stripped_tiff(path);
    }
    throw std::runtime_error(
        "TIFF storage structure is ambiguous or missing: " + path.string());
}

}  // namespace slicer_core
