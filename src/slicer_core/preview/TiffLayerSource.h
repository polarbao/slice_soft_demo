#pragma once

#include "slicer_core/preview/ProductionLayerRef.h"
#include "slicer_core/preview/TiffLayerCache.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

namespace slicer_core
{

/**
 * @brief Stable TIFF-native layer loading error categories.
 */
enum class TiffLayerErrorCode
{
    PackageNotFound,
    ManifestInvalid,
    LayerNotListed,
    PathEscape,
    FileMissing,
    ProtocolMismatch,
    DimensionMismatch,
    ReadFailed,
    Cancelled,
    StaleResult,
};

/**
 * @brief Convert a TIFF layer error category to its stable protocol string.
 * @param code Error category.
 * @return Stable TIFF_LAYER_* error string.
 */
std::string TiffLayerErrorCodeString(TiffLayerErrorCode code);

/**
 * @brief Structured failure raised by the TIFF-native production layer source.
 */
class TiffLayerError final : public std::runtime_error
{
public:
    /**
     * @brief Construct a structured layer-loading failure.
     * @param code Stable error category.
     * @param message Technical diagnostic message.
     * @param packageIdentity Package identity, when known.
     * @param layerIndex Layer index, or -1 when not applicable.
     * @param path Related manifest or TIFF path.
     * @param sourceCode Underlying reader or filesystem error.
     */
    TiffLayerError(
        TiffLayerErrorCode code,
        std::string message,
        std::string packageIdentity = {},
        int layerIndex = -1,
        std::filesystem::path path = {},
        std::string sourceCode = {});

    /**
     * @brief Return the stable error category.
     * @return Error category.
     */
    TiffLayerErrorCode Code() const noexcept;

    /**
     * @brief Return the package identity associated with the failure.
     * @return Package identity, possibly empty.
     */
    const std::string& PackageIdentity() const noexcept;

    /**
     * @brief Return the affected manifest layer index.
     * @return Layer index, or -1 when not applicable.
     */
    int LayerIndex() const noexcept;

    /**
     * @brief Return the related manifest or TIFF path.
     * @return Related path, possibly empty.
     */
    const std::filesystem::path& Path() const noexcept;

    /**
     * @brief Return the underlying reader or filesystem error.
     * @return Source error string, possibly empty.
     */
    const std::string& SourceCode() const noexcept;

private:
    TiffLayerErrorCode m_code;
    std::string m_packageIdentity;
    int m_layerIndex{-1};
    std::filesystem::path m_path;
    std::string m_sourceCode;
};

/**
 * @brief Cooperative cancellation and generation checks for one layer load.
 */
struct TiffLayerLoadControl
{
    std::uint64_t requestGeneration{0U};
    std::function<bool()> cancellationRequested;
    std::function<bool(std::uint64_t)> generationCurrent;
};

/**
 * @brief Result of one TIFF-native production layer request.
 */
struct TiffLayerLoadResult
{
    std::shared_ptr<const RgbwsvLayerBuffer> buffer;
    std::uint64_t requestGeneration{0U};
    bool cacheHit{false};
};

/**
 * @brief Manifest-authoritative, cache-backed RGBWSV TIFF layer source.
 */
class TiffLayerSource final
{
public:
    /**
     * @brief Construct a layer source with default cache limits.
     */
    TiffLayerSource();

    /**
     * @brief Construct a layer source with explicit cache limits.
     * @param cacheLimits Decoded layer LRU limits.
     */
    explicit TiffLayerSource(TiffLayerCacheLimits cacheLimits);

    /**
     * @brief Validate and index one current-protocol production manifest.
     * @param manifestPath Path to package manifest.json.
     * @return Immutable package and layer metadata snapshot.
     */
    ProductionPackageIndex IndexPackage(
        const std::filesystem::path& manifestPath);

    /**
     * @brief Find one exact manifest-listed layer in the current package.
     * @param layerIndex Real manifest layer index.
     * @return Layer reference, or empty when not listed.
     */
    std::optional<ProductionLayerRef> FindLayer(int layerIndex) const;

    /**
     * @brief Decode or retrieve one exact manifest-listed production layer.
     * @param layer Manifest-derived immutable layer reference.
     * @param control Cooperative cancellation and generation checks.
     * @return Immutable RGBWSV buffer and cache-hit metadata.
     */
    TiffLayerLoadResult LoadLayer(
        const ProductionLayerRef& layer,
        const TiffLayerLoadControl& control = {});

    /**
     * @brief Invalidate the package index and decoded cache entries.
     * @param packageIdentity Package identity to clear.
     */
    void ClearPackage(const std::string& packageIdentity);

    /**
     * @brief Return the current decoded layer cache statistics.
     * @return Cache occupancy and cumulative counters.
     */
    TiffLayerCacheStats CacheStats() const;

private:
    mutable std::mutex m_mutex;
    std::optional<ProductionPackageIndex> m_package;
    TiffLayerCache m_cache;
};

}  // namespace slicer_core
