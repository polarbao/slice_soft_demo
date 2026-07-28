#pragma once

#include "slicer_core/preview/ProductionLayerRef.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace slicer_core
{

/**
 * @brief Stable identity used to cache one decoded production TIFF layer.
 */
struct TiffLayerCacheKey
{
    std::string packageIdentity;
    std::string manifestHash;
    int layerIndex{-1};
    std::string checksum;

    bool operator==(const TiffLayerCacheKey& other) const = default;
};

/**
 * @brief Layer-count and byte limits for the decoded TIFF LRU.
 */
struct TiffLayerCacheLimits
{
    std::size_t maxLayers{5U};
    std::size_t maxBytes{256U * 1024U * 1024U};
};

/**
 * @brief Runtime counters for the decoded TIFF LRU.
 */
struct TiffLayerCacheStats
{
    std::size_t layerCount{0U};
    std::size_t currentBytes{0U};
    std::uint64_t hits{0U};
    std::uint64_t misses{0U};
    std::uint64_t evictions{0U};
    std::uint64_t skippedOversized{0U};
};

/**
 * @brief Thread-safe bounded LRU for immutable decoded RGBWSV layer buffers.
 */
class TiffLayerCache final
{
public:
    /**
     * @brief Construct a cache with the default five-layer, 256 MiB limits.
     */
    TiffLayerCache();

    /**
     * @brief Construct a cache with explicit limits.
     * @param limits Layer-count and decoded-byte limits.
     */
    explicit TiffLayerCache(TiffLayerCacheLimits limits);

    /**
     * @brief Find a decoded layer and promote it to most recently used.
     * @param key Stable package, manifest, layer, and file identity.
     * @return Shared immutable buffer, or null on cache miss.
     */
    std::shared_ptr<const RgbwsvLayerBuffer> Find(
        const TiffLayerCacheKey& key);

    /**
     * @brief Insert a decoded layer and evict old entries as required.
     * @param key Stable package, manifest, layer, and file identity.
     * @param buffer Immutable decoded layer.
     * @return True when the layer entered the cache; false when over-sized.
     */
    bool Put(
        const TiffLayerCacheKey& key,
        std::shared_ptr<const RgbwsvLayerBuffer> buffer);

    /**
     * @brief Remove every entry belonging to one package identity.
     * @param packageIdentity Package identity to invalidate.
     */
    void ClearPackage(const std::string& packageIdentity);

    /**
     * @brief Remove all cached entries while retaining cumulative counters.
     */
    void Clear();

    /**
     * @brief Return a consistent cache statistics snapshot.
     * @return Current occupancy and cumulative counters.
     */
    TiffLayerCacheStats Stats() const;

private:
    struct KeyHash
    {
        std::size_t operator()(const TiffLayerCacheKey& key) const noexcept;
    };

    struct Entry
    {
        std::shared_ptr<const RgbwsvLayerBuffer> buffer;
        std::size_t bytes{0U};
        std::list<TiffLayerCacheKey>::iterator lruPosition;
    };

    using EntryMap =
        std::unordered_map<TiffLayerCacheKey, Entry, KeyHash>;

    void Remove(EntryMap::iterator iterator);
    void EnforceLimits();

    TiffLayerCacheLimits m_limits;
    mutable std::mutex m_mutex;
    std::list<TiffLayerCacheKey> m_lru;
    EntryMap m_entries;
    std::size_t m_currentBytes{0U};
    std::uint64_t m_hits{0U};
    std::uint64_t m_misses{0U};
    std::uint64_t m_evictions{0U};
    std::uint64_t m_skippedOversized{0U};
};

}  // namespace slicer_core
