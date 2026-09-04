#include "slicer_core/output/rgbwsvt/RgbwsvtPackageReader.h"

#include "slicer_core/json_value.h"
#include "slicer_core/materials/transfer/TransferChannelError.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace slicer_core
{
namespace
{
constexpr std::size_t kChannelCount{7U};
[[noreturn]] void Fail(
    const ValidationErrorCode code,
    const std::string& message)
{
    throw ValidationError(code, message);
}
std::string ReadFileBytes(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        Fail(
            ValidationErrorCode::ManifestMissing,
            "manifest is missing: " + path.generic_string());
    }
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}
Json ParseManifest(const std::filesystem::path& path, std::string* bytes)
{
    *bytes = ReadFileBytes(path);
    try
    {
        std::istringstream input{*bytes};
        Json manifest = Json::parse(input);
        if (!manifest.is_object())
        {
            Fail(
                ValidationErrorCode::ManifestParseFailed,
                "manifest root must be an object");
        }
        return manifest;
    }
    catch (const ValidationError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        Fail(
            ValidationErrorCode::ManifestParseFailed,
            "manifest parse failed: " + std::string{error.what()});
    }
}
const Json& RequireObject(
    const Json& object,
    const std::string& key,
    const ValidationErrorCode code)
{
    if (!object.contains(key) || !object.at(key).is_object())
    {
        Fail(code, "manifest." + key + " must be an object");
    }
    return object.at(key);
}
int RequirePositiveInt(
    const Json& object,
    const std::string& key,
    const ValidationErrorCode code)
{
    if (!object.contains(key) || !object.at(key).is_number())
    {
        Fail(code, "manifest field must be numeric: " + key);
    }
    const int value = object.at(key).as_int();
    if (value <= 0)
    {
        Fail(code, "manifest field must be positive: " + key);
    }
    return value;
}
std::string RequireString(
    const Json& object,
    const std::string& key,
    const ValidationErrorCode code)
{
    if (!object.contains(key) || !object.at(key).is_string()
        || object.at(key).as_string().empty())
    {
        Fail(code, "manifest field must be a non-empty string: " + key);
    }
    return object.at(key).as_string();
}
std::uint64_t RequireCount(
    const Json& object,
    const std::string& key)
{
    if (!object.contains(key) || !object.at(key).is_number())
    {
        Fail(
            ValidationErrorCode::LayerStatisticsMismatch,
            "channel statistic is missing: " + key);
    }
    const double value = object.at(key).as_double();
    if (!std::isfinite(value) || value < 0.0 || std::floor(value) != value
        || value > static_cast<double>(std::numeric_limits<std::uint64_t>::max()))
    {
        Fail(
            ValidationErrorCode::LayerStatisticsMismatch,
            "channel statistic is not a non-negative integer: " + key);
    }
    return static_cast<std::uint64_t>(value);
}

std::string FileMetadataIdentity(const std::filesystem::path& path)
{
    std::error_code error;
    const std::uintmax_t bytes = std::filesystem::file_size(path, error);
    if (error)
    {
        return {};
    }
    const auto modified = std::filesystem::last_write_time(path, error);
    if (error)
    {
        return {};
    }
    return ComputeSha256(
        path.generic_string() + "|" + std::to_string(bytes) + "|"
        + std::to_string(modified.time_since_epoch().count()));
}

bool IsWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate)
{
    const std::filesystem::path relative = candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute())
    {
        return false;
    }
    const auto first = relative.begin();
    return first == relative.end() || *first != "..";
}

void MergeStats(
    std::vector<TiffChannelStats>& totals,
    const std::vector<TiffChannelStats>& layer)
{
    for (std::size_t channel = 0U; channel < totals.size(); ++channel)
    {
        TiffChannelStats& total = totals[channel];
        const TiffChannelStats& current = layer[channel];
        total.print_pixels += current.print_pixels;
        total.full_print_pixels += current.full_print_pixels;
        total.partial_print_pixels += current.partial_print_pixels;
        total.empty_pixels += current.empty_pixels;
        total.min_value = std::min(total.min_value, current.min_value);
        total.max_value = std::max(total.max_value, current.max_value);
    }
}

bool StatsEqual(const TiffChannelStats& left, const Json& right)
{
    return left.print_pixels == RequireCount(right, "printPixels")
        && left.full_print_pixels == RequireCount(right, "fullPrintPixels")
        && left.partial_print_pixels == RequireCount(right, "partialPrintPixels")
        && left.empty_pixels == RequireCount(right, "emptyPixels")
        && left.min_value == right.at("minValue").as_int()
        && left.max_value == right.at("maxValue").as_int();
}

std::vector<std::string> ReadChannelOrder(const Json& tiff)
{
    static const std::vector<std::string> expected{
        "R", "G", "B", "W", "S", "V", "T"};
    if (!tiff.contains("channelOrder")
        || !tiff.at("channelOrder").is_array()
        || tiff.at("channelOrder").size() != expected.size())
    {
        Fail(
            ValidationErrorCode::ChannelOrderInvalid,
            "RGBWSVT channel order must contain seven channels");
    }
    std::vector<std::string> channels;
    for (std::size_t index = 0U; index < expected.size(); ++index)
    {
        const Json& value = tiff.at("channelOrder").at(index);
        if (!value.is_string() || value.as_string() != expected[index])
        {
            Fail(
                ValidationErrorCode::ChannelOrderInvalid,
                "RGBWSVT channel order must be R G B W S V T");
        }
        channels.push_back(value.as_string());
    }
    return channels;
}

void ValidateFixedTiffContract(const Json& tiff)
{
    if (RequirePositiveInt(
            tiff, "channelCount", ValidationErrorCode::ChannelCountInvalid)
        != static_cast<int>(kChannelCount))
    {
        Fail(
            ValidationErrorCode::ChannelCountInvalid,
            "RGBWSVT channel count must be seven");
    }
    if (RequirePositiveInt(
            tiff, "bitDepth", ValidationErrorCode::BitDepthInvalid) != 8)
    {
        Fail(ValidationErrorCode::BitDepthInvalid, "bit depth must be 8");
    }
    if (RequireString(
            tiff, "sampleFormat", ValidationErrorCode::TiffReadFailed) != "uint"
        || RequireString(
            tiff, "planarConfig", ValidationErrorCode::TiffPlanarConfigInvalid)
            != "contiguous")
    {
        Fail(
            ValidationErrorCode::TiffPlanarConfigInvalid,
            "RGBWSVT TIFF must be contiguous uint8");
    }
    if (RequireString(tiff, "polarity", ValidationErrorCode::PolarityInvalid)
            != "black_is_print"
        || !tiff.contains("printValue") || tiff.at("printValue").as_int() != 0
        || !tiff.contains("emptyValue") || tiff.at("emptyValue").as_int() != 255)
    {
        Fail(
            ValidationErrorCode::PrintEmptyValueInvalid,
            "RGBWSVT polarity and print/empty values are invalid");
    }
}

RgbwsvtPackageLayer ValidateLayer(
    const Json& layer,
    const std::filesystem::path& packageDirectory,
    const int width,
    const int height,
    const TiffStorageMode storage,
    const TiffCompressionMode compression)
{
    if (!layer.is_object())
    {
        Fail(ValidationErrorCode::LayerListInvalid, "layer must be an object");
    }
    RgbwsvtPackageLayer result;
    if (!layer.contains("index") || !layer.at("index").is_number())
    {
        Fail(ValidationErrorCode::LayerListInvalid, "layer index is missing");
    }
    result.index = layer.at("index").as_int();
    if (result.index < 0 || !layer.contains("zMm")
        || !layer.at("zMm").is_number() || layer.at("zMm").as_double() < 0.0)
    {
        Fail(ValidationErrorCode::LayerListInvalid, "layer index/zMm is invalid");
    }
    result.zMm = layer.at("zMm").as_double();
    result.width = static_cast<std::uint32_t>(RequirePositiveInt(
        layer, "widthPx", ValidationErrorCode::LayerSizeMismatch));
    result.height = static_cast<std::uint32_t>(RequirePositiveInt(
        layer, "heightPx", ValidationErrorCode::LayerSizeMismatch));
    if (result.width != static_cast<std::uint32_t>(width)
        || result.height != static_cast<std::uint32_t>(height))
    {
        Fail(
            ValidationErrorCode::LayerSizeMismatch,
            "layer dimensions do not match manifest grid");
    }
    const std::filesystem::path relative{
        RequireString(layer, "path", ValidationErrorCode::LayerListInvalid)};
    if (relative.is_absolute())
    {
        Fail(ValidationErrorCode::LayerListInvalid, "layer path must be relative");
    }
    result.path = std::filesystem::weakly_canonical(packageDirectory / relative);
    if (!IsWithin(packageDirectory, result.path)
        || !std::filesystem::is_regular_file(result.path))
    {
        Fail(
            ValidationErrorCode::LayerMissing,
            "layer path is missing or escapes package: "
                + result.path.generic_string());
    }
    result.storage = storage;
    result.compression = compression;
    RgbwsvtTiffReadResult decoded;
    try
    {
        decoded = ReadRgbwsvtTiff(result.path);
    }
    catch (const TransferChannelError& error)
    {
        Fail(ValidationErrorCode::TiffReadFailed, error.what());
    }
    if (decoded.spec.width != result.width || decoded.spec.height != result.height)
    {
        Fail(
            ValidationErrorCode::LayerSizeMismatch,
            "decoded TIFF dimensions do not match manifest");
    }
    if (decoded.spec.storage_mode != storage)
    {
        Fail(
            ValidationErrorCode::TiffStorageMismatch,
            "decoded TIFF storage does not match manifest");
    }
    if (decoded.spec.compression_mode != compression)
    {
        Fail(
            ValidationErrorCode::TiffCompressionMismatch,
            "decoded TIFF compression does not match manifest");
    }
    result.checksums.assign(
        decoded.channelChecksums.begin(), decoded.channelChecksums.end());
    result.channelStats.assign(
        decoded.channelStats.begin(), decoded.channelStats.end());
    result.fileIdentity = FileMetadataIdentity(result.path);
    return result;
}

}  // namespace

RgbwsvtPackageValidation ValidateRgbwsvtPackage(
    const std::filesystem::path& packageDirectory)
{
    RgbwsvtPackageValidation result;
    result.packageDirectory = std::filesystem::weakly_canonical(packageDirectory);
    result.manifestPath = result.packageDirectory / "manifest.json";
    std::string manifestBytes;
    const Json manifest = ParseManifest(result.manifestPath, &manifestBytes);
    result.schema = ReadPackageManifestSchema(result.packageDirectory);
    if (result.schema != "p0.rgbwsvt.1")
    {
        Fail(
            ValidationErrorCode::SchemaUnsupported,
            "RGBWSVT reader requires p0.rgbwsvt.1");
    }
    result.productionAcceptance = RequireString(
        manifest, "productionAcceptance", ValidationErrorCode::SchemaUnsupported);
    if (result.productionAcceptance != "admitted"
        && result.productionAcceptance != "rgbwsvt_candidate_unvalidated")
    {
        Fail(
            ValidationErrorCode::SchemaUnsupported,
            "RGBWSVT productionAcceptance is unsupported");
    }
    result.manifestHash = ComputeSha256(manifestBytes);

    const Json& grid = RequireObject(
        manifest, "grid", ValidationErrorCode::GridInvalid);
    result.widthPx = RequirePositiveInt(
        grid, "widthPx", ValidationErrorCode::GridInvalid);
    result.heightPx = RequirePositiveInt(
        grid, "heightPx", ValidationErrorCode::GridInvalid);
    result.layerCount = RequirePositiveInt(
        grid, "layerCount", ValidationErrorCode::GridInvalid);
    result.dpiX = RequirePositiveInt(grid, "dpiX", ValidationErrorCode::GridInvalid);
    result.dpiY = RequirePositiveInt(grid, "dpiY", ValidationErrorCode::GridInvalid);

    const Json& tiff = RequireObject(
        manifest, "tiff", ValidationErrorCode::ChannelCountInvalid);
    ValidateFixedTiffContract(tiff);
    result.channelOrder = ReadChannelOrder(tiff);
    const std::string storage = RequireString(
        tiff, "storageMode", ValidationErrorCode::TiffStorageModeInvalid);
    if (storage != "stripped" && storage != "tiled")
    {
        Fail(
            ValidationErrorCode::TiffStorageModeInvalid,
            "storageMode must be stripped or tiled");
    }
    result.storageMode = storage;
    const TiffStorageMode storageMode = storage == "stripped"
        ? TiffStorageMode::Stripped : TiffStorageMode::Tiled;
    result.compression = RequireString(
        tiff, "compression", ValidationErrorCode::TiffCompressionInvalid);
    TiffCompressionMode compressionMode;
    try
    {
        compressionMode = ParseTiffCompressionMode(result.compression);
    }
    catch (const std::invalid_argument& error)
    {
        Fail(ValidationErrorCode::TiffCompressionInvalid, error.what());
    }

    if (!manifest.contains("layers") || !manifest.at("layers").is_array()
        || manifest.at("layers").size() != static_cast<std::size_t>(result.layerCount))
    {
        Fail(
            ValidationErrorCode::LayerCountMismatch,
            "manifest layers do not match grid.layerCount");
    }
    result.totalChannelStats.resize(kChannelCount);
    std::set<int> indices;
    std::set<std::filesystem::path> paths;
    std::string identitySeed = result.manifestHash;
    for (const Json& layer : manifest.at("layers").as_array())
    {
        RgbwsvtPackageLayer validated = ValidateLayer(
            layer, result.packageDirectory, result.widthPx, result.heightPx,
            storageMode, compressionMode);
        if (!indices.insert(validated.index).second
            || !paths.insert(validated.path).second)
        {
            Fail(
                ValidationErrorCode::LayerListInvalid,
                "RGBWSVT layer index/path must be unique");
        }
        MergeStats(result.totalChannelStats, validated.channelStats);
        identitySeed += "|" + validated.fileIdentity;
        result.layers.push_back(std::move(validated));
    }
    std::sort(
        result.layers.begin(), result.layers.end(),
        [](const RgbwsvtPackageLayer& left, const RgbwsvtPackageLayer& right)
        {
            return left.index < right.index;
        });

    const Json& declaredStats = RequireObject(
        tiff, "channelStats", ValidationErrorCode::LayerStatisticsMismatch);
    for (std::size_t channel = 0U; channel < result.channelOrder.size(); ++channel)
    {
        const std::string& name = result.channelOrder[channel];
        if (!declaredStats.contains(name) || !declaredStats.at(name).is_object()
            || !StatsEqual(result.totalChannelStats[channel], declaredStats.at(name)))
        {
            Fail(
                ValidationErrorCode::LayerStatisticsMismatch,
                "persisted TIFF statistics do not match manifest channel " + name);
        }
    }
    result.packageIdentity = ComputeSha256(identitySeed);
    return result;
}

RgbwsvtDecodedPackageLayer ReadRgbwsvtPackageLayer(
    const RgbwsvtPackageLayer& layer)
{
    try
    {
        const RgbwsvtTiffReadResult decoded = ReadRgbwsvtTiff(layer.path);
        if (decoded.spec.width != layer.width || decoded.spec.height != layer.height
            || decoded.spec.storage_mode != layer.storage
            || decoded.spec.compression_mode != layer.compression)
        {
            Fail(
                ValidationErrorCode::TiffStorageMismatch,
                "RGBWSVT layer no longer matches its verified descriptor");
        }
        return RgbwsvtDecodedPackageLayer{layer, decoded.pixels};
    }
    catch (const TransferChannelError& error)
    {
        Fail(ValidationErrorCode::TiffReadFailed, error.what());
    }
}

bool IsRgbwsvtPackageSnapshotCurrent(
    const RgbwsvtPackageValidation& package)
{
    try
    {
        const std::string bytes = ReadFileBytes(package.manifestPath);
        if (ComputeSha256(bytes) != package.manifestHash)
        {
            return false;
        }
        return std::all_of(
            package.layers.begin(), package.layers.end(),
            [](const RgbwsvtPackageLayer& layer)
            {
                return FileMetadataIdentity(layer.path) == layer.fileIdentity;
            });
    }
    catch (const ValidationError&)
    {
        return false;
    }
}

}  // namespace slicer_core
