#include "slicer_core/output/tiff/TiffWriterImplementations.h"

#include "slicer_core/output/tiff/HandwrittenTiffStructureInternal.h"
#include "slicer_core/output/tiff/TiffPackBitsWriteInternal.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace slicer_core
{
namespace
{

using handwritten_tiff_internal::Ascii;
using handwritten_tiff_internal::BuildIfd;
using handwritten_tiff_internal::CompressionTagValue;
using handwritten_tiff_internal::EncodePackBitsBlock;
using handwritten_tiff_internal::IfdEntry;
using handwritten_tiff_internal::Longs;
using handwritten_tiff_internal::Shorts;
using handwritten_tiff_internal::TiffFieldType;
using handwritten_tiff_internal::WriteFile;

constexpr std::uint32_t kTiffHeaderSize{8U};

void ValidateImage(
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels,
    const bool tiled)
{
    const bool storageDimensionsValid = tiled
        ? spec.tile_width != 0U && spec.tile_height != 0U
        : spec.rows_per_strip != 0U;
    if (spec.width == 0U || spec.height == 0U || !storageDimensionsValid)
    {
        throw std::runtime_error("invalid TIFF dimensions");
    }
    if (spec.samples_per_pixel != rgbwsv_channel_count
        || spec.bits_per_sample != 8U || spec.planar_config != 1U)
    {
        throw std::runtime_error(
            tiled
                ? "P0 00B TIFF writer only supports RGBWSV uint8 contiguous pixels"
                : "P0 03B TIFF writer only supports RGBWSV uint8 contiguous pixels");
    }
    const std::size_t expected = static_cast<std::size_t>(spec.width)
        * spec.height * spec.samples_per_pixel;
    if (pixels.size() != expected)
    {
        throw std::runtime_error(
            "pixel buffer size does not match TIFF dimensions");
    }
}

std::vector<IfdEntry> CommonEntries(const TiffImageSpec& spec)
{
    return {
        {256U, TiffFieldType::Long, 1U, Longs({spec.width})},
        {257U, TiffFieldType::Long, 1U, Longs({spec.height})},
        {258U, TiffFieldType::Short, spec.samples_per_pixel,
         Shorts({8U, 8U, 8U, 8U, 8U, 8U})},
        {259U, TiffFieldType::Short, 1U,
         Shorts({CompressionTagValue(spec.compression_mode)})},
        {262U, TiffFieldType::Short, 1U, Shorts({2U})},
        {270U, TiffFieldType::Ascii, 7U, Ascii("RGBWSV")},
        {277U, TiffFieldType::Short, 1U,
         Shorts({spec.samples_per_pixel})},
        {284U, TiffFieldType::Short, 1U, Shorts({spec.planar_config})},
        {305U, TiffFieldType::Ascii, 19U, Ascii("slice_soft_demo p0")},
        {338U, TiffFieldType::Short, 3U, Shorts({0U, 0U, 0U})},
        {339U, TiffFieldType::Short, spec.samples_per_pixel,
         Shorts({1U, 1U, 1U, 1U, 1U, 1U})},
    };
}

std::vector<std::uint8_t> EncodeStorageBlock(
    const std::span<const std::uint8_t> bytes,
    const std::size_t rowByteCount,
    const std::uint32_t rowCount,
    const TiffCompressionMode compression)
{
    if (compression == TiffCompressionMode::PackBits)
    {
        return EncodePackBitsBlock(bytes, rowByteCount, rowCount);
    }
    return {bytes.begin(), bytes.end()};
}

class HandwrittenTiffWriter final : public ITiffWriter
{
public:
    TiffWriterBackend Backend() const noexcept override
    {
        return TiffWriterBackend::Handwritten;
    }

    void Write(
        const std::filesystem::path& path,
        const TiffImageSpec& spec,
        const std::span<const std::uint8_t> pixels) const override
    {
        if (spec.storage_mode == TiffStorageMode::Tiled)
        {
            write_rgbwsv_tiled_tiff(path, spec, pixels);
            return;
        }
        write_rgbwsv_stripped_tiff(path, spec, pixels);
    }
};

}  // namespace

void write_rgbwsv_tiled_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels)
{
    ValidateImage(spec, pixels, true);
    const std::uint32_t tilesX =
        (spec.width + spec.tile_width - 1U) / spec.tile_width;
    const std::uint32_t tilesY =
        (spec.height + spec.tile_height - 1U) / spec.tile_height;
    const std::uint32_t tileCount{tilesX * tilesY};
    const std::uint32_t tileBytes =
        spec.tile_width * spec.tile_height * spec.samples_per_pixel;

    std::vector<std::uint8_t> payload;
    std::vector<std::uint8_t> tile(tileBytes, 255U);
    std::vector<std::uint32_t> offsets;
    std::vector<std::uint32_t> byteCounts;
    offsets.reserve(tileCount);
    byteCounts.reserve(tileCount);
    for (std::uint32_t tileY{0U}; tileY < tilesY; ++tileY)
    {
        for (std::uint32_t tileX{0U}; tileX < tilesX; ++tileX)
        {
            std::fill(tile.begin(), tile.end(), 255U);
            for (std::uint32_t y{0U}; y < spec.tile_height; ++y)
            {
                const std::uint32_t imageY{tileY * spec.tile_height + y};
                if (imageY >= spec.height)
                {
                    continue;
                }
                for (std::uint32_t x{0U}; x < spec.tile_width; ++x)
                {
                    const std::uint32_t imageX{tileX * spec.tile_width + x};
                    if (imageX >= spec.width)
                    {
                        continue;
                    }
                    const std::size_t source =
                        (static_cast<std::size_t>(imageY) * spec.width + imageX)
                        * spec.samples_per_pixel;
                    const std::size_t target =
                        (static_cast<std::size_t>(y) * spec.tile_width + x)
                        * spec.samples_per_pixel;
                    std::copy_n(
                        pixels.begin() + static_cast<std::ptrdiff_t>(source),
                        spec.samples_per_pixel,
                        tile.begin() + static_cast<std::ptrdiff_t>(target));
                }
            }
            offsets.push_back(
                kTiffHeaderSize + static_cast<std::uint32_t>(payload.size()));
            const std::vector<std::uint8_t> encoded = EncodeStorageBlock(
                tile,
                static_cast<std::size_t>(spec.tile_width)
                    * spec.samples_per_pixel,
                spec.tile_height,
                spec.compression_mode);
            byteCounts.push_back(static_cast<std::uint32_t>(encoded.size()));
            payload.insert(payload.end(), encoded.begin(), encoded.end());
        }
    }

    std::vector<IfdEntry> entries = CommonEntries(spec);
    entries.push_back(
        {322U, TiffFieldType::Long, 1U, Longs({spec.tile_width})});
    entries.push_back(
        {323U, TiffFieldType::Long, 1U, Longs({spec.tile_height})});
    entries.push_back(
        {324U, TiffFieldType::Long, tileCount, Longs(offsets)});
    entries.push_back(
        {325U, TiffFieldType::Long, tileCount, Longs(byteCounts)});
    const std::uint32_t ifdOffset =
        kTiffHeaderSize + static_cast<std::uint32_t>(payload.size());
    WriteFile(path, payload, BuildIfd(std::move(entries), ifdOffset), ifdOffset);
}

void write_rgbwsv_stripped_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels)
{
    ValidateImage(spec, pixels, false);
    const std::uint32_t stripCount =
        (spec.height + spec.rows_per_strip - 1U) / spec.rows_per_strip;
    std::vector<std::uint8_t> payload;
    std::vector<std::uint32_t> offsets;
    std::vector<std::uint32_t> byteCounts;
    offsets.reserve(stripCount);
    byteCounts.reserve(stripCount);
    for (std::uint32_t index{0U}; index < stripCount; ++index)
    {
        const std::uint32_t startRow{index * spec.rows_per_strip};
        const std::uint32_t rows = std::min(
            spec.rows_per_strip,
            static_cast<std::uint32_t>(spec.height - startRow));
        const std::size_t byteCount = static_cast<std::size_t>(rows)
            * spec.width * spec.samples_per_pixel;
        const std::size_t sourceOffset = static_cast<std::size_t>(startRow)
            * spec.width * spec.samples_per_pixel;
        offsets.push_back(
            kTiffHeaderSize + static_cast<std::uint32_t>(payload.size()));
        const std::vector<std::uint8_t> encoded = EncodeStorageBlock(
            pixels.subspan(sourceOffset, byteCount),
            static_cast<std::size_t>(spec.width) * spec.samples_per_pixel,
            rows,
            spec.compression_mode);
        byteCounts.push_back(static_cast<std::uint32_t>(encoded.size()));
        payload.insert(payload.end(), encoded.begin(), encoded.end());
    }

    std::vector<IfdEntry> entries = CommonEntries(spec);
    entries.push_back(
        {273U, TiffFieldType::Long, stripCount, Longs(offsets)});
    entries.push_back(
        {278U, TiffFieldType::Long, 1U, Longs({spec.rows_per_strip})});
    entries.push_back(
        {279U, TiffFieldType::Long, stripCount, Longs(byteCounts)});
    const std::uint32_t ifdOffset =
        kTiffHeaderSize + static_cast<std::uint32_t>(payload.size());
    WriteFile(path, payload, BuildIfd(std::move(entries), ifdOffset), ifdOffset);
}

namespace detail
{

std::unique_ptr<ITiffWriter> CreateHandwrittenTiffWriter()
{
    return std::make_unique<HandwrittenTiffWriter>();
}

}  // namespace detail
}  // namespace slicer_core
