#include "rip_integration/RipOutputValidator.h"

#include <tiffio.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

bool IsCancelled(const RipOutputValidationRequest& request) noexcept
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

struct StagedLayer
{
    std::size_t index{0U};
    std::filesystem::path source;
};

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

std::filesystem::path CanonicalAbsolute(
    const std::filesystem::path& path,
    std::error_code& error)
{
    error.clear();
    if (!path.is_absolute())
    {
        error = std::make_error_code(std::errc::invalid_argument);
        return {};
    }
    return std::filesystem::weakly_canonical(path, error).lexically_normal();
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
    const std::filesystem::path& candidate)
{
    std::error_code rootError;
    std::error_code candidateError;
    const std::filesystem::path normalizedRoot =
        CanonicalAbsolute(root, rootError);
    const std::filesystem::path normalizedCandidate =
        CanonicalAbsolute(candidate, candidateError);
    if (rootError || candidateError)
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
    return candidatePart != normalizedCandidate.end();
}

std::string LowerAscii(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

std::optional<std::size_t> ParseSliceIndex(
    const std::filesystem::path& path)
{
    const std::string name = LowerAscii(path.filename().string());
    constexpr std::string_view prefix{"slice."};
    constexpr std::string_view tiffSuffix{".tiff"};
    constexpr std::string_view tifSuffix{".tif"};
    if (!name.starts_with(prefix))
    {
        return std::nullopt;
    }
    const std::size_t suffixSize = name.ends_with(tiffSuffix)
        ? tiffSuffix.size()
        : name.ends_with(tifSuffix) ? tifSuffix.size() : 0U;
    if (suffixSize == 0U
        || name.size() <= prefix.size() + suffixSize)
    {
        return std::nullopt;
    }
    const std::string digits = name.substr(
        prefix.size(), name.size() - prefix.size() - suffixSize);
    if (!std::all_of(
            digits.begin(), digits.end(),
            [](const unsigned char value)
            {
                return std::isdigit(value) != 0;
            }))
    {
        return std::nullopt;
    }
    try
    {
        const unsigned long long value = std::stoull(digits);
        if (value > std::numeric_limits<std::size_t>::max())
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(value);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

bool IsTiffExtension(const std::filesystem::path& path)
{
    const std::string extension = LowerAscii(path.extension().string());
    return extension == ".tif" || extension == ".tiff";
}

std::filesystem::path NormalizedLayerPath(
    const std::filesystem::path& directory,
    const std::size_t layerIndex)
{
    std::array<char, 32> name{};
    const int count = std::snprintf(
        name.data(), name.size(), "rip_%06zu.tif", layerIndex);
    if (count <= 0 || static_cast<std::size_t>(count) >= name.size())
    {
        return {};
    }
    return directory / name.data();
}

TiffHandle OpenTiff(const std::filesystem::path& path)
{
#ifdef _WIN32
    return TiffHandle{TIFFOpenW(path.c_str(), "r")};
#else
    return TiffHandle{TIFFOpen(path.c_str(), "r")};
#endif
}

TiffHandle OpenTiffForWrite(const std::filesystem::path& path)
{
#ifdef _WIN32
    return TiffHandle{TIFFOpenW(path.c_str(), "w")};
#else
    return TiffHandle{TIFFOpen(path.c_str(), "w")};
#endif
}

bool IsVendorAlignedWidth(
    const std::uint32_t actualWidth,
    const std::uint32_t expectedWidth)
{
    const std::uint64_t alignedWidth =
        (static_cast<std::uint64_t>(expectedWidth) + 3U) & ~3ULL;
    return actualWidth > expectedWidth
        && static_cast<std::uint64_t>(actualWidth) == alignedWidth;
}

RipStatus RewriteWithExpectedWidth(
    const StagedLayer& staged,
    const RipOutputValidationRequest& request,
    const std::filesystem::path& destination)
{
    TiffHandle input = OpenTiff(staged.source);
    if (!input)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_WIDTH_NORMALIZE_OPEN_FAILED",
            "the padded RIP TIFF cannot be reopened for width normalization");
    }

    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::uint16_t bitsPerSample{0U};
    std::uint16_t sampleFormat{SAMPLEFORMAT_UINT};
    std::uint16_t samplesPerPixel{0U};
    std::uint16_t planarConfig{0U};
    std::uint16_t compression{COMPRESSION_NONE};
    std::uint16_t photometric{0U};
    std::uint32_t rowsPerStrip{0U};
    if (TIFFGetField(input.get(), TIFFTAG_IMAGEWIDTH, &width) != 1
        || TIFFGetField(input.get(), TIFFTAG_IMAGELENGTH, &height) != 1
        || TIFFGetFieldDefaulted(
            input.get(), TIFFTAG_BITSPERSAMPLE, &bitsPerSample) != 1
        || TIFFGetFieldDefaulted(
            input.get(), TIFFTAG_SAMPLEFORMAT, &sampleFormat) != 1
        || TIFFGetFieldDefaulted(
            input.get(), TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel) != 1
        || TIFFGetFieldDefaulted(
            input.get(), TIFFTAG_PLANARCONFIG, &planarConfig) != 1
        || TIFFGetFieldDefaulted(
            input.get(), TIFFTAG_COMPRESSION, &compression) != 1
        || TIFFGetFieldDefaulted(
            input.get(), TIFFTAG_PHOTOMETRIC, &photometric) != 1
        || TIFFGetFieldDefaulted(
            input.get(), TIFFTAG_ROWSPERSTRIP, &rowsPerStrip) != 1
        || height != request.expected_height_px
        || !IsVendorAlignedWidth(width, request.expected_width_px)
        || bitsPerSample != 8U
        || sampleFormat != SAMPLEFORMAT_UINT
        || samplesPerPixel < 7U
        || planarConfig != PLANARCONFIG_CONTIG
        || rowsPerStrip == 0U
        || TIFFIsTiled(input.get()) != 0)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_WIDTH_NORMALIZE_SOURCE_CHANGED",
            "the padded RIP TIFF changed after validation");
    }

    TiffHandle output = OpenTiffForWrite(destination);
    if (!output)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_WIDTH_NORMALIZE_CREATE_FAILED",
            "the normalized RIP TIFF cannot be created in staging");
    }

    bool tagsWritten =
        TIFFSetField(
            output.get(), TIFFTAG_IMAGEWIDTH,
            request.expected_width_px) == 1
        && TIFFSetField(output.get(), TIFFTAG_IMAGELENGTH, height) == 1
        && TIFFSetField(
            output.get(), TIFFTAG_BITSPERSAMPLE, bitsPerSample) == 1
        && TIFFSetField(
            output.get(), TIFFTAG_SAMPLEFORMAT, sampleFormat) == 1
        && TIFFSetField(
            output.get(), TIFFTAG_SAMPLESPERPIXEL, samplesPerPixel) == 1
        && TIFFSetField(
            output.get(), TIFFTAG_PLANARCONFIG, planarConfig) == 1
        && TIFFSetField(
            output.get(), TIFFTAG_PHOTOMETRIC, photometric) == 1
        && TIFFSetField(
            output.get(), TIFFTAG_COMPRESSION, compression) == 1
        && TIFFSetField(
            output.get(), TIFFTAG_ROWSPERSTRIP, rowsPerStrip) == 1;

    std::uint16_t extraSampleCount{0U};
    std::uint16_t* extraSampleTypes{nullptr};
    std::vector<std::uint16_t> defaultExtraSamples;
    if (TIFFGetField(
            input.get(), TIFFTAG_EXTRASAMPLES,
            &extraSampleCount, &extraSampleTypes) == 1
        && extraSampleCount > 0U)
    {
        tagsWritten = tagsWritten
            && TIFFSetField(
                output.get(), TIFFTAG_EXTRASAMPLES,
                extraSampleCount, extraSampleTypes) == 1;
    }
    else if (samplesPerPixel > 4U)
    {
        defaultExtraSamples.assign(
            samplesPerPixel - 4U, EXTRASAMPLE_UNSPECIFIED);
        tagsWritten = tagsWritten
            && TIFFSetField(
                output.get(), TIFFTAG_EXTRASAMPLES,
                static_cast<std::uint16_t>(defaultExtraSamples.size()),
                defaultExtraSamples.data()) == 1;
    }

    float xResolution{0.0F};
    float yResolution{0.0F};
    std::uint16_t resolutionUnit{RESUNIT_INCH};
    if (TIFFGetField(input.get(), TIFFTAG_XRESOLUTION, &xResolution) == 1
        && TIFFGetField(input.get(), TIFFTAG_YRESOLUTION, &yResolution) == 1
        && TIFFGetFieldDefaulted(
            input.get(), TIFFTAG_RESOLUTIONUNIT, &resolutionUnit) == 1)
    {
        tagsWritten = tagsWritten
            && TIFFSetField(
                output.get(), TIFFTAG_XRESOLUTION, xResolution) == 1
            && TIFFSetField(
                output.get(), TIFFTAG_YRESOLUTION, yResolution) == 1
            && TIFFSetField(
                output.get(), TIFFTAG_RESOLUTIONUNIT, resolutionUnit) == 1;
    }

    std::uint16_t orientation{ORIENTATION_TOPLEFT};
    std::uint16_t fillOrder{FILLORDER_MSB2LSB};
    if (TIFFGetFieldDefaulted(
            input.get(), TIFFTAG_ORIENTATION, &orientation) == 1)
    {
        tagsWritten = tagsWritten
            && TIFFSetField(
                output.get(), TIFFTAG_ORIENTATION, orientation) == 1;
    }
    if (TIFFGetFieldDefaulted(
            input.get(), TIFFTAG_FILLORDER, &fillOrder) == 1)
    {
        tagsWritten = tagsWritten
            && TIFFSetField(
                output.get(), TIFFTAG_FILLORDER, fillOrder) == 1;
    }

    std::uint16_t predictor{PREDICTOR_NONE};
    if (TIFFGetField(input.get(), TIFFTAG_PREDICTOR, &predictor) == 1)
    {
        tagsWritten = tagsWritten
            && TIFFSetField(
                output.get(), TIFFTAG_PREDICTOR, predictor) == 1;
    }

    std::uint32_t profileSize{0U};
    void* profileData{nullptr};
    if (TIFFGetField(
            input.get(), TIFFTAG_ICCPROFILE,
            &profileSize, &profileData) == 1
        && profileSize > 0U && profileData != nullptr)
    {
        tagsWritten = tagsWritten
            && TIFFSetField(
                output.get(), TIFFTAG_ICCPROFILE,
                profileSize, profileData) == 1;
    }
    if (!tagsWritten)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_WIDTH_NORMALIZE_TAG_FAILED",
            "required TIFF tags cannot be written during width normalization");
    }

    const tmsize_t inputScanlineSize = TIFFScanlineSize(input.get());
    const tmsize_t outputScanlineSize = TIFFScanlineSize(output.get());
    const std::uint64_t logicalBytes =
        static_cast<std::uint64_t>(request.expected_width_px)
        * samplesPerPixel;
    if (inputScanlineSize <= 0 || outputScanlineSize <= 0
        || static_cast<std::uint64_t>(inputScanlineSize) < logicalBytes
        || static_cast<std::uint64_t>(outputScanlineSize) != logicalBytes)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_WIDTH_NORMALIZE_SCANLINE_INVALID",
            "TIFF scanline storage is invalid during width normalization");
    }

    std::vector<std::uint8_t> row(
        static_cast<std::size_t>(inputScanlineSize));
    for (std::uint32_t y{0U}; y < height; ++y)
    {
        if (IsCancelled(request))
        {
            return RipStatus::Failure(
                "RIP_VALIDATION_CANCELLED",
                "RIP output width normalization was cancelled");
        }
        if (TIFFReadScanline(input.get(), row.data(), y, 0U) < 0
            || TIFFWriteScanline(output.get(), row.data(), y, 0U) < 0)
        {
            return RipStatus::Failure(
                "RIP_OUTPUT_WIDTH_NORMALIZE_SCANLINE_FAILED",
                "a RIP TIFF scanline cannot be rewritten at the Package width");
        }
    }
    if (TIFFWriteDirectory(output.get()) != 1)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_WIDTH_NORMALIZE_FINALIZE_FAILED",
            "the normalized RIP TIFF directory cannot be finalized");
    }
    return RipStatus::Success();
}

RipStatus ValidateLayer(
    const StagedLayer& staged,
    const RipOutputValidationRequest& request,
    RipOutputLayer* output,
    bool* requiresWidthNormalization,
    RipOutputDropViolation* firstViolation)
{
    TiffHandle handle = OpenTiff(staged.source);
    if (!handle)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_TIFF_OPEN_FAILED",
            "a staged RIP TIFF cannot be opened");
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
            "RIP_OUTPUT_TIFF_TAG_MISSING",
            "a staged RIP TIFF is missing required image tags");
    }
    const bool vendorAlignedWidth =
        IsVendorAlignedWidth(width, request.expected_width_px);
    if ((width != request.expected_width_px && !vendorAlignedWidth)
        || height != request.expected_height_px)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_DIMENSION_MISMATCH",
            "RIP output dimensions " + std::to_string(width) + "x"
                + std::to_string(height) + " do not match Package grid "
                + std::to_string(request.expected_width_px) + "x"
                + std::to_string(request.expected_height_px));
    }
    if (bitsPerSample != 8U || sampleFormat != SAMPLEFORMAT_UINT
        || samplesPerPixel < 7U
        || planarConfig != PLANARCONFIG_CONTIG)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_SAMPLE_LAYOUT_INVALID",
            "RIP output must be unsigned 8-bit contiguous data with at least 7 samples per pixel");
    }
    if (TIFFIsTiled(handle.get()) != 0 || rowsPerStrip == 0U)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_STORAGE_INVALID",
            "RIP output must be stripped and must not be tiled");
    }
    const std::array<std::uint8_t, 3> limits = request.gray_bits == 1
        ? std::array<std::uint8_t, 3>{2U, 3U, 3U}
        : std::array<std::uint8_t, 3>{6U, 9U, 9U};
    constexpr std::array<std::string_view, 3> channelNames{
        "W", "S", "V"};
    const tmsize_t scanlineSize = TIFFScanlineSize(handle.get());
    const std::uint64_t requiredBytes =
        static_cast<std::uint64_t>(width) * samplesPerPixel;
    if (scanlineSize <= 0
        || static_cast<std::uint64_t>(scanlineSize) < requiredBytes)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_SCANLINE_SIZE_INVALID",
            "RIP output scanline storage is smaller than its declared samples");
    }
    std::vector<std::uint8_t> row(static_cast<std::size_t>(scanlineSize));
    std::array<std::uint8_t, 3> minima{255U, 255U, 255U};
    std::array<std::uint8_t, 3> maxima{};
    for (std::uint32_t y{0U}; y < height; ++y)
    {
        if (IsCancelled(request))
        {
            return RipStatus::Failure(
                "RIP_VALIDATION_CANCELLED",
                "RIP output validation was cancelled");
        }
        if (TIFFReadScanline(handle.get(), row.data(), y, 0U) < 0)
        {
            return RipStatus::Failure(
                "RIP_OUTPUT_SCANLINE_READ_FAILED",
                "a staged RIP TIFF scanline cannot be decoded");
        }
        for (std::uint32_t x{0U}; x < request.expected_width_px; ++x)
        {
            const std::size_t offset =
                static_cast<std::size_t>(x) * samplesPerPixel;
            for (std::size_t channel{0U}; channel < 3U; ++channel)
            {
                const std::uint8_t value = row.at(offset + 4U + channel);
                minima[channel] = (std::min)(minima[channel], value);
                maxima[channel] = (std::max)(maxima[channel], value);
                if (value > limits[channel])
                {
                    ++output->samples_exceeding_drop_limit[channel];
                    if (firstViolation->channel.empty())
                    {
                        *firstViolation = RipOutputDropViolation{
                            staged.index,
                            std::string(channelNames[channel]),
                            value,
                            limits[channel],
                            x,
                            y};
                    }
                    if (request.validation_mode
                        == RipOutputValidationMode::StrictS2)
                    {
                        return RipStatus::Failure(
                            "RIP_OUTPUT_DROP_LIMIT_EXCEEDED",
                            "RIP output layer "
                                + std::to_string(staged.index) + " channel "
                                + std::string(channelNames[channel]) + " sample "
                                + std::to_string(value) + " exceeds grayBits="
                                + std::to_string(request.gray_bits) + " limit "
                                + std::to_string(limits[channel]) + " at ("
                                + std::to_string(x) + ","
                                + std::to_string(y) + ")");
                    }
                }
            }
        }
    }
    if (TIFFReadDirectory(handle.get()) == 1)
    {
        return RipStatus::Failure(
            "RIP_OUTPUT_MULTIPAGE_UNSUPPORTED",
            "each RIP output file must contain exactly one TIFF directory");
    }
    output->layer_index = staged.index;
    output->path = NormalizedLayerPath(
        request.staging_directory, staged.index);
    output->minimum_white = minima[0];
    output->minimum_support = minima[1];
    output->minimum_varnish = minima[2];
    output->maximum_white = maxima[0];
    output->maximum_support = maxima[1];
    output->maximum_varnish = maxima[2];
    *requiresWidthNormalization = vendorAlignedWidth;
    return RipStatus::Success();
}

}  // namespace

RipOutputValidationResult ValidateAndNormalizeRipOutput(
    const RipOutputValidationRequest& request)
{
    RipOutputValidationResult result;
    if (request.gray_bits != 1 && request.gray_bits != 2)
    {
        result.status = RipStatus::Failure(
            "RIP_SETTINGS_GRAY_BITS_INVALID",
            "RIP device gray bits must be 1 or 2");
        return result;
    }
    if (request.validation_mode != RipOutputValidationMode::StrictS2
        && request.validation_mode
            != RipOutputValidationMode::DiagnosticUnvalidated)
    {
        result.status = RipStatus::Failure(
            "RIP_OUTPUT_VALIDATION_MODE_INVALID",
            "RIP output validation mode is unsupported");
        return result;
    }
    if (!request.package_directory.is_absolute()
        || !request.staging_directory.is_absolute()
        || request.expected_layer_count == 0U
        || request.expected_layer_count > 1000000U
        || request.expected_width_px == 0U
        || request.expected_height_px == 0U)
    {
        result.status = RipStatus::Failure(
            "RIP_OUTPUT_REQUEST_INVALID",
            "RIP output validation requires absolute paths and positive expected metadata");
        return result;
    }
    std::error_code error;
    if (!std::filesystem::is_directory(request.package_directory, error)
        || error
        || !std::filesystem::is_directory(request.staging_directory, error)
        || error
        || IsReparsePoint(request.package_directory)
        || IsReparsePoint(request.staging_directory)
        || !IsContained(request.package_directory, request.staging_directory))
    {
        result.status = RipStatus::Failure(
            "RIP_OUTPUT_STAGING_PATH_INVALID",
            "the staging directory must be a real directory contained by the Package");
        return result;
    }
    const std::filesystem::path canonicalPackage =
        CanonicalAbsolute(request.package_directory, error);
    const std::filesystem::path canonicalStageParent =
        CanonicalAbsolute(request.staging_directory.parent_path(), error);
    if (error || !ComponentEqual(canonicalPackage, canonicalStageParent)
        || !request.staging_directory.filename().string().starts_with(
            ".rip.staging."))
    {
        result.status = RipStatus::Failure(
            "RIP_OUTPUT_STAGING_PATH_INVALID",
            "the staging directory must be a direct Package child named .rip.staging.<attempt>");
        return result;
    }

    std::vector<StagedLayer> stagedLayers;
    for (std::filesystem::directory_iterator iterator(
             request.staging_directory, error), end;
         !error && iterator != end;
         iterator.increment(error))
    {
        const std::filesystem::directory_entry& entry = *iterator;
        if (IsReparsePoint(entry.path()))
        {
            result.status = RipStatus::Failure(
                "RIP_OUTPUT_REPARSE_POINT_FORBIDDEN",
                "staged RIP output cannot contain links or reparse points");
            return result;
        }
        if (!entry.is_regular_file(error) || error)
        {
            result.status = RipStatus::Failure(
                "RIP_OUTPUT_DIRECTORY_ENTRY_INVALID",
                "staged RIP output may only contain regular files");
            return result;
        }
        if (!IsTiffExtension(entry.path()))
        {
            continue;
        }
        const std::optional<std::size_t> index = ParseSliceIndex(entry.path());
        if (!index.has_value())
        {
            result.status = RipStatus::Failure(
                "RIP_OUTPUT_FILE_NAME_INVALID",
                "every staged TIFF must use the vendor slice.N.tif[f] pattern");
            return result;
        }
        stagedLayers.push_back(StagedLayer{*index, entry.path()});
    }
    if (error)
    {
        result.status = RipStatus::Failure(
            "RIP_OUTPUT_DIRECTORY_READ_FAILED",
            "the staged RIP directory cannot be enumerated");
        return result;
    }
    std::sort(
        stagedLayers.begin(), stagedLayers.end(),
        [](const StagedLayer& left, const StagedLayer& right)
        {
            return left.index < right.index;
        });
    if (stagedLayers.size() != request.expected_layer_count)
    {
        result.status = RipStatus::Failure(
            "RIP_OUTPUT_LAYER_COUNT_MISMATCH",
            "the staged RIP layer count does not match the source Package");
        return result;
    }
    std::vector<bool> requiresWidthNormalization;
    requiresWidthNormalization.reserve(stagedLayers.size());
    for (std::size_t index{0U}; index < stagedLayers.size(); ++index)
    {
        if (IsCancelled(request))
        {
            result.status = RipStatus::Failure(
                "RIP_VALIDATION_CANCELLED",
                "RIP output validation was cancelled");
            result.layers.clear();
            return result;
        }
        if (stagedLayers[index].index != index)
        {
            result.status = RipStatus::Failure(
                "RIP_OUTPUT_LAYER_INDEX_INVALID",
                "staged RIP layer indices must be unique and contiguous from zero");
            return result;
        }
        RipOutputLayer layer;
        bool normalizeWidth{false};
        RipOutputDropViolation firstViolation;
        const RipStatus layerStatus = ValidateLayer(
            stagedLayers[index], request, &layer, &normalizeWidth,
            &firstViolation);
        if (!layerStatus.ok)
        {
            result.status = layerStatus;
            result.layers.clear();
            return result;
        }
        for (std::size_t channel{0U}; channel < 3U; ++channel)
        {
            result.samples_exceeding_drop_limit[channel] +=
                layer.samples_exceeding_drop_limit[channel];
        }
        if (!firstViolation.channel.empty()
            && !result.first_drop_violation.has_value())
        {
            result.first_drop_violation = std::move(firstViolation);
        }
        if (layer.path.empty()
            || !IsContained(request.staging_directory, layer.path)
            || std::filesystem::exists(layer.path, error)
            || error)
        {
            result.status = RipStatus::Failure(
                "RIP_OUTPUT_NORMALIZED_PATH_INVALID",
                "a normalized RIP layer target is unsafe or already exists");
            result.layers.clear();
            return result;
        }
        result.layers.push_back(std::move(layer));
        requiresWidthNormalization.push_back(normalizeWidth);
    }

    for (std::size_t index{0U}; index < stagedLayers.size(); ++index)
    {
        if (!requiresWidthNormalization[index])
        {
            continue;
        }
        const RipStatus normalizeStatus = RewriteWithExpectedWidth(
            stagedLayers[index], request, result.layers[index].path);
        if (!normalizeStatus.ok)
        {
            result.status = normalizeStatus;
            result.layers.clear();
            return result;
        }
    }

    for (std::size_t index{0U}; index < stagedLayers.size(); ++index)
    {
        bool normalized{false};
        if (requiresWidthNormalization[index])
        {
            normalized = std::filesystem::remove(
                stagedLayers[index].source, error);
        }
        else
        {
            std::filesystem::rename(
                stagedLayers[index].source, result.layers[index].path, error);
            normalized = !error;
        }
        if (error || !normalized)
        {
            result.status = RipStatus::Failure(
                "RIP_OUTPUT_NORMALIZE_RENAME_FAILED",
                "a validated RIP layer could not replace its vendor staging file");
            result.layers.clear();
            return result;
        }
    }
    result.s2_drop_limits_passed =
        std::all_of(
            result.samples_exceeding_drop_limit.begin(),
            result.samples_exceeding_drop_limit.end(),
            [](const std::uint64_t count)
            {
                return count == 0U;
            });
    result.status = RipStatus::Success();
    return result;
}

}  // namespace slicesoft::rip
