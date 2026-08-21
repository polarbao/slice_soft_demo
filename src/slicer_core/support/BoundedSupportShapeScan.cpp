#include "slicer_core/support/BoundedSupportShapeScan.h"

#include "slicer_core/system/Sha256Internal.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr std::string_view kReplayDigestDomain{
    "mf03b3.shaped-support.v1"};

[[nodiscard]] std::size_t CheckedPixelCount(
    const int widthPx,
    const int heightPx)
{
    if (widthPx <= 0 || heightPx <= 0)
    {
        throw std::invalid_argument(
            "bounded support shape dimensions must be positive");
    }
    const std::size_t width = static_cast<std::size_t>(widthPx);
    const std::size_t height = static_cast<std::size_t>(heightPx);
    if (width > std::numeric_limits<std::size_t>::max() / height)
    {
        throw std::overflow_error(
            "bounded support shape pixel count overflow");
    }
    const std::size_t pixelCount = width * height;
    if (pixelCount
        > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::invalid_argument(
            "bounded support shape raster exceeds index capacity");
    }
    return pixelCount;
}

template <typename Left, typename Right>
[[nodiscard]] bool ByteRangesOverlap(
    const std::span<Left> left,
    const std::span<Right> right) noexcept
{
    if (left.empty() || right.empty())
    {
        return false;
    }
    const auto leftBegin = reinterpret_cast<std::uintptr_t>(left.data());
    const auto rightBegin = reinterpret_cast<std::uintptr_t>(right.data());
    const std::size_t leftBytes = left.size_bytes();
    const std::size_t rightBytes = right.size_bytes();
    if (leftBytes > std::numeric_limits<std::uintptr_t>::max() - leftBegin
        || rightBytes
            > std::numeric_limits<std::uintptr_t>::max() - rightBegin)
    {
        return true;
    }
    const std::uintptr_t leftEnd = leftBegin + leftBytes;
    const std::uintptr_t rightEnd = rightBegin + rightBytes;
    return leftBegin < rightEnd && rightBegin < leftEnd;
}

void ValidateBinaryMask(
    const std::span<const std::uint8_t> mask,
    const std::string_view name)
{
    if (std::any_of(mask.begin(), mask.end(), [](const std::uint8_t value)
        {
            return value > 1U;
        }))
    {
        throw std::invalid_argument(
            std::string(name) + " must be binary");
    }
}

void ValidateLayerArguments(
    const int expectedLayerIndex,
    const int layerIndex,
    const std::size_t pixelCount,
    const std::span<const std::uint8_t> modelMask,
    const std::span<const std::uint8_t> upperBoundaryMask,
    const std::span<std::uint8_t> outputSupportMask,
    const std::span<SupportType> outputTypeMap)
{
    if (layerIndex != expectedLayerIndex)
    {
        throw std::invalid_argument(
            "bounded support shape layers must be consumed in order");
    }
    if (modelMask.size() != pixelCount
        || upperBoundaryMask.size() != pixelCount
        || outputSupportMask.size() != pixelCount
        || outputTypeMap.size() != pixelCount)
    {
        throw std::invalid_argument(
            "bounded support shape layer dimensions do not match");
    }
    ValidateBinaryMask(modelMask, "modelMask");
    ValidateBinaryMask(upperBoundaryMask, "upperBoundaryMask");
    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        if (modelMask[index] != 0U && upperBoundaryMask[index] == 0U)
        {
            throw std::invalid_argument(
                "upperBoundaryMask must contain modelMask");
        }
    }
    if (ByteRangesOverlap(modelMask, outputSupportMask)
        || ByteRangesOverlap(modelMask, outputTypeMap)
        || ByteRangesOverlap(upperBoundaryMask, outputSupportMask)
        || ByteRangesOverlap(upperBoundaryMask, outputTypeMap)
        || ByteRangesOverlap(outputSupportMask, outputTypeMap))
    {
        throw std::invalid_argument(
            "bounded support shape output buffers must not alias inputs or each other");
    }
}

[[nodiscard]] std::size_t MaskIndex(
    const int width,
    const int x,
    const int y) noexcept
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

void SetSupportPixel(
    std::vector<std::uint8_t>& supportMask,
    std::vector<SupportType>& typeMap,
    const std::size_t index,
    const SupportType type)
{
    supportMask[index] = 1U;
    if (SupportTypePriority(type) >= SupportTypePriority(typeMap[index]))
    {
        typeMap[index] = type;
    }
}

void AddInternalVoidSupport(
    const BoundedSupportShapeScanRequest& request,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<SupportType>& typeMap,
    std::vector<std::uint8_t>& externalEmpty,
    std::vector<std::uint8_t>& visited,
    std::vector<int>& stack,
    std::vector<int>& componentPixels)
{
    if (!request.internalVoid.enabled)
    {
        return;
    }

    std::fill(
        externalEmpty.begin(),
        externalEmpty.end(),
        static_cast<std::uint8_t>(0U));
    std::fill(
        visited.begin(),
        visited.end(),
        static_cast<std::uint8_t>(0U));
    stack.clear();
    componentPixels.clear();

    const auto pushExternal = [&](const int x, const int y)
    {
        if (x < 0 || x >= request.widthPx
            || y < 0 || y >= request.heightPx)
        {
            return;
        }
        const std::size_t index = MaskIndex(request.widthPx, x, y);
        if (modelMask[index] != 0U || externalEmpty[index] != 0U)
        {
            return;
        }
        externalEmpty[index] = 1U;
        stack.push_back(static_cast<int>(index));
    };

    for (int x{0}; x < request.widthPx; ++x)
    {
        pushExternal(x, 0);
        pushExternal(x, request.heightPx - 1);
    }
    for (int y{0}; y < request.heightPx; ++y)
    {
        pushExternal(0, y);
        pushExternal(request.widthPx - 1, y);
    }

    constexpr std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}},
        {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    constexpr std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};

    while (!stack.empty())
    {
        const int current = stack.back();
        stack.pop_back();
        const int x = current % request.widthPx;
        const int y = current / request.widthPx;
        if (request.connectivity == 8)
        {
            for (const auto& neighbor : neighbors8)
            {
                pushExternal(x + neighbor[0], y + neighbor[1]);
            }
        }
        else
        {
            for (const auto& neighbor : neighbors4)
            {
                pushExternal(x + neighbor[0], y + neighbor[1]);
            }
        }
    }

    for (std::size_t start{0U}; start < modelMask.size(); ++start)
    {
        if (modelMask[start] != 0U
            || externalEmpty[start] != 0U
            || visited[start] != 0U)
        {
            continue;
        }

        componentPixels.clear();
        stack.push_back(static_cast<int>(start));
        visited[start] = 1U;
        while (!stack.empty())
        {
            const int current = stack.back();
            stack.pop_back();
            componentPixels.push_back(current);
            const int x = current % request.widthPx;
            const int y = current / request.widthPx;
            const auto pushComponent = [&](const int nx, const int ny)
            {
                if (nx < 0 || nx >= request.widthPx
                    || ny < 0 || ny >= request.heightPx)
                {
                    return;
                }
                const std::size_t next = MaskIndex(request.widthPx, nx, ny);
                if (modelMask[next] == 0U
                    && externalEmpty[next] == 0U
                    && visited[next] == 0U)
                {
                    visited[next] = 1U;
                    stack.push_back(static_cast<int>(next));
                }
            };
            if (request.connectivity == 8)
            {
                for (const auto& neighbor : neighbors8)
                {
                    pushComponent(x + neighbor[0], y + neighbor[1]);
                }
            }
            else
            {
                for (const auto& neighbor : neighbors4)
                {
                    pushComponent(x + neighbor[0], y + neighbor[1]);
                }
            }
        }

        if (static_cast<int>(componentPixels.size())
            < request.internalVoid.min_area_px)
        {
            continue;
        }
        for (const int pixel : componentPixels)
        {
            SetSupportPixel(
                supportMask,
                typeMap,
                static_cast<std::size_t>(pixel),
                SupportType::InternalVoid);
        }
    }
}

void SynchronizeShapeTypes(
    const std::vector<std::uint8_t>& originalSupportMask,
    const std::vector<std::uint8_t>& optimizedSupportMask,
    std::vector<SupportType>& typeMap)
{
    for (std::size_t index{0U}; index < optimizedSupportMask.size(); ++index)
    {
        if (optimizedSupportMask[index] == 0U)
        {
            typeMap[index] = SupportType::None;
        }
        else if (originalSupportMask[index] == 0U
                 && typeMap[index] == SupportType::None)
        {
            typeMap[index] = SupportType::BottomProjection;
        }
    }
}

void AnalyzeComponentsBounded(
    const BoundedSupportShapeScanRequest& request,
    const int layerIndex,
    std::vector<std::uint8_t>& supportMask,
    const int filterArea,
    BoundedSupportComponentAnalysis& analysis,
    std::vector<BoundedFilteredSupportComponent>* const filtered,
    std::vector<std::uint8_t>& visited,
    std::vector<int>& stack,
    std::vector<int>& componentPixels)
{
    analysis = BoundedSupportComponentAnalysis{};
    analysis.enabled = true;
    std::fill(visited.begin(), visited.end(), static_cast<std::uint8_t>(0U));
    constexpr std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}},
        {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    constexpr std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};
    for (std::size_t start{0U}; start < supportMask.size(); ++start)
    {
        if (supportMask[start] == 0U || visited[start] != 0U)
        {
            continue;
        }
        stack.clear();
        componentPixels.clear();
        stack.push_back(static_cast<int>(start));
        visited[start] = 1U;
        int minX{request.widthPx};
        int minY{request.heightPx};
        int maxX{-1};
        int maxY{-1};
        while (!stack.empty())
        {
            const int current{stack.back()};
            stack.pop_back();
            componentPixels.push_back(current);
            const int x{current % request.widthPx};
            const int y{current / request.widthPx};
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
            const auto visit = [&](const int nx, const int ny)
            {
                if (nx < 0 || nx >= request.widthPx
                    || ny < 0 || ny >= request.heightPx)
                {
                    return;
                }
                const std::size_t next{MaskIndex(request.widthPx, nx, ny)};
                if (supportMask[next] != 0U && visited[next] == 0U)
                {
                    visited[next] = 1U;
                    stack.push_back(static_cast<int>(next));
                }
            };
            if (request.connectivity == 8)
            {
                for (const auto& delta : neighbors8)
                {
                    visit(x + delta[0], y + delta[1]);
                }
            }
            else
            {
                for (const auto& delta : neighbors4)
                {
                    visit(x + delta[0], y + delta[1]);
                }
            }
        }
        const int area{static_cast<int>(componentPixels.size())};
        ++analysis.componentCount;
        analysis.largestComponentArea = std::max(
            analysis.largestComponentArea,
            area);
        if (area <= analysis.tinyComponentAreaPx)
        {
            ++analysis.tinyComponentCount;
        }
        else if (area <= analysis.smallComponentAreaPx)
        {
            ++analysis.smallComponentCount;
        }
        analysis.components.push_back(
            BoundedSupportComponentSummary{area, minX, minY, maxX, maxY});
        if (filtered != nullptr && filterArea > 0 && area < filterArea)
        {
            for (const int pixel : componentPixels)
            {
                supportMask[static_cast<std::size_t>(pixel)] = 0U;
            }
            filtered->push_back(BoundedFilteredSupportComponent{
                layerIndex, area, minX, minY, maxX, maxY});
        }
    }
}

void AddShapePixel(
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<std::uint8_t>& addedMask,
    const std::size_t index)
{
    if (modelMask[index] == 0U && supportMask[index] == 0U)
    {
        supportMask[index] = 1U;
        addedMask[index] = 1U;
    }
}

void ApplyBoundedDilation(
    const BoundedSupportShapeScanRequest& request,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<std::uint8_t>& addedMask,
    std::vector<std::uint8_t>& source)
{
    if (request.shape.xy_dilation_px <= 0)
    {
        return;
    }
    std::copy(supportMask.begin(), supportMask.end(), source.begin());
    const int radius{request.shape.xy_dilation_px};
    for (int y{0}; y < request.heightPx; ++y)
    {
        for (int x{0}; x < request.widthPx; ++x)
        {
            if (source[MaskIndex(request.widthPx, x, y)] == 0U)
            {
                continue;
            }
            for (int dy{-radius}; dy <= radius; ++dy)
            {
                for (int dx{-radius}; dx <= radius; ++dx)
                {
                    const int nx{x + dx};
                    const int ny{y + dy};
                    if (nx >= 0 && nx < request.widthPx
                        && ny >= 0 && ny < request.heightPx)
                    {
                        AddShapePixel(
                            modelMask,
                            supportMask,
                            addedMask,
                            MaskIndex(request.widthPx, nx, ny));
                    }
                }
            }
        }
    }
}

void ApplyBoundedClosing(
    const BoundedSupportShapeScanRequest& request,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<std::uint8_t>& addedMask,
    std::vector<std::uint8_t>& source)
{
    if (request.shape.closing_radius_px <= 0)
    {
        return;
    }
    std::copy(supportMask.begin(), supportMask.end(), source.begin());
    const int radius{request.shape.closing_radius_px};
    for (int y{0}; y < request.heightPx; ++y)
    {
        for (int x{0}; x < request.widthPx; ++x)
        {
            const std::size_t index{MaskIndex(request.widthPx, x, y)};
            if (modelMask[index] != 0U || supportMask[index] != 0U)
            {
                continue;
            }
            bool left{false};
            bool right{false};
            bool up{false};
            bool down{false};
            for (int step{1}; step <= radius; ++step)
            {
                left = left || (x - step >= 0
                    && source[MaskIndex(request.widthPx, x - step, y)] != 0U);
                right = right || (x + step < request.widthPx
                    && source[MaskIndex(request.widthPx, x + step, y)] != 0U);
                up = up || (y - step >= 0
                    && source[MaskIndex(request.widthPx, x, y - step)] != 0U);
                down = down || (y + step < request.heightPx
                    && source[MaskIndex(request.widthPx, x, y + step)] != 0U);
            }
            if ((left && right) || (up && down))
            {
                AddShapePixel(modelMask, supportMask, addedMask, index);
            }
        }
    }
}

void ApplyBoundedBridges(
    const BoundedSupportShapeScanRequest& request,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<std::uint8_t>& addedMask,
    BoundedSupportShapeLayerReport& report)
{
    const int maxGap{request.shape.bridge_gap_px};
    if (maxGap <= 0)
    {
        return;
    }
    for (int y{0}; y < request.heightPx; ++y)
    {
        int x{0};
        while (x < request.widthPx)
        {
            if (supportMask[MaskIndex(request.widthPx, x, y)] == 0U)
            {
                ++x;
                continue;
            }
            const int left{x};
            int gapStart{x + 1};
            while (gapStart < request.widthPx
                   && supportMask[MaskIndex(request.widthPx, gapStart, y)] != 0U)
            {
                ++gapStart;
            }
            int gapEnd{gapStart};
            while (gapEnd < request.widthPx
                   && supportMask[MaskIndex(request.widthPx, gapEnd, y)] == 0U)
            {
                ++gapEnd;
            }
            const int gap{gapEnd - gapStart};
            bool allowed{gap > 0 && gap <= maxGap && gapEnd < request.widthPx};
            for (int bx{gapStart}; allowed && bx < gapEnd; ++bx)
            {
                allowed = modelMask[MaskIndex(request.widthPx, bx, y)] == 0U;
            }
            if (allowed)
            {
                for (int bx{gapStart}; bx < gapEnd; ++bx)
                {
                    AddShapePixel(
                        modelMask, supportMask, addedMask,
                        MaskIndex(request.widthPx, bx, y));
                }
                report.bridgedGaps.push_back(BoundedBridgedSupportGap{
                    report.layerIndex, left, y, gapEnd, y, gap, "horizontal"});
            }
            x = std::max(gapEnd, x + 1);
        }
    }
    for (int x{0}; x < request.widthPx; ++x)
    {
        int y{0};
        while (y < request.heightPx)
        {
            if (supportMask[MaskIndex(request.widthPx, x, y)] == 0U)
            {
                ++y;
                continue;
            }
            const int top{y};
            int gapStart{y + 1};
            while (gapStart < request.heightPx
                   && supportMask[MaskIndex(request.widthPx, x, gapStart)] != 0U)
            {
                ++gapStart;
            }
            int gapEnd{gapStart};
            while (gapEnd < request.heightPx
                   && supportMask[MaskIndex(request.widthPx, x, gapEnd)] == 0U)
            {
                ++gapEnd;
            }
            const int gap{gapEnd - gapStart};
            bool allowed{gap > 0 && gap <= maxGap && gapEnd < request.heightPx};
            for (int by{gapStart}; allowed && by < gapEnd; ++by)
            {
                allowed = modelMask[MaskIndex(request.widthPx, x, by)] == 0U;
            }
            if (allowed)
            {
                for (int by{gapStart}; by < gapEnd; ++by)
                {
                    AddShapePixel(
                        modelMask, supportMask, addedMask,
                        MaskIndex(request.widthPx, x, by));
                }
                report.bridgedGaps.push_back(BoundedBridgedSupportGap{
                    report.layerIndex, x, top, x, gapEnd, gap, "vertical"});
            }
            y = std::max(gapEnd, y + 1);
        }
    }
}

[[nodiscard]] bool OptimizeShapeBounded(
    const BoundedSupportShapeScanRequest& request,
    const int layerIndex,
    const std::vector<std::uint8_t>& modelMask,
    const std::vector<std::uint8_t>& originalSupportMask,
    std::vector<std::uint8_t>& supportMask,
    BoundedSupportShapeLayerReport& report,
    std::vector<std::uint8_t>& source,
    std::vector<std::uint8_t>& added,
    std::vector<std::uint8_t>& visited,
    std::vector<int>& stack,
    std::vector<int>& componentPixels)
{
    if (!request.shape.enabled)
    {
        return false;
    }
    report = BoundedSupportShapeLayerReport{};
    report.layerIndex = layerIndex;
    AnalyzeComponentsBounded(
        request, layerIndex, supportMask,
        request.shape.min_component_area_px, report.pre,
        &report.filteredComponents, visited, stack, componentPixels);
    std::fill(added.begin(), added.end(), static_cast<std::uint8_t>(0U));
    ApplyBoundedDilation(request, modelMask, supportMask, added, source);
    ApplyBoundedClosing(request, modelMask, supportMask, added, source);
    ApplyBoundedBridges(request, modelMask, supportMask, added, report);
    const int preSupportPixels{static_cast<int>(std::count_if(
        originalSupportMask.begin(),
        originalSupportMask.end(),
        [](const std::uint8_t value) { return value != 0U; }))};
    for (std::size_t index{0U}; index < supportMask.size(); ++index)
    {
        if (originalSupportMask[index] == 0U && supportMask[index] != 0U)
        {
            ++report.addedSupportPixels;
        }
        if (originalSupportMask[index] != 0U && supportMask[index] == 0U)
        {
            ++report.removedSupportPixels;
        }
        if (request.shape.preserve_model_priority && modelMask[index] != 0U)
        {
            supportMask[index] = 0U;
        }
    }
    const int maxAdded{static_cast<int>(std::floor(
        static_cast<double>(preSupportPixels)
        * request.shape.max_added_support_ratio))};
    if (preSupportPixels > 0 && report.addedSupportPixels > maxAdded)
    {
        for (std::size_t index{0U}; index < supportMask.size(); ++index)
        {
            if (originalSupportMask[index] == 0U && supportMask[index] != 0U)
            {
                supportMask[index] = 0U;
            }
        }
        report.warnings.push_back(
            "added support pixels exceeded maxAddedSupportRatio; additions were reverted");
        report.globalWarnings.push_back(
            "layer " + std::to_string(layerIndex)
            + ": added support pixels exceeded maxAddedSupportRatio");
        report.addedSupportPixels = 0;
    }
    AnalyzeComponentsBounded(
        request, layerIndex, supportMask, 0, report.post, nullptr,
        visited, stack, componentPixels);
    return report.pre.componentCount > 0 || report.post.componentCount > 0
        || report.addedSupportPixels > 0 || report.removedSupportPixels > 0
        || !report.bridgedGaps.empty() || !report.filteredComponents.empty();
}

template <typename Integer>
void HashLittleEndian(detail::Sha256Hasher& hasher, const Integer value)
{
    using Unsigned = std::make_unsigned_t<Integer>;
    Unsigned bits = static_cast<Unsigned>(value);
    std::array<std::uint8_t, sizeof(Integer)> bytes{};
    for (std::size_t index{0U}; index < bytes.size(); ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(bits & 0xffU);
        bits >>= 8U;
    }
    hasher.Update(bytes.data(), bytes.size());
}

void HashBoolean(detail::Sha256Hasher& hasher, const bool value)
{
    const std::uint8_t byte = value ? 1U : 0U;
    hasher.Update(&byte, 1U);
}

void HashString(detail::Sha256Hasher& hasher, const std::string& value)
{
    HashLittleEndian<std::uint64_t>(
        hasher,
        static_cast<std::uint64_t>(value.size()));
    hasher.Update(
        reinterpret_cast<const std::uint8_t*>(value.data()),
        value.size());
}

void HashComponentAnalysis(
    detail::Sha256Hasher& hasher,
    const BoundedSupportComponentAnalysis& analysis)
{
    HashBoolean(hasher, analysis.enabled);
    HashLittleEndian<std::int32_t>(hasher, analysis.componentCount);
    HashLittleEndian<std::int32_t>(hasher, analysis.largestComponentArea);
    HashLittleEndian<std::int32_t>(hasher, analysis.smallComponentCount);
    HashLittleEndian<std::int32_t>(hasher, analysis.tinyComponentCount);
    HashLittleEndian<std::int32_t>(hasher, analysis.tinyComponentAreaPx);
    HashLittleEndian<std::int32_t>(hasher, analysis.smallComponentAreaPx);
    HashLittleEndian<std::uint64_t>(
        hasher,
        static_cast<std::uint64_t>(analysis.components.size()));
    for (const BoundedSupportComponentSummary& component : analysis.components)
    {
        HashLittleEndian<std::int32_t>(hasher, component.areaPx);
        HashLittleEndian<std::int32_t>(hasher, component.minX);
        HashLittleEndian<std::int32_t>(hasher, component.minY);
        HashLittleEndian<std::int32_t>(hasher, component.maxX);
        HashLittleEndian<std::int32_t>(hasher, component.maxY);
    }
}

void HashCompactReport(
    detail::Sha256Hasher& hasher,
    const BoundedSupportShapeLayerReport* const report)
{
    HashBoolean(hasher, report != nullptr);
    if (report == nullptr)
    {
        return;
    }
    HashLittleEndian<std::int32_t>(hasher, report->layerIndex);
    HashComponentAnalysis(hasher, report->pre);
    HashComponentAnalysis(hasher, report->post);
    HashLittleEndian<std::int32_t>(hasher, report->addedSupportPixels);
    HashLittleEndian<std::int32_t>(hasher, report->removedSupportPixels);
    HashLittleEndian<std::uint64_t>(
        hasher,
        static_cast<std::uint64_t>(report->filteredComponents.size()));
    for (const BoundedFilteredSupportComponent& component
         : report->filteredComponents)
    {
        HashLittleEndian<std::int32_t>(hasher, component.layerIndex);
        HashLittleEndian<std::int32_t>(hasher, component.areaPx);
        HashLittleEndian<std::int32_t>(hasher, component.minX);
        HashLittleEndian<std::int32_t>(hasher, component.minY);
        HashLittleEndian<std::int32_t>(hasher, component.maxX);
        HashLittleEndian<std::int32_t>(hasher, component.maxY);
    }
    HashLittleEndian<std::uint64_t>(
        hasher,
        static_cast<std::uint64_t>(report->bridgedGaps.size()));
    for (const BoundedBridgedSupportGap& gap : report->bridgedGaps)
    {
        HashLittleEndian<std::int32_t>(hasher, gap.layerIndex);
        HashLittleEndian<std::int32_t>(hasher, gap.x0);
        HashLittleEndian<std::int32_t>(hasher, gap.y0);
        HashLittleEndian<std::int32_t>(hasher, gap.x1);
        HashLittleEndian<std::int32_t>(hasher, gap.y1);
        HashLittleEndian<std::int32_t>(hasher, gap.gapPx);
        HashString(hasher, gap.direction);
    }
    const auto hashStrings = [&hasher](const std::vector<std::string>& values)
    {
        HashLittleEndian<std::uint64_t>(
            hasher,
            static_cast<std::uint64_t>(values.size()));
        for (const std::string& value : values)
        {
            HashString(hasher, value);
        }
    };
    hashStrings(report->warnings);
    hashStrings(report->globalWarnings);
}

[[nodiscard]] BoundedSupportReplayDigest ComputeReplayDigest(
    const BoundedSupportShapeScanRequest& request,
    const int layerIndex,
    const std::vector<std::uint8_t>& modelMask,
    const std::vector<std::uint8_t>& upperBoundaryMask,
    const std::vector<std::uint8_t>& supportMask,
    const std::vector<SupportType>& typeMap,
    const BoundedSupportShapeLayerReport* const report,
    std::vector<std::uint8_t>& typeBytes)
{
    detail::Sha256Hasher hasher;
    hasher.Update(
        reinterpret_cast<const std::uint8_t*>(kReplayDigestDomain.data()),
        kReplayDigestDomain.size());
    constexpr std::uint8_t separator{0U};
    hasher.Update(&separator, 1U);
    HashLittleEndian<std::int32_t>(hasher, request.widthPx);
    HashLittleEndian<std::int32_t>(hasher, request.heightPx);
    HashLittleEndian<std::int32_t>(hasher, request.layerCount);
    HashLittleEndian<std::int32_t>(hasher, layerIndex);
    HashLittleEndian<std::uint64_t>(
        hasher,
        static_cast<std::uint64_t>(supportMask.size()));
    HashLittleEndian<std::uint8_t>(
        hasher,
        static_cast<std::uint8_t>(request.inputKind));
    HashLittleEndian<std::int32_t>(hasher, request.connectivity);
    HashBoolean(hasher, request.internalVoid.enabled);
    HashLittleEndian<std::int32_t>(
        hasher,
        request.internalVoid.min_area_px);
    HashString(hasher, request.internalVoid.fill_rule);
    HashBoolean(hasher, request.shape.enabled);
    HashLittleEndian<std::int32_t>(
        hasher,
        request.shape.min_component_area_px);
    HashLittleEndian<std::int32_t>(hasher, request.shape.xy_dilation_px);
    HashLittleEndian<std::int32_t>(hasher, request.shape.closing_radius_px);
    HashLittleEndian<std::int32_t>(hasher, request.shape.bridge_gap_px);
    HashBoolean(hasher, request.shape.preserve_model_priority);
    HashLittleEndian<std::uint64_t>(
        hasher,
        std::bit_cast<std::uint64_t>(request.shape.max_added_support_ratio));
    hasher.Update(modelMask.data(), modelMask.size());
    hasher.Update(upperBoundaryMask.data(), upperBoundaryMask.size());
    hasher.Update(supportMask.data(), supportMask.size());
    for (std::size_t index{0U}; index < typeMap.size(); ++index)
    {
        typeBytes[index] = static_cast<std::uint8_t>(typeMap[index]);
    }
    hasher.Update(typeBytes.data(), typeBytes.size());
    HashCompactReport(hasher, report);
    return hasher.Finalize();
}

}  // namespace

BoundedSupportShapeScanner::BoundedSupportShapeScanner(
    const BoundedSupportShapeScanRequest& request,
    const BoundedSupportDemandPlan& finalPlan,
    BoundedSupportShapeReportSink* const reportSink)
    : request_(request),
      finalPlan_(&finalPlan),
      reportSink_(reportSink),
      pixelCount_(CheckedPixelCount(request.widthPx, request.heightPx))
{
    if (request.layerCount <= 0)
    {
        throw std::invalid_argument(
            "bounded support shape layer count must be positive");
    }
    if (request.inputKind
        != GeometryOccupancyInputKind::SingleIntervalHeightfield)
    {
        throw std::invalid_argument(
            "bounded support shape does not support GeneralMesh");
    }
    if (request.connectivity != 4 && request.connectivity != 8)
    {
        throw std::invalid_argument(
            "bounded support shape connectivity must be 4 or 8");
    }
    if (request.internalVoid.min_area_px < 0
        || request.internalVoid.fill_rule != "all_internal_voids")
    {
        throw std::invalid_argument(
            "bounded support shape internal void policy is invalid");
    }
    if (request.shape.min_component_area_px < 0
        || request.shape.xy_dilation_px < 0
        || request.shape.closing_radius_px < 0
        || request.shape.bridge_gap_px < 0
        || !std::isfinite(request.shape.max_added_support_ratio)
        || request.shape.max_added_support_ratio < 0.0)
    {
        throw std::invalid_argument(
            "bounded support shape policy is invalid");
    }
    if (finalPlan.InputKind() != request.inputKind
        || finalPlan.LayerCount() != request.layerCount
        || finalPlan.ColumnCount() != pixelCount_)
    {
        throw std::invalid_argument(
            "bounded support shape requires a matching final support plan");
    }

    supportFootprint_.assign(pixelCount_, 0U);
    replayDigests_.resize(static_cast<std::size_t>(request.layerCount));
    modelScratch_.resize(pixelCount_);
    upperBoundaryScratch_.resize(pixelCount_);
    supportScratch_.resize(pixelCount_);
    originalSupportScratch_.resize(pixelCount_);
    sourceScratch_.resize(pixelCount_);
    addedScratch_.resize(pixelCount_);
    typeScratch_.resize(pixelCount_);
    typeDigestScratch_.resize(pixelCount_);
    externalEmptyScratch_.resize(pixelCount_);
    visitedScratch_.resize(pixelCount_);
    traversalStack_.reserve(pixelCount_);
    componentPixels_.reserve(pixelCount_);
}

void BoundedSupportShapeScanner::ConsumeLayer(
    const int layerIndex,
    const std::span<const std::uint8_t> modelMask,
    const std::span<const std::uint8_t> upperBoundaryMask,
    const std::span<std::uint8_t> outputSupportMask,
    const std::span<SupportType> outputTypeMap)
{
    if (failed_ || finished_)
    {
        throw std::logic_error(
            "bounded support shape scanner is not active");
    }
    ValidateLayerArguments(
        expectedLayerIndex_,
        layerIndex,
        pixelCount_,
        modelMask,
        upperBoundaryMask,
        outputSupportMask,
        outputTypeMap);

    std::copy(modelMask.begin(), modelMask.end(), modelScratch_.begin());
    std::copy(
        upperBoundaryMask.begin(),
        upperBoundaryMask.end(),
        upperBoundaryScratch_.begin());
    MaterializePreShapeSupportLayer(
        *finalPlan_,
        layerIndex,
        modelScratch_,
        upperBoundaryScratch_,
        supportScratch_,
        typeScratch_);
    AddInternalVoidSupport(
        request_,
        modelScratch_,
        supportScratch_,
        typeScratch_,
        externalEmptyScratch_,
        visitedScratch_,
        traversalStack_,
        componentPixels_);

    originalSupportScratch_ = supportScratch_;
    BoundedSupportShapeLayerReport compactReport;
    const bool hasReport{OptimizeShapeBounded(
        request_,
        layerIndex,
        modelScratch_,
        originalSupportScratch_,
        supportScratch_,
        compactReport,
        sourceScratch_,
        addedScratch_,
        visitedScratch_,
        traversalStack_,
        componentPixels_)};
    SynchronizeShapeTypes(
        originalSupportScratch_,
        supportScratch_,
        typeScratch_);
    const BoundedSupportReplayDigest digest = ComputeReplayDigest(
        request_,
        layerIndex,
        modelScratch_,
        upperBoundaryScratch_,
        supportScratch_,
        typeScratch_,
        hasReport ? &compactReport : nullptr,
        typeDigestScratch_);

    if (hasReport && reportSink_ != nullptr)
    {
        try
        {
            reportSink_->Consume(compactReport);
        }
        catch (...)
        {
            failed_ = true;
            throw;
        }
    }

    std::copy(
        supportScratch_.begin(),
        supportScratch_.end(),
        outputSupportMask.begin());
    std::copy(
        typeScratch_.begin(),
        typeScratch_.end(),
        outputTypeMap.begin());
    for (std::size_t index{0U}; index < pixelCount_; ++index)
    {
        if (supportScratch_[index] != 0U
            && supportFootprint_[index] == 0U)
        {
            supportFootprint_[index] = 1U;
            ++footprintPixels_;
        }
    }
    replayDigests_[static_cast<std::size_t>(layerIndex)] = digest;
    if (hasReport)
    {
        ++totals_.reportedLayerCount;
        totals_.addedSupportPixels += static_cast<std::uint64_t>(
            compactReport.addedSupportPixels);
        totals_.removedSupportPixels += static_cast<std::uint64_t>(
            compactReport.removedSupportPixels);
        totals_.filteredComponentCount += compactReport.filteredComponents.size();
        totals_.bridgedGapCount += compactReport.bridgedGaps.size();
        totals_.warningCount += compactReport.globalWarnings.size();
    }
    ++expectedLayerIndex_;
}

BoundedSupportShapeScanResult BoundedSupportShapeScanner::Finish() &&
{
    if (failed_ || finished_)
    {
        throw std::logic_error(
            "bounded support shape scanner is not active");
    }
    if (expectedLayerIndex_ != request_.layerCount)
    {
        throw std::logic_error(
            "bounded support shape scan is incomplete");
    }
    finished_ = true;
    BoundedSupportShapeScanResult result;
    result.supportFootprint_ = std::move(supportFootprint_);
    result.footprintPixels_ = footprintPixels_;
    result.replayDigests_ = std::move(replayDigests_);
    result.totals_ = totals_;
    return result;
}

}  // namespace slicer_core
