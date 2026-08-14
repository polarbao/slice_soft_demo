#include "slicer_core/output/tiff/TiffWriterImplementations.h"

#include "slicer_core/output/tiff/TiffWriterError.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

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

struct TiffOpenOptionsDeleter
{
    void operator()(TIFFOpenOptions* options) const noexcept
    {
        if (options != nullptr)
        {
            TIFFOpenOptionsFree(options);
        }
    }
};

class TemporaryFileGuard
{
public:
    explicit TemporaryFileGuard(std::filesystem::path path)
        : m_path(std::move(path))
    {
    }

    ~TemporaryFileGuard()
    {
        if (!m_path.empty())
        {
            std::error_code error;
            std::filesystem::remove(m_path, error);
        }
    }

    TemporaryFileGuard(const TemporaryFileGuard&) = delete;
    TemporaryFileGuard& operator=(const TemporaryFileGuard&) = delete;

    void Release() noexcept
    {
        m_path.clear();
    }

private:
    std::filesystem::path m_path;
};

struct ErrorContext
{
    std::string message;
};

#ifdef _WIN32
std::wstring ExtendedLengthPath(const std::filesystem::path& path)
{
    const std::wstring absolutePath =
        std::filesystem::absolute(path).lexically_normal().native();
    if (absolutePath.starts_with(L"\\\\?\\"))
    {
        return absolutePath;
    }
    if (absolutePath.starts_with(L"\\\\"))
    {
        return L"\\\\?\\UNC\\" + absolutePath.substr(2U);
    }
    return L"\\\\?\\" + absolutePath;
}
#endif

using TiffHandle = std::unique_ptr<TIFF, TiffHandleDeleter>;
using TiffOpenOptionsHandle =
    std::unique_ptr<TIFFOpenOptions, TiffOpenOptionsDeleter>;

int CaptureTiffMessage(
    TIFF*,
    void* userData,
    const char* module,
    const char* format,
    va_list arguments)
{
    auto* context = static_cast<ErrorContext*>(userData);
    if (context == nullptr)
    {
        return 1;
    }

    char buffer[1024]{};
    std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    context->message.clear();
    if (module != nullptr && module[0] != '\0')
    {
        context->message.append(module).append(": ");
    }
    context->message.append(buffer);
    return 1;
}

[[noreturn]] void ThrowWriterError(
    const TiffWriterErrorCode code,
    const std::string& detail)
{
    throw TiffWriterException(code, detail);
}

std::size_t CheckedPixelByteCount(const TiffImageSpec& spec)
{
    const std::size_t width = spec.width;
    const std::size_t height = spec.height;
    const std::size_t channels = spec.samples_per_pixel;
    const std::size_t maximum =
        std::numeric_limits<std::size_t>::max();
    if (height != 0U && width > maximum / height)
    {
        ThrowWriterError(
            TiffWriterErrorCode::InvalidInput,
            "image dimensions overflow the address space");
    }
    const std::size_t pixelCount = width * height;
    if (channels != 0U && pixelCount > maximum / channels)
    {
        ThrowWriterError(
            TiffWriterErrorCode::InvalidInput,
            "image byte count overflows the address space");
    }
    return pixelCount * channels;
}

void ValidateInput(
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels)
{
    if (spec.width == 0U || spec.height == 0U)
    {
        ThrowWriterError(
            TiffWriterErrorCode::InvalidInput,
            "image width and height must be positive");
    }
    if (spec.samples_per_pixel != rgbwsv_channel_count
        || spec.bits_per_sample != 8U
        || spec.planar_config != PLANARCONFIG_CONTIG)
    {
        ThrowWriterError(
            TiffWriterErrorCode::InvalidInput,
            "only RGBWSV uint8 contiguous pixels are supported");
    }
    if (spec.storage_mode == TiffStorageMode::Stripped
        && spec.rows_per_strip == 0U)
    {
        ThrowWriterError(
            TiffWriterErrorCode::InvalidInput,
            "rows per strip must be positive");
    }
    if (spec.storage_mode == TiffStorageMode::Tiled
        && (spec.tile_width < 16U
            || spec.tile_height < 16U
            || spec.tile_width % 16U != 0U
            || spec.tile_height % 16U != 0U))
    {
        ThrowWriterError(
            TiffWriterErrorCode::InvalidInput,
            "LibTIFF tile width and height must be positive multiples of 16");
    }
    if (pixels.size() != CheckedPixelByteCount(spec))
    {
        ThrowWriterError(
            TiffWriterErrorCode::InvalidInput,
            "pixel buffer size does not match TIFF dimensions");
    }
}

std::filesystem::path MakeTemporaryPath(
    const std::filesystem::path& path)
{
    static std::atomic_uint64_t sequence{0U};
    static const std::uint32_t processToken = []()
    {
#ifdef _WIN32
        return static_cast<std::uint32_t>(GetCurrentProcessId());
#else
        const std::uint64_t ticks = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count());
        return static_cast<std::uint32_t>(ticks ^ (ticks >> 32U));
#endif
    }();
    const std::uint64_t ticket = ++sequence;
    // 继续采用同目录原子发布，但将临时文件基本名限制为 27 个字符，
    // 避免其中再次包含完整的最终层文件名。
    static_assert(sizeof("~t00000000.0000000000000000") - 1U == 27U);
    char temporaryName[32]{};
    (void)std::snprintf(
        temporaryName,
        sizeof(temporaryName),
        "~t%08x.%016llx",
        static_cast<unsigned int>(processToken),
        static_cast<unsigned long long>(ticket));
    return path.parent_path() / temporaryName;
}

TiffHandle OpenTiff(
    const std::filesystem::path& path,
    ErrorContext& errorContext)
{
    try
    {
        if (!path.parent_path().empty())
        {
            std::filesystem::create_directories(path.parent_path());
        }
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        ThrowWriterError(TiffWriterErrorCode::OpenFailed, error.what());
    }

    TiffOpenOptionsHandle options{TIFFOpenOptionsAlloc()};
    if (!options)
    {
        ThrowWriterError(
            TiffWriterErrorCode::OpenFailed,
            "failed to allocate LibTIFF open options");
    }
    TIFFOpenOptionsSetErrorHandlerExtR(
        options.get(),
        CaptureTiffMessage,
        &errorContext);
    TIFFOpenOptionsSetWarningHandlerExtR(
        options.get(),
        CaptureTiffMessage,
        &errorContext);

#ifdef _WIN32
    const std::wstring nativePath = ExtendedLengthPath(path);
    TiffHandle handle{TIFFOpenWExt(
        nativePath.c_str(),
        "w",
        options.get())};
#else
    TiffHandle handle{
        TIFFOpenExt(path.string().c_str(), "w", options.get())};
#endif
    if (!handle)
    {
        ThrowWriterError(
            TiffWriterErrorCode::OpenFailed,
            errorContext.message.empty()
                ? "LibTIFF could not open the temporary output"
                : errorContext.message);
    }
    return handle;
}

std::uint16_t LibTiffCompressionTag(
    const TiffCompressionMode mode)
{
    switch (mode)
    {
        case TiffCompressionMode::None:
            return COMPRESSION_NONE;
        case TiffCompressionMode::PackBits:
            return COMPRESSION_PACKBITS;
    }
    ThrowWriterError(
        TiffWriterErrorCode::TagSetupFailed,
        "unsupported TIFF compression mode");
}

void SetFixedTags(
    TIFF* handle,
    const TiffImageSpec& spec,
    const ErrorContext& errorContext)
{
    std::uint16_t extraSamples[3]{
        EXTRASAMPLE_UNSPECIFIED,
        EXTRASAMPLE_UNSPECIFIED,
        EXTRASAMPLE_UNSPECIFIED};
    bool configured =
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
        && TIFFSetField(
               handle,
               TIFFTAG_COMPRESSION,
               LibTiffCompressionTag(spec.compression_mode))
            == 1
        && TIFFSetField(handle, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB) == 1
        && TIFFSetField(handle, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG) == 1
        && TIFFSetField(handle, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT) == 1
        && TIFFSetField(
               handle,
               TIFFTAG_EXTRASAMPLES,
               static_cast<std::uint16_t>(3U),
               extraSamples)
            == 1
        && TIFFSetField(handle, TIFFTAG_IMAGEDESCRIPTION, "RGBWSV") == 1
        && TIFFSetField(handle, TIFFTAG_SOFTWARE, "slice_soft_demo p0") == 1;

    if (spec.storage_mode == TiffStorageMode::Tiled)
    {
        configured = configured
            && TIFFSetField(handle, TIFFTAG_TILEWIDTH, spec.tile_width) == 1
            && TIFFSetField(handle, TIFFTAG_TILELENGTH, spec.tile_height) == 1;
    }
    else
    {
        configured = configured
            && TIFFSetField(
                   handle,
                   TIFFTAG_ROWSPERSTRIP,
                   spec.rows_per_strip)
                == 1;
    }

    if (!configured)
    {
        ThrowWriterError(
            TiffWriterErrorCode::TagSetupFailed,
            errorContext.message);
    }
}

void WriteStrips(
    TIFF* handle,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels,
    const ErrorContext& errorContext)
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
            ThrowWriterError(
                TiffWriterErrorCode::StripWriteFailed,
                "strip byte count exceeds LibTIFF limits");
        }
        const tmsize_t requestedBytes =
            static_cast<tmsize_t>(byteCount);
        const tmsize_t writtenBytes = TIFFWriteEncodedStrip(
            handle,
            stripIndex,
            const_cast<std::uint8_t*>(pixels.data() + sourceOffset),
            requestedBytes);
        if (writtenBytes < 0)
        {
            ThrowWriterError(
                TiffWriterErrorCode::StripWriteFailed,
                errorContext.message);
        }
    }
}

void WriteTiles(
    TIFF* handle,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels,
    const ErrorContext& errorContext)
{
    const std::size_t tileByteCount =
        static_cast<std::size_t>(spec.tile_width)
        * spec.tile_height
        * spec.samples_per_pixel;
    if (tileByteCount
        > static_cast<std::size_t>(
            std::numeric_limits<tmsize_t>::max()))
    {
        ThrowWriterError(
            TiffWriterErrorCode::TileWriteFailed,
            "tile byte count exceeds LibTIFF limits");
    }

    std::vector<std::uint8_t> tile(tileByteCount, 255U);
    const tmsize_t requestedBytes =
        static_cast<tmsize_t>(tileByteCount);
    for (std::uint32_t tileY{0U};
         tileY < spec.height;
         tileY += spec.tile_height)
    {
        for (std::uint32_t tileX{0U};
             tileX < spec.width;
             tileX += spec.tile_width)
        {
            std::fill(tile.begin(), tile.end(), 255U);
            const std::uint32_t copyWidth =
                std::min(spec.tile_width, spec.width - tileX);
            const std::uint32_t copyHeight =
                std::min(spec.tile_height, spec.height - tileY);
            const std::size_t copyBytes =
                static_cast<std::size_t>(copyWidth)
                * spec.samples_per_pixel;
            for (std::uint32_t row{0U}; row < copyHeight; ++row)
            {
                const std::size_t sourceOffset =
                    (static_cast<std::size_t>(tileY + row) * spec.width
                     + tileX)
                    * spec.samples_per_pixel;
                const std::size_t destinationOffset =
                    static_cast<std::size_t>(row)
                    * spec.tile_width
                    * spec.samples_per_pixel;
                std::copy_n(
                    pixels.data() + sourceOffset,
                    copyBytes,
                    tile.data() + destinationOffset);
            }

            const ttile_t tileIndex =
                TIFFComputeTile(handle, tileX, tileY, 0U, 0U);
            const tmsize_t writtenBytes = TIFFWriteEncodedTile(
                handle,
                tileIndex,
                tile.data(),
                requestedBytes);
            if (writtenBytes < 0)
            {
                ThrowWriterError(
                    TiffWriterErrorCode::TileWriteFailed,
                    errorContext.message);
            }
        }
    }
}

void PublishTemporaryFile(
    const std::filesystem::path& temporaryPath,
    const std::filesystem::path& destinationPath)
{
#ifdef _WIN32
    const std::wstring nativeTemporaryPath =
        ExtendedLengthPath(temporaryPath);
    const std::wstring nativeDestinationPath =
        ExtendedLengthPath(destinationPath);
    if (MoveFileExW(
            nativeTemporaryPath.c_str(),
            nativeDestinationPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
        == 0)
    {
        ThrowWriterError(
            TiffWriterErrorCode::PublishFailed,
            "MoveFileExW failed with error "
                + std::to_string(GetLastError()));
    }
#else
    std::error_code error;
    std::filesystem::rename(temporaryPath, destinationPath, error);
    if (error)
    {
        ThrowWriterError(TiffWriterErrorCode::PublishFailed, error.message());
    }
#endif
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
        ValidateInput(spec, pixels);

        const std::filesystem::path temporaryPath =
            MakeTemporaryPath(path);
        TemporaryFileGuard temporaryGuard(temporaryPath);
        ErrorContext errorContext;
        TiffHandle handle = OpenTiff(temporaryPath, errorContext);
        SetFixedTags(handle.get(), spec, errorContext);
        if (spec.storage_mode == TiffStorageMode::Tiled)
        {
            WriteTiles(handle.get(), spec, pixels, errorContext);
        }
        else
        {
            WriteStrips(handle.get(), spec, pixels, errorContext);
        }
        if (TIFFFlush(handle.get()) != 1)
        {
            ThrowWriterError(
                TiffWriterErrorCode::CloseFailed,
                errorContext.message);
        }
        handle.reset();

        PublishTemporaryFile(temporaryPath, path);
        temporaryGuard.Release();
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
