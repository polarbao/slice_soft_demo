#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"

#include "slicer_core/materials/transfer/TransferChannelError.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifdef SLICER_CORE_HAS_LIBTIFF
#include <tiffio.h>
#endif

namespace slicer_core
{
namespace
{

#ifdef SLICER_CORE_HAS_LIBTIFF

struct TiffCloser
{
    void operator()(TIFF* handle) const noexcept
    {
        if (handle != nullptr)
        {
            TIFFClose(handle);
        }
    }
};

using TiffHandle = std::unique_ptr<TIFF, TiffCloser>;

[[noreturn]] void ThrowReadError(const std::string& detail)
{
    throw TransferChannelError(
        TransferChannelErrorCode::ProtocolInvalid,
        "RGBWSVT TIFF read failed: " + detail);
}

TiffHandle OpenTiff(const std::filesystem::path& path)
{
#ifdef _WIN32
    TiffHandle handle{TIFFOpenW(path.wstring().c_str(), "r")};
#else
    TiffHandle handle{TIFFOpen(path.string().c_str(), "r")};
#endif
    if (!handle)
    {
        ThrowReadError("cannot open " + path.string());
    }
    return handle;
}

void AccumulateStats(RgbwsvtTiffReadResult& result)
{
    for (std::size_t offset{0U}; offset < result.pixels.size(); offset += 7U)
    {
        for (std::size_t channel{0U}; channel < 7U; ++channel)
        {
            const std::uint8_t value = result.pixels[offset + channel];
            result.channelChecksums[channel] += value;
            TiffChannelStats& stats = result.channelStats[channel];
            stats.min_value = std::min(stats.min_value, static_cast<int>(value));
            stats.max_value = std::max(stats.max_value, static_cast<int>(value));
            stats.empty_pixels += value == 255U;
            stats.print_pixels += value != 255U;
            stats.full_print_pixels += value == 0U;
            stats.partial_print_pixels += value != 0U && value != 255U;
        }
    }
}

#endif

}  // namespace

RgbwsvtTiffReadResult ReadRgbwsvtTiff(const std::filesystem::path& path)
{
#ifndef SLICER_CORE_HAS_LIBTIFF
    (void)path;
    throw TransferChannelError(
        TransferChannelErrorCode::ProtocolInvalid,
        "RGBWSVT TIFF reader requires LibTIFF support");
#else
    TiffHandle handle = OpenTiff(path);
    RgbwsvtTiffReadResult result;
    std::uint16_t compression{0U};
    std::uint16_t sampleFormat{SAMPLEFORMAT_UINT};
    std::uint16_t extraSampleCount{0U};
    std::uint16_t* extraSamples{nullptr};
    char* description{nullptr};
    if (TIFFGetField(handle.get(), TIFFTAG_IMAGEWIDTH, &result.spec.width) != 1
        || TIFFGetField(handle.get(), TIFFTAG_IMAGELENGTH, &result.spec.height) != 1
        || TIFFGetField(handle.get(), TIFFTAG_SAMPLESPERPIXEL, &result.spec.samples_per_pixel) != 1
        || TIFFGetField(handle.get(), TIFFTAG_BITSPERSAMPLE, &result.spec.bits_per_sample) != 1
        || TIFFGetField(handle.get(), TIFFTAG_PLANARCONFIG, &result.spec.planar_config) != 1
        || TIFFGetField(handle.get(), TIFFTAG_COMPRESSION, &compression) != 1)
    {
        ThrowReadError("required image tags are missing");
    }
    (void)TIFFGetFieldDefaulted(handle.get(), TIFFTAG_SAMPLEFORMAT, &sampleFormat);
    if (result.spec.width == 0U || result.spec.height == 0U
        || result.spec.samples_per_pixel != 7U
        || result.spec.bits_per_sample != 8U
        || result.spec.planar_config != PLANARCONFIG_CONTIG
        || sampleFormat != SAMPLEFORMAT_UINT)
    {
        ThrowReadError("image is not seven-channel uint8 contiguous RGBWSVT");
    }
    if (TIFFGetField(handle.get(), TIFFTAG_EXTRASAMPLES, &extraSampleCount, &extraSamples) != 1
        || extraSampleCount != 4U)
    {
        ThrowReadError("ExtraSamples must declare four unspecified channels");
    }
    for (std::uint16_t index{0U}; index < extraSampleCount; ++index)
    {
        if (extraSamples[index] != EXTRASAMPLE_UNSPECIFIED)
        {
            ThrowReadError("ExtraSamples entries must be unspecified");
        }
    }
    if (TIFFGetField(handle.get(), TIFFTAG_IMAGEDESCRIPTION, &description) != 1
        || description == nullptr || std::string{description} != "RGBWSVT")
    {
        ThrowReadError("ImageDescription must be RGBWSVT");
    }
    if (compression == COMPRESSION_NONE)
    {
        result.spec.compression_mode = TiffCompressionMode::None;
    }
    else if (compression == COMPRESSION_PACKBITS)
    {
        result.spec.compression_mode = TiffCompressionMode::PackBits;
    }
    else
    {
        ThrowReadError("compression is not none or PackBits");
    }

    const std::size_t rowBytes = static_cast<std::size_t>(result.spec.width) * 7U;
    result.pixels.assign(rowBytes * result.spec.height, 255U);
    if (TIFFIsTiled(handle.get()) != 0)
    {
        result.spec.storage_mode = TiffStorageMode::Tiled;
        if (TIFFGetField(handle.get(), TIFFTAG_TILEWIDTH, &result.spec.tile_width) != 1
            || TIFFGetField(handle.get(), TIFFTAG_TILELENGTH, &result.spec.tile_height) != 1)
        {
            ThrowReadError("tile dimensions are missing");
        }
        const tmsize_t tileSize = TIFFTileSize(handle.get());
        if (tileSize <= 0)
        {
            ThrowReadError("tile size is invalid");
        }
        std::vector<std::uint8_t> tile(static_cast<std::size_t>(tileSize));
        for (std::uint32_t y{0U}; y < result.spec.height; y += result.spec.tile_height)
        {
            for (std::uint32_t x{0U}; x < result.spec.width; x += result.spec.tile_width)
            {
                if (TIFFReadTile(handle.get(), tile.data(), x, y, 0U, 0U) < 0)
                {
                    ThrowReadError("tile payload could not be decoded");
                }
                const std::uint32_t copyWidth =
                    std::min(result.spec.tile_width, result.spec.width - x);
                const std::uint32_t copyHeight =
                    std::min(result.spec.tile_height, result.spec.height - y);
                for (std::uint32_t row{0U}; row < copyHeight; ++row)
                {
                    const std::size_t source =
                        static_cast<std::size_t>(row) * result.spec.tile_width * 7U;
                    const std::size_t target =
                        (static_cast<std::size_t>(y + row) * result.spec.width + x) * 7U;
                    std::copy_n(
                        tile.data() + source,
                        static_cast<std::size_t>(copyWidth) * 7U,
                        result.pixels.data() + target);
                }
            }
        }
    }
    else
    {
        result.spec.storage_mode = TiffStorageMode::Stripped;
        (void)TIFFGetFieldDefaulted(handle.get(), TIFFTAG_ROWSPERSTRIP, &result.spec.rows_per_strip);
        std::vector<std::uint8_t> scanline(
            static_cast<std::size_t>(TIFFScanlineSize(handle.get())));
        if (scanline.size() < rowBytes)
        {
            ThrowReadError("scanline size is smaller than the RGBWSVT row");
        }
        for (std::uint32_t row{0U}; row < result.spec.height; ++row)
        {
            if (TIFFReadScanline(handle.get(), scanline.data(), row, 0U) < 0)
            {
                ThrowReadError("strip scanline could not be decoded");
            }
            std::copy_n(
                scanline.data(), rowBytes,
                result.pixels.data() + static_cast<std::size_t>(row) * rowBytes);
        }
    }
    AccumulateStats(result);
    return result;
#endif
}

}  // namespace slicer_core
