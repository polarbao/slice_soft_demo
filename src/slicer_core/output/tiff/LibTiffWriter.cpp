#include "slicer_core/output/tiff/TiffWriterImplementations.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#ifdef SLICER_CORE_HAS_LIBTIFF
#include <tiffio.h>
#endif

namespace slicer_core
{
namespace
{

#ifdef SLICER_CORE_HAS_LIBTIFF

struct TiffHandleDeleter
{
    void operator()(TIFF* handle) const noexcept
    {
        if (handle != nullptr)
        {
            TIFFClose(handle);
        }
    }
};

using TiffHandle = std::unique_ptr<TIFF, TiffHandleDeleter>;

void ValidateStrippedInput(
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels)
{
    if (spec.width == 0U
        || spec.height == 0U
        || spec.rows_per_strip == 0U)
    {
        throw std::runtime_error("invalid TIFF dimensions");
    }
    if (spec.samples_per_pixel != rgbwsv_channel_count
        || spec.bits_per_sample != 8U
        || spec.planar_config != PLANARCONFIG_CONTIG)
    {
        throw std::runtime_error(
            "P0 03B TIFF writer only supports RGBWSV uint8 contiguous pixels");
    }
    const std::size_t expectedPixels =
        static_cast<std::size_t>(spec.width)
        * spec.height
        * spec.samples_per_pixel;
    if (pixels.size() != expectedPixels)
    {
        throw std::runtime_error(
            "pixel buffer size does not match TIFF dimensions");
    }
}

TiffHandle OpenTiff(const std::filesystem::path& path)
{
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path());
    }

#ifdef _WIN32
    TiffHandle handle{TIFFOpenW(path.c_str(), "w")};
#else
    TiffHandle handle{TIFFOpen(path.string().c_str(), "w")};
#endif
    if (!handle)
    {
        throw std::runtime_error(
            "failed to open TIFF for writing: " + path.string());
    }
    return handle;
}

void SetFixedTags(TIFF* handle, const TiffImageSpec& spec)
{
    std::uint16_t extraSamples[3]{
        EXTRASAMPLE_UNSPECIFIED,
        EXTRASAMPLE_UNSPECIFIED,
        EXTRASAMPLE_UNSPECIFIED};
    const bool configured =
        TIFFSetField(handle, TIFFTAG_IMAGEWIDTH, spec.width) == 1
        && TIFFSetField(handle, TIFFTAG_IMAGELENGTH, spec.height) == 1
        && TIFFSetField(
               handle,
               TIFFTAG_SAMPLESPERPIXEL,
               spec.samples_per_pixel)
            == 1
        && TIFFSetField(
               handle,
               TIFFTAG_BITSPERSAMPLE,
               spec.bits_per_sample)
            == 1
        && TIFFSetField(handle, TIFFTAG_COMPRESSION, COMPRESSION_NONE) == 1
        && TIFFSetField(handle, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB) == 1
        && TIFFSetField(
               handle,
               TIFFTAG_PLANARCONFIG,
               PLANARCONFIG_CONTIG)
            == 1
        && TIFFSetField(
               handle,
               TIFFTAG_SAMPLEFORMAT,
               SAMPLEFORMAT_UINT)
            == 1
        && TIFFSetField(
               handle,
               TIFFTAG_EXTRASAMPLES,
               static_cast<std::uint16_t>(3U),
               extraSamples)
            == 1
        && TIFFSetField(
               handle,
               TIFFTAG_ROWSPERSTRIP,
               spec.rows_per_strip)
            == 1
        && TIFFSetField(handle, TIFFTAG_IMAGEDESCRIPTION, "RGBWSV") == 1
        && TIFFSetField(
               handle,
               TIFFTAG_SOFTWARE,
               "slice_soft_demo p0")
            == 1;
    if (!configured)
    {
        throw std::runtime_error("tiff_tag_setup_failed");
    }
}

void WriteStrips(
    TIFF* handle,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels)
{
    const std::uint32_t stripCount =
        (spec.height + spec.rows_per_strip - 1U)
        / spec.rows_per_strip;
    for (std::uint32_t stripIndex{0U};
         stripIndex < stripCount;
         ++stripIndex)
    {
        const std::uint32_t startRow =
            stripIndex * spec.rows_per_strip;
        const std::uint32_t rows = std::min(
            spec.rows_per_strip,
            spec.height - startRow);
        const std::size_t sourceOffset =
            static_cast<std::size_t>(startRow)
            * spec.width
            * spec.samples_per_pixel;
        const std::size_t byteCount =
            static_cast<std::size_t>(rows)
            * spec.width
            * spec.samples_per_pixel;
        if (byteCount
            > static_cast<std::size_t>(
                std::numeric_limits<tmsize_t>::max()))
        {
            throw std::runtime_error("tiff_strip_write_failed");
        }
        const tmsize_t requestedBytes =
            static_cast<tmsize_t>(byteCount);
        const tmsize_t writtenBytes = TIFFWriteEncodedStrip(
            handle,
            stripIndex,
            const_cast<std::uint8_t*>(pixels.data() + sourceOffset),
            requestedBytes);
        if (writtenBytes != requestedBytes)
        {
            throw std::runtime_error("tiff_strip_write_failed");
        }
    }
}

class LibTiffWriter final : public ITiffWriter
{
public:
    TiffWriterBackend Backend() const noexcept override
    {
        return TiffWriterBackend::LibTiff;
    }

    void Write(
        const std::filesystem::path& path,
        const TiffImageSpec& spec,
        const std::span<const std::uint8_t> pixels) const override
    {
        if (spec.storage_mode != TiffStorageMode::Stripped)
        {
            throw std::runtime_error(
                "LibTIFF tiled writer is not implemented in 03D-03");
        }

        ValidateStrippedInput(spec, pixels);
        TiffHandle handle = OpenTiff(path);
        SetFixedTags(handle.get(), spec);
        WriteStrips(handle.get(), spec, pixels);
        if (TIFFFlush(handle.get()) != 1)
        {
            throw std::runtime_error("tiff_close_failed");
        }
    }
};

#endif

}  // namespace

namespace detail
{

std::unique_ptr<ITiffWriter> CreateLibTiffWriter()
{
#ifdef SLICER_CORE_HAS_LIBTIFF
    return std::make_unique<LibTiffWriter>();
#else
    throw std::runtime_error(
        "LibTIFF writer is unavailable in this build");
#endif
}

}  // namespace detail
}  // namespace slicer_core
