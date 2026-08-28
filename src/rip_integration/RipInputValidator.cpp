#include "rip_integration/RipInputValidator.h"

#include <tiffio.h>

#include <memory>
#include <system_error>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace slicesoft::rip
{
namespace
{
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

bool IsCancelled(const RipInputValidationRequest& request) noexcept
{
    if (!request.is_cancelled)
    {
        return false;
    }
    try
    {
        return request.is_cancelled();
    }
    catch (...)
    {
        return true;
    }
}

bool IsReparsePoint(const std::filesystem::path& path)
{
    std::error_code error;
    if (std::filesystem::is_symlink(
            std::filesystem::symlink_status(path, error)))
    {
        return true;
    }
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
#else
    return false;
#endif
}

bool ComponentEqual(
    const std::filesystem::path& left,
    const std::filesystem::path& right)
{
#ifdef _WIN32
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
#else
    return left == right;
#endif
}

bool IsContained(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate,
    const bool allowRoot = false)
{
    std::error_code rootError;
    std::error_code candidateError;
    const auto normalizedRoot = std::filesystem::weakly_canonical(
        root, rootError).lexically_normal();
    const auto normalizedCandidate = std::filesystem::weakly_canonical(
        candidate, candidateError).lexically_normal();
    if (rootError || candidateError || !root.is_absolute()
        || !candidate.is_absolute())
    {
        return false;
    }
    auto rootPart = normalizedRoot.begin();
    auto candidatePart = normalizedCandidate.begin();
    for (; rootPart != normalizedRoot.end(); ++rootPart, ++candidatePart)
    {
        if (candidatePart == normalizedCandidate.end()
            || !ComponentEqual(*rootPart, *candidatePart))
        {
            return false;
        }
    }
    return allowRoot || candidatePart != normalizedCandidate.end();
}

TiffHandle OpenTiff(const std::filesystem::path& path)
{
#ifdef _WIN32
    return TiffHandle{TIFFOpenW(path.c_str(), "r")};
#else
    return TiffHandle{TIFFOpen(path.c_str(), "r")};
#endif
}

}

RipStatus ValidateRipInput(const RipInputValidationRequest& request)
{
    if (!request.package_directory.is_absolute()
        || !request.input_directory.is_absolute()
        || request.layer_paths.empty()
        || request.layer_paths.size() > 1000000U
        || request.expected_width_px == 0U
        || request.expected_height_px == 0U
        || !IsContained(
            request.package_directory, request.input_directory))
    {
        return RipStatus::Failure(
            "RIP_INPUT_REQUEST_INVALID",
            "RIP input validation requires an absolute contained layer set");
    }
    std::error_code error;
    if (!std::filesystem::is_directory(request.input_directory, error)
        || error || IsReparsePoint(request.input_directory))
    {
        return RipStatus::Failure(
            "RIP_INPUT_DIRECTORY_INVALID",
            "the RIP input directory must be a real Package directory");
    }
    for (const auto& path : request.layer_paths)
    {
        if (IsCancelled(request))
        {
            return RipStatus::Failure(
                "RIP_VALIDATION_CANCELLED",
                "RIP input validation was cancelled");
        }
        if (!path.is_absolute()
            || !IsContained(request.input_directory, path)
            || !std::filesystem::is_regular_file(path, error)
            || error || IsReparsePoint(path))
        {
            return RipStatus::Failure(
                "RIP_INPUT_LAYER_PATH_INVALID",
                "a RIP input layer is missing, linked, or outside layers");
        }
        TiffHandle handle = OpenTiff(path);
        if (!handle)
        {
            return RipStatus::Failure(
                "RIP_INPUT_TIFF_OPEN_FAILED",
                "a RIP input layer cannot be opened");
        }
        std::uint32_t width{0U};
        std::uint32_t height{0U};
        std::uint16_t bitsPerSample{0U};
        std::uint16_t sampleFormat{SAMPLEFORMAT_UINT};
        std::uint16_t samplesPerPixel{0U};
        std::uint16_t planarConfig{0U};
        std::uint32_t rowsPerStrip{0U};
        if (TIFFGetField(handle.get(), TIFFTAG_IMAGEWIDTH, &width) != 1
            || TIFFGetField(handle.get(), TIFFTAG_IMAGELENGTH, &height) != 1
            || TIFFGetFieldDefaulted(
                handle.get(), TIFFTAG_BITSPERSAMPLE, &bitsPerSample) != 1
            || TIFFGetFieldDefaulted(
                handle.get(), TIFFTAG_SAMPLEFORMAT, &sampleFormat) != 1
            || TIFFGetFieldDefaulted(
                handle.get(), TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel) != 1
            || TIFFGetFieldDefaulted(
                handle.get(), TIFFTAG_PLANARCONFIG, &planarConfig) != 1
            || TIFFGetFieldDefaulted(
                handle.get(), TIFFTAG_ROWSPERSTRIP, &rowsPerStrip) != 1)
        {
            return RipStatus::Failure(
                "RIP_INPUT_TIFF_TAG_MISSING",
                "a RIP input layer is missing required TIFF tags");
        }
        if (width != request.expected_width_px
            || height != request.expected_height_px)
        {
            return RipStatus::Failure(
                "RIP_INPUT_DIMENSION_MISMATCH",
                "a RIP input layer does not match the Package grid");
        }
        if (bitsPerSample != 8U || sampleFormat != SAMPLEFORMAT_UINT
            || samplesPerPixel != 6U
            || planarConfig != PLANARCONFIG_CONTIG)
        {
            return RipStatus::Failure(
                "RIP_INPUT_SAMPLE_LAYOUT_INVALID",
                "RIP input must be unsigned 8-bit contiguous RGBWSV");
        }
        if (TIFFIsTiled(handle.get()) != 0 || rowsPerStrip == 0U)
        {
            return RipStatus::Failure(
                "RIP_INPUT_STORAGE_UNSUPPORTED",
                "the current external RIP only accepts stripped input");
        }
        const tmsize_t scanlineSize = TIFFScanlineSize(handle.get());
        const std::uint64_t requiredBytes =
            static_cast<std::uint64_t>(width) * samplesPerPixel;
        if (scanlineSize <= 0
            || static_cast<std::uint64_t>(scanlineSize) < requiredBytes)
        {
            return RipStatus::Failure(
                "RIP_INPUT_SCANLINE_SIZE_INVALID",
                "a RIP input scanline is smaller than declared");
        }
        std::vector<std::uint8_t> row(
            static_cast<std::size_t>(scanlineSize));
        for (std::uint32_t y{0U}; y < height; ++y)
        {
            if (IsCancelled(request))
            {
                return RipStatus::Failure(
                    "RIP_VALIDATION_CANCELLED",
                    "RIP input validation was cancelled");
            }
            if (TIFFReadScanline(handle.get(), row.data(), y, 0U) < 0)
            {
                return RipStatus::Failure(
                    "RIP_INPUT_SCANLINE_READ_FAILED",
                    "a RIP input scanline cannot be decoded");
            }
        }
        if (TIFFReadDirectory(handle.get()) == 1)
        {
            return RipStatus::Failure(
                "RIP_INPUT_MULTIPAGE_UNSUPPORTED",
                "each RIP input layer must contain one TIFF directory");
        }
    }
    return RipStatus::Success();
}

RipStatus ProbeRipInputGeometry(
    const std::filesystem::path& layer_path,
    RipInputGeometry* geometry)
{
    if (geometry == nullptr)
    {
        return RipStatus::Failure(
            "RIP_INPUT_GEOMETRY_OUTPUT_MISSING",
            "a geometry output object is required");
    }
    *geometry = {};
    std::error_code error;
    if (!layer_path.is_absolute()
        || !std::filesystem::is_regular_file(layer_path, error)
        || error
        || IsReparsePoint(layer_path))
    {
        return RipStatus::Failure(
            "RIP_INPUT_LAYER_PATH_INVALID",
            "a RIP input layer is missing, linked, or not a regular file");
    }
    TiffHandle handle = OpenTiff(layer_path);
    if (!handle)
    {
        return RipStatus::Failure(
            "RIP_INPUT_TIFF_OPEN_FAILED",
            "a RIP input layer cannot be opened");
    }
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::uint16_t bitsPerSample{0U};
    std::uint16_t sampleFormat{SAMPLEFORMAT_UINT};
    std::uint16_t samplesPerPixel{0U};
    std::uint16_t planarConfig{0U};
    std::uint32_t rowsPerStrip{0U};
    if (TIFFGetField(handle.get(), TIFFTAG_IMAGEWIDTH, &width) != 1
        || TIFFGetField(handle.get(), TIFFTAG_IMAGELENGTH, &height) != 1
        || TIFFGetFieldDefaulted(
            handle.get(), TIFFTAG_BITSPERSAMPLE, &bitsPerSample) != 1
        || TIFFGetFieldDefaulted(
            handle.get(), TIFFTAG_SAMPLEFORMAT, &sampleFormat) != 1
        || TIFFGetFieldDefaulted(
            handle.get(), TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel) != 1
        || TIFFGetFieldDefaulted(
            handle.get(), TIFFTAG_PLANARCONFIG, &planarConfig) != 1
        || TIFFGetFieldDefaulted(
            handle.get(), TIFFTAG_ROWSPERSTRIP, &rowsPerStrip) != 1)
    {
        return RipStatus::Failure(
            "RIP_INPUT_TIFF_TAG_MISSING",
            "a RIP input layer is missing required TIFF tags");
    }
    if (width == 0U || height == 0U)
    {
        return RipStatus::Failure(
            "RIP_INPUT_DIMENSION_MISMATCH",
            "a RIP input layer declares an empty pixel grid");
    }
    if (bitsPerSample != 8U || sampleFormat != SAMPLEFORMAT_UINT
        || samplesPerPixel != 6U
        || planarConfig != PLANARCONFIG_CONTIG)
    {
        return RipStatus::Failure(
            "RIP_INPUT_SAMPLE_LAYOUT_INVALID",
            "RIP input must be unsigned 8-bit contiguous RGBWSV");
    }
    if (TIFFIsTiled(handle.get()) != 0 || rowsPerStrip == 0U)
    {
        return RipStatus::Failure(
            "RIP_INPUT_STORAGE_UNSUPPORTED",
            "the current external RIP only accepts stripped input");
    }
    geometry->width_px = width;
    geometry->height_px = height;
    return RipStatus::Success();
}

}  // namespace slicesoft::rip
