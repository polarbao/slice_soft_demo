#include "slicer_core/preview/TiffLayerSource.h"

#include "slicer_core/json_value.h"
#include "slicer_core/system/Sha256.h"
#include "slicer_core/TiffReadApi.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr const char* kCurrentSchema{"p0.rgbwsv.2"};
constexpr std::array<const char*, rgbwsv_channel_count> kChannelOrder{
    "R", "G", "B", "W", "S", "V"};

[[noreturn]] void Fail(
    const TiffLayerErrorCode code,
    const std::string& message,
    const std::string& packageIdentity = {},
    const int layerIndex = -1,
    const std::filesystem::path& path = {},
    const std::string& sourceCode = {})
{
    throw TiffLayerError(
        code,
        message,
        packageIdentity,
        layerIndex,
        path,
        sourceCode);
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        Fail(
            TiffLayerErrorCode::PackageNotFound,
            "manifest file is missing or unreadable",
            {},
            -1,
            path);
    }
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

Json ParseManifest(
    const std::string& payload,
    const std::filesystem::path& path)
{
    try
    {
        std::istringstream input{payload};
        const Json root = Json::parse(input);
        if (!root.is_object())
        {
            Fail(
                TiffLayerErrorCode::ManifestInvalid,
                "manifest root must be an object",
                {},
                -1,
                path);
        }
        return root;
    }
    catch (const TiffLayerError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest JSON parsing failed",
            {},
            -1,
            path,
            error.what());
    }
}

const Json& RequireObject(
    const Json& object,
    const std::string& key,
    const std::filesystem::path& path)
{
    if (!object.contains(key) || !object.at(key).is_object())
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest field must be an object: " + key,
            {},
            -1,
            path);
    }
    return object.at(key);
}

const Json& RequireArray(
    const Json& object,
    const std::string& key,
    const std::filesystem::path& path)
{
    if (!object.contains(key) || !object.at(key).is_array())
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest field must be an array: " + key,
            {},
            -1,
            path);
    }
    return object.at(key);
}

int RequirePositiveInt(
    const Json& object,
    const std::string& key,
    const std::filesystem::path& path)
{
    try
    {
        const int value = object.at(key).as_int();
        if (value <= 0)
        {
            Fail(
                TiffLayerErrorCode::ManifestInvalid,
                "manifest field must be positive: " + key,
                {},
                -1,
                path);
        }
        return value;
    }
    catch (const TiffLayerError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest field must be an integer: " + key,
            {},
            -1,
            path,
            error.what());
    }
}

int RequireLayerIndex(
    const Json& object,
    const std::filesystem::path& path)
{
    try
    {
        const int value = object.at("index").as_int();
        if (value < 0)
        {
            Fail(
                TiffLayerErrorCode::ManifestInvalid,
                "layer index must be non-negative",
                {},
                value,
                path);
        }
        return value;
    }
    catch (const TiffLayerError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest layer index is missing or invalid",
            {},
            -1,
            path,
            error.what());
    }
}

double RequireFiniteNumber(
    const Json& object,
    const std::string& key,
    const int layerIndex,
    const std::filesystem::path& path)
{
    try
    {
        const double value = object.at(key).as_double();
        if (!std::isfinite(value))
        {
            Fail(
                TiffLayerErrorCode::ManifestInvalid,
                "manifest layer value must be finite: " + key,
                {},
                layerIndex,
                path);
        }
        return value;
    }
    catch (const TiffLayerError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest layer value is missing or invalid: " + key,
            {},
            layerIndex,
            path,
            error.what());
    }
}

double RequirePositiveFiniteNumber(
    const Json& object,
    const std::string& key,
    const std::filesystem::path& path)
{
    const double value = RequireFiniteNumber(object, key, -1, path);
    if (value <= 0.0)
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest field must be finite and positive: " + key,
            {},
            -1,
            path);
    }
    return value;
}

std::vector<double> RequireFiniteNumberArray(
    const Json& object,
    const std::string& key,
    const std::size_t expectedSize,
    const bool requirePositive,
    const std::filesystem::path& path)
{
    const Json& values = RequireArray(object, key, path);
    if (values.size() != expectedSize)
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest array has an invalid size: " + key,
            {},
            -1,
            path);
    }

    std::vector<double> result;
    result.reserve(expectedSize);
    try
    {
        for (const Json& item : values.as_array())
        {
            const double value = item.as_double();
            if (!std::isfinite(value)
                || (requirePositive && value <= 0.0))
            {
                Fail(
                    TiffLayerErrorCode::ManifestInvalid,
                    "manifest array contains an invalid number: " + key,
                    {},
                    -1,
                    path);
            }
            result.push_back(value);
        }
    }
    catch (const TiffLayerError&)
    {
        throw;
    }
    catch (const std::exception& error)
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest array must contain finite numbers: " + key,
            {},
            -1,
            path,
            error.what());
    }
    return result;
}

std::string RequireString(
    const Json& object,
    const std::string& key,
    const std::filesystem::path& path)
{
    try
    {
        return object.at(key).as_string();
    }
    catch (const std::exception& error)
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest field must be a string: " + key,
            {},
            -1,
            path,
            error.what());
    }
}

void RequireProtocol(
    const Json& manifest,
    const Json& tiff,
    const std::filesystem::path& manifestPath)
{
    const auto RequireExactString =
        [&](const Json& object,
            const std::string& key,
            const std::string& expected)
    {
        try
        {
            if (object.at(key).as_string() != expected)
            {
                Fail(
                    TiffLayerErrorCode::ProtocolMismatch,
                    "production protocol mismatch: " + key,
                    {},
                    -1,
                    manifestPath);
            }
        }
        catch (const TiffLayerError&)
        {
            throw;
        }
        catch (const std::exception& error)
        {
            Fail(
                TiffLayerErrorCode::ProtocolMismatch,
                "production protocol field is missing or invalid: " + key,
                {},
                -1,
                manifestPath,
                error.what());
        }
    };
    const auto RequireExactInt =
        [&](const Json& object, const std::string& key, const int expected)
    {
        try
        {
            if (object.at(key).as_int() != expected)
            {
                Fail(
                    TiffLayerErrorCode::ProtocolMismatch,
                    "production protocol mismatch: " + key,
                    {},
                    -1,
                    manifestPath);
            }
        }
        catch (const TiffLayerError&)
        {
            throw;
        }
        catch (const std::exception& error)
        {
            Fail(
                TiffLayerErrorCode::ProtocolMismatch,
                "production protocol field is missing or invalid: " + key,
                {},
                -1,
                manifestPath,
                error.what());
        }
    };

    RequireExactString(manifest, "schema", kCurrentSchema);
    RequireExactInt(tiff, "channelCount", rgbwsv_channel_count);
    RequireExactInt(tiff, "bitDepth", 8);
    RequireExactString(tiff, "sampleFormat", "uint");
    RequireExactString(tiff, "planarConfig", "contiguous");
    RequireExactString(tiff, "polarity", "black_is_print");
    RequireExactInt(tiff, "printValue", 0);
    RequireExactInt(tiff, "emptyValue", 255);

    const Json& channels =
        RequireArray(tiff, "channelOrder", manifestPath);
    if (channels.size() != kChannelOrder.size())
    {
        Fail(
            TiffLayerErrorCode::ProtocolMismatch,
            "production channel order must contain six channels",
            {},
            -1,
            manifestPath);
    }
    for (std::size_t index{0U}; index < kChannelOrder.size(); ++index)
    {
        try
        {
            if (channels.at(index).as_string() != kChannelOrder.at(index))
            {
                Fail(
                    TiffLayerErrorCode::ProtocolMismatch,
                    "production channel order must be R G B W S V",
                    {},
                    -1,
                    manifestPath);
            }
        }
        catch (const TiffLayerError&)
        {
            throw;
        }
        catch (const std::exception& error)
        {
            Fail(
                TiffLayerErrorCode::ProtocolMismatch,
                "production channel order is invalid",
                {},
                -1,
                manifestPath,
                error.what());
        }
    }
}

TiffStorageMode ParseStorage(
    const Json& tiff,
    const std::filesystem::path& manifestPath)
{
    const std::string storage = tiff.contains("storageMode")
        ? RequireString(tiff, "storageMode", manifestPath)
        : RequireString(tiff, "storage", manifestPath);
    if (tiff.contains("storage")
        && RequireString(tiff, "storage", manifestPath) != storage)
    {
        Fail(
            TiffLayerErrorCode::ProtocolMismatch,
            "TIFF storage and storageMode fields disagree",
            {},
            -1,
            manifestPath);
    }
    if (tiff.contains("tiled"))
    {
        try
        {
            if (tiff.at("tiled").as_bool()
                != (storage == "tiled"))
            {
                Fail(
                    TiffLayerErrorCode::ProtocolMismatch,
                    "TIFF tiled flag disagrees with storage mode",
                    {},
                    -1,
                    manifestPath);
            }
        }
        catch (const TiffLayerError&)
        {
            throw;
        }
        catch (const std::exception& error)
        {
            Fail(
                TiffLayerErrorCode::ProtocolMismatch,
                "TIFF tiled flag is invalid",
                {},
                -1,
                manifestPath,
                error.what());
        }
    }
    if (storage == "stripped")
    {
        RequirePositiveInt(tiff, "rowsPerStrip", manifestPath);
        return TiffStorageMode::Stripped;
    }
    if (storage == "tiled")
    {
        try
        {
            const Json& tileSize =
                RequireArray(tiff, "tileSize", manifestPath);
            if (tileSize.size() != 2U
                || tileSize.at(0U).as_int() <= 0
                || tileSize.at(1U).as_int() <= 0)
            {
                Fail(
                    TiffLayerErrorCode::ProtocolMismatch,
                    "TIFF tile size must contain two positive integers",
                    {},
                    -1,
                    manifestPath);
            }
        }
        catch (const TiffLayerError&)
        {
            throw;
        }
        catch (const std::exception& error)
        {
            Fail(
                TiffLayerErrorCode::ProtocolMismatch,
                "TIFF tile size must contain two positive integers",
                {},
                -1,
                manifestPath,
                error.what());
        }
        return TiffStorageMode::Tiled;
    }

    Fail(
        TiffLayerErrorCode::ProtocolMismatch,
        "TIFF storage mode must be stripped or tiled",
        {},
        -1,
        manifestPath);
}

TiffCompressionMode ParseCompression(
    const Json& tiff,
    const std::filesystem::path& manifestPath)
{
    if (!tiff.contains("compression"))
    {
        return TiffCompressionMode::None;
    }
    const std::string compression =
        RequireString(tiff, "compression", manifestPath);
    try
    {
        return ParseTiffCompressionMode(compression);
    }
    catch (const std::invalid_argument& error)
    {
        Fail(
            TiffLayerErrorCode::ProtocolMismatch,
            "TIFF compression must be none or packbits",
            {},
            -1,
            manifestPath,
            error.what());
    }
}

bool IsPathWithin(
    const std::filesystem::path& directory,
    const std::filesystem::path& candidate)
{
    const std::filesystem::path normalizedDirectory =
        std::filesystem::weakly_canonical(directory);
    const std::filesystem::path normalizedCandidate =
        std::filesystem::weakly_canonical(candidate);
    auto directoryIterator = normalizedDirectory.begin();
    auto candidateIterator = normalizedCandidate.begin();
    for (; directoryIterator != normalizedDirectory.end();
         ++directoryIterator, ++candidateIterator)
    {
        if (candidateIterator == normalizedCandidate.end()
            || *candidateIterator != *directoryIterator)
        {
            return false;
        }
    }
    return true;
}

std::string ComputeFileMetadataIdentity(
    const std::filesystem::path& path)
{
    std::error_code error;
    const std::uintmax_t bytes = std::filesystem::file_size(path, error);
    if (error)
    {
        Fail(
            TiffLayerErrorCode::FileMissing,
            "TIFF file size is unavailable",
            {},
            -1,
            path,
            error.message());
    }
    const auto modified = std::filesystem::last_write_time(path, error);
    if (error)
    {
        Fail(
            TiffLayerErrorCode::FileMissing,
            "TIFF modification time is unavailable",
            {},
            -1,
            path,
            error.message());
    }
    const std::string payload =
        path.generic_string() + "|" + std::to_string(bytes) + "|"
        + std::to_string(modified.time_since_epoch().count());
    return ComputeSha256(payload);
}

bool IsCancelled(const TiffLayerLoadControl& control)
{
    return control.cancellationRequested
        && control.cancellationRequested();
}

bool IsStale(const TiffLayerLoadControl& control)
{
    return control.generationCurrent
        && !control.generationCurrent(control.requestGeneration);
}

void CheckControl(
    const TiffLayerLoadControl& control,
    const ProductionLayerRef& layer)
{
    if (IsCancelled(control))
    {
        Fail(
            TiffLayerErrorCode::Cancelled,
            "TIFF layer request was cancelled",
            layer.packageIdentity,
            layer.layerIndex,
            layer.path);
    }
    if (IsStale(control))
    {
        Fail(
            TiffLayerErrorCode::StaleResult,
            "TIFF layer request generation is stale",
            layer.packageIdentity,
            layer.layerIndex,
            layer.path);
    }
}

}  // namespace

std::string TiffLayerErrorCodeString(const TiffLayerErrorCode code)
{
    switch (code)
    {
    case TiffLayerErrorCode::PackageNotFound:
        return "TIFF_LAYER_PACKAGE_NOT_FOUND";
    case TiffLayerErrorCode::ManifestInvalid:
        return "TIFF_LAYER_MANIFEST_INVALID";
    case TiffLayerErrorCode::LayerNotListed:
        return "TIFF_LAYER_NOT_LISTED";
    case TiffLayerErrorCode::PathEscape:
        return "TIFF_LAYER_PATH_ESCAPE";
    case TiffLayerErrorCode::FileMissing:
        return "TIFF_LAYER_FILE_MISSING";
    case TiffLayerErrorCode::ProtocolMismatch:
        return "TIFF_LAYER_PROTOCOL_MISMATCH";
    case TiffLayerErrorCode::DimensionMismatch:
        return "TIFF_LAYER_DIMENSION_MISMATCH";
    case TiffLayerErrorCode::ReadFailed:
        return "TIFF_LAYER_READ_FAILED";
    case TiffLayerErrorCode::Cancelled:
        return "TIFF_LAYER_CANCELLED";
    case TiffLayerErrorCode::StaleResult:
        return "TIFF_LAYER_STALE_RESULT";
    }
    return "TIFF_LAYER_READ_FAILED";
}

TiffLayerError::TiffLayerError(
    const TiffLayerErrorCode code,
    std::string message,
    std::string packageIdentity,
    const int layerIndex,
    std::filesystem::path path,
    std::string sourceCode)
    : std::runtime_error(
          TiffLayerErrorCodeString(code) + ": " + std::move(message)),
      m_code(code),
      m_packageIdentity(std::move(packageIdentity)),
      m_layerIndex(layerIndex),
      m_path(std::move(path)),
      m_sourceCode(std::move(sourceCode))
{
}

TiffLayerErrorCode TiffLayerError::Code() const noexcept
{
    return m_code;
}

const std::string& TiffLayerError::PackageIdentity() const noexcept
{
    return m_packageIdentity;
}

int TiffLayerError::LayerIndex() const noexcept
{
    return m_layerIndex;
}

const std::filesystem::path& TiffLayerError::Path() const noexcept
{
    return m_path;
}

const std::string& TiffLayerError::SourceCode() const noexcept
{
    return m_sourceCode;
}

TiffLayerSource::TiffLayerSource()
    : TiffLayerSource(TiffLayerCacheLimits{})
{
}

TiffLayerSource::TiffLayerSource(
    const TiffLayerCacheLimits cacheLimits)
    : m_cache(cacheLimits)
{
}

ProductionPackageIndex TiffLayerSource::IndexPackage(
    const std::filesystem::path& manifestPath)
{
    const std::filesystem::path absoluteManifest =
        std::filesystem::absolute(manifestPath).lexically_normal();
    const std::string manifestPayload = ReadFile(absoluteManifest);
    const Json manifest = ParseManifest(manifestPayload, absoluteManifest);
    const Json& grid = RequireObject(manifest, "grid", absoluteManifest);
    const Json& tiff = RequireObject(manifest, "tiff", absoluteManifest);
    RequireProtocol(manifest, tiff, absoluteManifest);

    ProductionPackageIndex package;
    package.packageDirectory =
        std::filesystem::weakly_canonical(absoluteManifest.parent_path());
    package.manifestPath = absoluteManifest;
    package.packageIdentity =
        ComputeSha256(package.packageDirectory.generic_string());
    package.manifestHash = ComputeSha256(manifestPayload);
    package.width = static_cast<std::uint32_t>(
        RequirePositiveInt(grid, "widthPx", absoluteManifest));
    package.height = static_cast<std::uint32_t>(
        RequirePositiveInt(grid, "heightPx", absoluteManifest));
    const int expectedLayerCount =
        RequirePositiveInt(grid, "layerCount", absoluteManifest);
    package.dpiX = RequirePositiveInt(grid, "dpiX", absoluteManifest);
    package.dpiY = RequirePositiveInt(grid, "dpiY", absoluteManifest);
    const std::vector<double> origin = RequireFiniteNumberArray(
        grid,
        "originMm",
        3U,
        false,
        absoluteManifest);
    const std::vector<double> pixelSize = RequireFiniteNumberArray(
        grid,
        "pixelSizeMm",
        2U,
        true,
        absoluteManifest);
    package.originxmm = origin.at(0U);
    package.originymm = origin.at(1U);
    package.originzmm = origin.at(2U);
    package.pixelsizexmm = pixelSize.at(0U);
    package.pixelsizeymm = pixelSize.at(1U);
    package.layerthicknessmm = RequirePositiveFiniteNumber(
        grid,
        "layerThicknessMm",
        absoluteManifest);
    if (manifest.contains("scene"))
    {
        const Json& scene =
            RequireObject(manifest, "scene", absoluteManifest);
        package.sceneid =
            RequireString(scene, "sceneId", absoluteManifest);
        try
        {
            const int sceneRevision =
                scene.at("sceneRevision").as_int();
            if (package.sceneid.empty() || sceneRevision < 0)
            {
                Fail(
                    TiffLayerErrorCode::ManifestInvalid,
                    "manifest scene identity is invalid",
                    package.packageIdentity,
                    -1,
                    absoluteManifest);
            }
            package.scenerevision =
                static_cast<std::uint64_t>(sceneRevision);
            package.sceneidentityavailable = true;
        }
        catch (const TiffLayerError&)
        {
            throw;
        }
        catch (const std::exception& error)
        {
            Fail(
                TiffLayerErrorCode::ManifestInvalid,
                "manifest scene revision is missing or invalid",
                package.packageIdentity,
                -1,
                absoluteManifest,
                error.what());
        }
    }
    package.storage = ParseStorage(tiff, absoluteManifest);
    package.compression = ParseCompression(tiff, absoluteManifest);

    const Json& layers =
        RequireArray(manifest, "layers", absoluteManifest);
    const Json& tiffLayers =
        RequireArray(tiff, "layers", absoluteManifest);
    if (layers.size() != static_cast<std::size_t>(expectedLayerCount))
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest layer count does not match grid",
            package.packageIdentity,
            -1,
            absoluteManifest);
    }
    if (tiffLayers.dump(0) != layers.dump(0))
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest root and TIFF layer lists disagree",
            package.packageIdentity,
            -1,
            absoluteManifest);
    }

    package.layers.reserve(layers.size());
    for (const Json& layerJson : layers.as_array())
    {
        if (!layerJson.is_object())
        {
            Fail(
                TiffLayerErrorCode::ManifestInvalid,
                "manifest layer entry must be an object",
                package.packageIdentity,
                -1,
                absoluteManifest);
        }

        ProductionLayerRef layer;
        layer.packageIdentity = package.packageIdentity;
        layer.manifestHash = package.manifestHash;
        layer.layerIndex =
            RequireLayerIndex(layerJson, absoluteManifest);
        layer.zMm = RequireFiniteNumber(
            layerJson,
            "zMm",
            layer.layerIndex,
            absoluteManifest);
        layer.width = static_cast<std::uint32_t>(
            RequirePositiveInt(layerJson, "widthPx", absoluteManifest));
        layer.height = static_cast<std::uint32_t>(
            RequirePositiveInt(layerJson, "heightPx", absoluteManifest));
        if (layer.width != package.width || layer.height != package.height)
        {
            Fail(
                TiffLayerErrorCode::DimensionMismatch,
                "manifest layer dimensions do not match grid",
                package.packageIdentity,
                layer.layerIndex,
                absoluteManifest);
        }

        const std::filesystem::path relativePath{
            RequireString(layerJson, "path", absoluteManifest)};
        if (relativePath.empty() || relativePath.is_absolute())
        {
            Fail(
                TiffLayerErrorCode::PathEscape,
                "manifest TIFF path must be package-relative",
                package.packageIdentity,
                layer.layerIndex,
                relativePath);
        }
        layer.path =
            (package.packageDirectory / relativePath).lexically_normal();
        if (!IsPathWithin(package.packageDirectory, layer.path))
        {
            Fail(
                TiffLayerErrorCode::PathEscape,
                "manifest TIFF path escapes package directory",
                package.packageIdentity,
                layer.layerIndex,
                layer.path);
        }
        if (!std::filesystem::is_regular_file(layer.path))
        {
            Fail(
                TiffLayerErrorCode::FileMissing,
                "manifest-listed TIFF file is missing",
                package.packageIdentity,
                layer.layerIndex,
                layer.path);
        }

        layer.storage = package.storage;
        layer.compression = package.compression;
        layer.checksum = ComputeFileMetadataIdentity(layer.path);
        layer.dpiX = package.dpiX;
        layer.dpiY = package.dpiY;
        layer.originxmm = package.originxmm;
        layer.originymm = package.originymm;
        layer.originzmm = package.originzmm;
        layer.pixelsizexmm = package.pixelsizexmm;
        layer.pixelsizeymm = package.pixelsizeymm;
        layer.layerthicknessmm = package.layerthicknessmm;
        layer.sceneidentityavailable =
            package.sceneidentityavailable;
        layer.sceneid = package.sceneid;
        layer.scenerevision = package.scenerevision;
        layer.sourceIdentity = ComputeSha256(
            layer.packageIdentity + "|" + layer.manifestHash + "|"
            + std::to_string(layer.layerIndex) + "|" + layer.checksum);
        package.layers.push_back(std::move(layer));
    }

    std::sort(
        package.layers.begin(),
        package.layers.end(),
        [](const ProductionLayerRef& left, const ProductionLayerRef& right)
        {
            return left.layerIndex < right.layerIndex;
        });
    const auto duplicate = std::adjacent_find(
        package.layers.begin(),
        package.layers.end(),
        [](const ProductionLayerRef& left, const ProductionLayerRef& right)
        {
            return left.layerIndex == right.layerIndex;
        });
    if (duplicate != package.layers.end())
    {
        Fail(
            TiffLayerErrorCode::ManifestInvalid,
            "manifest layer indices must be unique",
            package.packageIdentity,
            duplicate->layerIndex,
            absoluteManifest);
    }

    std::string previousPackage;
    {
        std::scoped_lock lock{m_mutex};
        if (m_package.has_value())
        {
            previousPackage = m_package->packageIdentity;
        }
        m_package = package;
    }
    if (!previousPackage.empty())
    {
        m_cache.ClearPackage(previousPackage);
    }
    return package;
}

std::optional<ProductionLayerRef> TiffLayerSource::FindLayer(
    const int layerIndex) const
{
    std::scoped_lock lock{m_mutex};
    if (!m_package.has_value())
    {
        return std::nullopt;
    }
    const auto iterator = std::lower_bound(
        m_package->layers.begin(),
        m_package->layers.end(),
        layerIndex,
        [](const ProductionLayerRef& layer, const int index)
        {
            return layer.layerIndex < index;
        });
    if (iterator == m_package->layers.end()
        || iterator->layerIndex != layerIndex)
    {
        return std::nullopt;
    }
    return *iterator;
}

TiffLayerLoadResult TiffLayerSource::LoadLayer(
    const ProductionLayerRef& layer,
    const TiffLayerLoadControl& control)
{
    ProductionLayerRef indexedLayer;
    std::filesystem::path manifestPath;
    {
        std::scoped_lock lock{m_mutex};
        if (!m_package.has_value()
            || m_package->packageIdentity != layer.packageIdentity)
        {
            Fail(
                TiffLayerErrorCode::LayerNotListed,
                "layer does not belong to the current indexed package",
                layer.packageIdentity,
                layer.layerIndex,
                layer.path);
        }
        const auto iterator = std::lower_bound(
            m_package->layers.begin(),
            m_package->layers.end(),
            layer.layerIndex,
            [](const ProductionLayerRef& candidate, const int index)
            {
                return candidate.layerIndex < index;
            });
        if (iterator == m_package->layers.end()
            || iterator->layerIndex != layer.layerIndex
            || iterator->sourceIdentity != layer.sourceIdentity)
        {
            Fail(
                TiffLayerErrorCode::LayerNotListed,
                "layer is not the exact manifest-listed source",
                layer.packageIdentity,
                layer.layerIndex,
                layer.path);
        }
        indexedLayer = *iterator;
        manifestPath = m_package->manifestPath;
    }

    CheckControl(control, indexedLayer);
    if (ComputeSha256(ReadFile(manifestPath)) != indexedLayer.manifestHash)
    {
        m_cache.ClearPackage(indexedLayer.packageIdentity);
        Fail(
            TiffLayerErrorCode::StaleResult,
            "manifest changed after package indexing",
            indexedLayer.packageIdentity,
            indexedLayer.layerIndex,
            manifestPath);
    }
    if (!std::filesystem::is_regular_file(indexedLayer.path))
    {
        Fail(
            TiffLayerErrorCode::FileMissing,
            "manifest-listed TIFF file is missing",
            indexedLayer.packageIdentity,
            indexedLayer.layerIndex,
            indexedLayer.path);
    }
    if (ComputeFileMetadataIdentity(indexedLayer.path)
        != indexedLayer.checksum)
    {
        m_cache.ClearPackage(indexedLayer.packageIdentity);
        Fail(
            TiffLayerErrorCode::StaleResult,
            "TIFF file changed after package indexing",
            indexedLayer.packageIdentity,
            indexedLayer.layerIndex,
            indexedLayer.path);
    }

    const TiffLayerCacheKey key{
        indexedLayer.packageIdentity,
        indexedLayer.manifestHash,
        indexedLayer.layerIndex,
        indexedLayer.checksum};
    if (const auto cached = m_cache.Find(key); cached != nullptr)
    {
        CheckControl(control, indexedLayer);
        {
            std::scoped_lock lock{m_mutex};
            if (!m_package.has_value()
                || m_package->packageIdentity
                    != indexedLayer.packageIdentity
                || m_package->manifestHash
                    != indexedLayer.manifestHash)
            {
                Fail(
                    TiffLayerErrorCode::StaleResult,
                    "indexed package changed before cache delivery",
                    indexedLayer.packageIdentity,
                    indexedLayer.layerIndex,
                    indexedLayer.path);
            }
        }
        return TiffLayerLoadResult{
            cached,
            control.requestGeneration,
            true};
    }

    TiffReadResult decoded;
    try
    {
        decoded = read_rgbwsv_tiff(indexedLayer.path);
    }
    catch (const std::exception& error)
    {
        Fail(
            TiffLayerErrorCode::ReadFailed,
            "RGBWSV TIFF decoding failed",
            indexedLayer.packageIdentity,
            indexedLayer.layerIndex,
            indexedLayer.path,
            error.what());
    }
    CheckControl(control, indexedLayer);

    if (decoded.spec.width != indexedLayer.width
        || decoded.spec.height != indexedLayer.height)
    {
        Fail(
            TiffLayerErrorCode::DimensionMismatch,
            "decoded TIFF dimensions do not match manifest",
            indexedLayer.packageIdentity,
            indexedLayer.layerIndex,
            indexedLayer.path);
    }
    if (decoded.spec.samples_per_pixel != rgbwsv_channel_count
        || decoded.spec.bits_per_sample != 8
        || decoded.spec.planar_config != 1
        || decoded.spec.storage_mode != indexedLayer.storage
        || decoded.spec.compression_mode != indexedLayer.compression)
    {
        Fail(
            TiffLayerErrorCode::ProtocolMismatch,
            "decoded TIFF protocol does not match manifest",
            indexedLayer.packageIdentity,
            indexedLayer.layerIndex,
            indexedLayer.path);
    }

    auto buffer = std::make_shared<RgbwsvLayerBuffer>();
    buffer->sourceIdentity = indexedLayer.sourceIdentity;
    buffer->layerIndex = indexedLayer.layerIndex;
    buffer->zMm = indexedLayer.zMm;
    buffer->width = indexedLayer.width;
    buffer->height = indexedLayer.height;
    buffer->dpiX = indexedLayer.dpiX;
    buffer->dpiY = indexedLayer.dpiY;
    buffer->originxmm = indexedLayer.originxmm;
    buffer->originymm = indexedLayer.originymm;
    buffer->originzmm = indexedLayer.originzmm;
    buffer->pixelsizexmm = indexedLayer.pixelsizexmm;
    buffer->pixelsizeymm = indexedLayer.pixelsizeymm;
    buffer->layerthicknessmm =
        indexedLayer.layerthicknessmm;
    buffer->sceneidentityavailable =
        indexedLayer.sceneidentityavailable;
    buffer->sceneid = indexedLayer.sceneid;
    buffer->scenerevision =
        indexedLayer.scenerevision;
    buffer->pixels = std::move(decoded.pixels);
    buffer->channelStats = decoded.channel_stats;
    buffer->channelChecksums = decoded.channel_checksums;
    buffer->decodedBytes = buffer->pixels.size();
    CheckControl(control, indexedLayer);
    {
        std::scoped_lock lock{m_mutex};
        if (!m_package.has_value()
            || m_package->packageIdentity
                != indexedLayer.packageIdentity
            || m_package->manifestHash
                != indexedLayer.manifestHash)
        {
            Fail(
                TiffLayerErrorCode::StaleResult,
                "indexed package changed before decoded layer delivery",
                indexedLayer.packageIdentity,
                indexedLayer.layerIndex,
                indexedLayer.path);
        }
    }
    static_cast<void>(m_cache.Put(key, buffer));
    return TiffLayerLoadResult{
        std::move(buffer),
        control.requestGeneration,
        false};
}

void TiffLayerSource::ClearPackage(
    const std::string& packageIdentity)
{
    {
        std::scoped_lock lock{m_mutex};
        if (m_package.has_value()
            && m_package->packageIdentity == packageIdentity)
        {
            m_package.reset();
        }
    }
    m_cache.ClearPackage(packageIdentity);
}

TiffLayerCacheStats TiffLayerSource::CacheStats() const
{
    return m_cache.Stats();
}

}  // namespace slicer_core
