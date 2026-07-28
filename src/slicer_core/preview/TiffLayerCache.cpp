#include "slicer_core/preview/TiffLayerCache.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace slicer_core
{
namespace
{

void CombineHash(std::size_t& seed, const std::size_t value) noexcept
{
    seed ^= value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
}

}  // namespace

TiffLayerCache::TiffLayerCache()
    : TiffLayerCache(TiffLayerCacheLimits{})
{
}

TiffLayerCache::TiffLayerCache(const TiffLayerCacheLimits limits)
    : m_limits(limits)
{
}

std::shared_ptr<const RgbwsvLayerBuffer> TiffLayerCache::Find(
    const TiffLayerCacheKey& key)
{
    std::scoped_lock lock{m_mutex};
    const auto iterator = m_entries.find(key);
    if (iterator == m_entries.end())
    {
        ++m_misses;
        return {};
    }

    m_lru.splice(m_lru.begin(), m_lru, iterator->second.lruPosition);
    iterator->second.lruPosition = m_lru.begin();
    ++m_hits;
    return iterator->second.buffer;
}

bool TiffLayerCache::Put(
    const TiffLayerCacheKey& key,
    std::shared_ptr<const RgbwsvLayerBuffer> buffer)
{
    if (buffer == nullptr)
    {
        return false;
    }

    const std::size_t bytes =
        buffer->decodedBytes == 0U
        ? buffer->pixels.size()
        : buffer->decodedBytes;

    std::scoped_lock lock{m_mutex};
    const auto existing = m_entries.find(key);
    if (existing != m_entries.end())
    {
        Remove(existing);
    }

    if (m_limits.maxLayers == 0U
        || m_limits.maxBytes == 0U
        || bytes > m_limits.maxBytes)
    {
        ++m_skippedOversized;
        return false;
    }

    m_lru.push_front(key);
    Entry entry;
    entry.buffer = std::move(buffer);
    entry.bytes = bytes;
    entry.lruPosition = m_lru.begin();
    m_currentBytes += bytes;
    m_entries.emplace(key, std::move(entry));
    EnforceLimits();
    return m_entries.contains(key);
}

void TiffLayerCache::ClearPackage(const std::string& packageIdentity)
{
    std::scoped_lock lock{m_mutex};
    for (auto iterator = m_entries.begin(); iterator != m_entries.end();)
    {
        if (iterator->first.packageIdentity == packageIdentity)
        {
            const auto current = iterator++;
            Remove(current);
        }
        else
        {
            ++iterator;
        }
    }
}

void TiffLayerCache::Clear()
{
    std::scoped_lock lock{m_mutex};
    m_entries.clear();
    m_lru.clear();
    m_currentBytes = 0U;
}

TiffLayerCacheStats TiffLayerCache::Stats() const
{
    std::scoped_lock lock{m_mutex};
    TiffLayerCacheStats stats;
    stats.layerCount = m_entries.size();
    stats.currentBytes = m_currentBytes;
    stats.hits = m_hits;
    stats.misses = m_misses;
    stats.evictions = m_evictions;
    stats.skippedOversized = m_skippedOversized;
    return stats;
}

std::size_t TiffLayerCache::KeyHash::operator()(
    const TiffLayerCacheKey& key) const noexcept
{
    std::size_t seed{0U};
    CombineHash(seed, std::hash<std::string>{}(key.packageIdentity));
    CombineHash(seed, std::hash<std::string>{}(key.manifestHash));
    CombineHash(seed, std::hash<int>{}(key.layerIndex));
    CombineHash(seed, std::hash<std::string>{}(key.checksum));
    return seed;
}

void TiffLayerCache::Remove(const EntryMap::iterator iterator)
{
    m_currentBytes -= std::min(m_currentBytes, iterator->second.bytes);
    m_lru.erase(iterator->second.lruPosition);
    m_entries.erase(iterator);
}

void TiffLayerCache::EnforceLimits()
{
    while (!m_lru.empty()
           && (m_entries.size() > m_limits.maxLayers
               || m_currentBytes > m_limits.maxBytes))
    {
        const TiffLayerCacheKey key = m_lru.back();
        const auto iterator = m_entries.find(key);
        if (iterator == m_entries.end())
        {
            m_lru.pop_back();
            continue;
        }
        Remove(iterator);
        ++m_evictions;
    }
}

}  // namespace slicer_core
