#include "slicer_core/preview/TiffLayerCache.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::shared_ptr<const slicer_core::RgbwsvLayerBuffer> MakeBuffer(
    const int layerIndex,
    const std::size_t decodedBytes)
{
    auto buffer = std::make_shared<slicer_core::RgbwsvLayerBuffer>();
    buffer->layerIndex = layerIndex;
    buffer->decodedBytes = decodedBytes;
    buffer->pixels.assign(decodedBytes, static_cast<std::uint8_t>(layerIndex));
    return buffer;
}

slicer_core::TiffLayerCacheKey MakeKey(
    const std::string& packageIdentity,
    const int layerIndex)
{
    return slicer_core::TiffLayerCacheKey{
        packageIdentity,
        "manifest-hash",
        layerIndex,
        "checksum-" + std::to_string(layerIndex)};
}

bool LeastRecentlyUsedEntryIsEvicted()
{
    slicer_core::TiffLayerCache cache(
        slicer_core::TiffLayerCacheLimits{2U, 1024U});
    const auto first = MakeKey("package-a", 1);
    const auto second = MakeKey("package-a", 2);
    const auto third = MakeKey("package-a", 3);

    cache.Put(first, MakeBuffer(1, 24U));
    cache.Put(second, MakeBuffer(2, 24U));
    const auto firstHit = cache.Find(first);
    cache.Put(third, MakeBuffer(3, 24U));

    const auto secondMiss = cache.Find(second);
    const auto firstStillPresent = cache.Find(first);
    const auto thirdPresent = cache.Find(third);
    const slicer_core::TiffLayerCacheStats stats = cache.Stats();

    return ExpectTrue(firstHit != nullptr, "first entry is a cache hit")
        && ExpectTrue(secondMiss == nullptr, "least recently used entry is evicted")
        && ExpectTrue(firstStillPresent != nullptr, "recently used entry remains")
        && ExpectTrue(thirdPresent != nullptr, "new entry remains")
        && ExpectTrue(stats.layerCount == 2U, "cache respects layer limit")
        && ExpectTrue(stats.currentBytes == 48U, "cache tracks decoded bytes")
        && ExpectTrue(stats.evictions == 1U, "cache reports one eviction");
}

bool ByteBudgetAndOversizedLayerAreBounded()
{
    slicer_core::TiffLayerCache cache(
        slicer_core::TiffLayerCacheLimits{5U, 50U});
    const auto first = MakeKey("package-a", 1);
    const auto second = MakeKey("package-a", 2);
    const auto oversized = MakeKey("package-a", 3);

    cache.Put(first, MakeBuffer(1, 30U));
    cache.Put(second, MakeBuffer(2, 30U));
    const bool oversizedCached =
        cache.Put(oversized, MakeBuffer(3, 60U));
    const slicer_core::TiffLayerCacheStats stats = cache.Stats();

    return ExpectTrue(cache.Find(first) == nullptr, "byte budget evicts oldest entry")
        && ExpectTrue(cache.Find(second) != nullptr, "new bounded entry remains")
        && ExpectTrue(!oversizedCached, "oversized layer is returned without caching")
        && ExpectTrue(cache.Find(oversized) == nullptr, "oversized layer is absent")
        && ExpectTrue(stats.layerCount == 1U, "only one bounded layer remains")
        && ExpectTrue(stats.currentBytes == 30U, "byte accounting remains bounded")
        && ExpectTrue(stats.skippedOversized == 1U, "oversized skip is reported");
}

bool PackageClearDoesNotAffectOtherPackages()
{
    slicer_core::TiffLayerCache cache;
    const auto packageA = MakeKey("package-a", 1);
    const auto packageB = MakeKey("package-b", 1);
    cache.Put(packageA, MakeBuffer(1, 24U));
    cache.Put(packageB, MakeBuffer(1, 24U));

    cache.ClearPackage("package-a");

    return ExpectTrue(cache.Find(packageA) == nullptr, "cleared package is absent")
        && ExpectTrue(cache.Find(packageB) != nullptr, "other package remains")
        && ExpectTrue(cache.Stats().layerCount == 1U, "package clear updates count");
}

}  // namespace

int main()
{
    const bool ok = LeastRecentlyUsedEntryIsEvicted()
        && ByteBudgetAndOversizedLayerAreBounded()
        && PackageClearDoesNotAffectOtherPackages();
    if (!ok)
    {
        return 1;
    }

    std::cout << "tiff_layer_cache_unit_tests: PASS\n";
    return 0;
}
