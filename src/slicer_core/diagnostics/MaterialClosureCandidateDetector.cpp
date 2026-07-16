#include "slicer_core/diagnostics/MaterialClosureCandidateDetector.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <vector>

namespace slicer_core
{
namespace
{

constexpr std::size_t channelCount{6U};
constexpr std::uint8_t emptyValue{255U};

enum MaterialFlags : std::uint8_t
{
    MaterialNone = 0U,
    MaterialColor = 1U << 0U,
    MaterialFill = 1U << 1U,
    MaterialSupport = 1U << 2U,
    MaterialModel = 1U << 3U,
};

struct Direction
{
    int dx{0};
    int dy{0};
};

constexpr std::array<Direction, 4> floodDirections4{{
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1},
}};

constexpr std::array<Direction, 8> floodDirections8{{
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1},
    {1, 1},
    {-1, -1},
    {1, -1},
    {-1, 1},
}};

constexpr std::array<Direction, 2> opposingDirections4{{
    {1, 0},
    {0, 1},
}};

constexpr std::array<Direction, 4> opposingDirections8{{
    {1, 0},
    {0, 1},
    {1, 1},
    {1, -1},
}};

std::size_t PixelIndex(const int widthPx, const int x, const int y)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(widthPx)
        + static_cast<std::size_t>(x);
}

bool IsInside(const int widthPx, const int heightPx, const int x, const int y)
{
    return x >= 0 && x < widthPx && y >= 0 && y < heightPx;
}

std::vector<std::uint8_t> BuildMaterialFlags(
    const std::vector<std::uint8_t>& rgbwsvPixels,
    const int widthPx,
    const int heightPx)
{
    const std::size_t pixelCount = static_cast<std::size_t>(widthPx)
        * static_cast<std::size_t>(heightPx);
    std::vector<std::uint8_t> flags(pixelCount, MaterialNone);

    for (std::size_t pixelIndex{0U}; pixelIndex < pixelCount; ++pixelIndex)
    {
        const std::size_t base = pixelIndex * channelCount;
        const bool color = rgbwsvPixels.at(base + 0U) < emptyValue
            || rgbwsvPixels.at(base + 1U) < emptyValue
            || rgbwsvPixels.at(base + 2U) < emptyValue;
        const bool fill = rgbwsvPixels.at(base + 3U) < emptyValue
            || rgbwsvPixels.at(base + 5U) < emptyValue;
        const bool support = rgbwsvPixels.at(base + 4U) < emptyValue;

        std::uint8_t pixelFlags{MaterialNone};
        if (color)
        {
            pixelFlags |= MaterialColor;
        }
        if (fill)
        {
            pixelFlags |= MaterialFill;
        }
        if (support)
        {
            pixelFlags |= MaterialSupport;
        }
        if (color || fill)
        {
            pixelFlags |= MaterialModel;
        }
        flags.at(pixelIndex) = pixelFlags;
    }
    return flags;
}

template <std::size_t DirectionCount>
int CountExternalBackground(
    const std::vector<std::uint8_t>& materialFlags,
    const int widthPx,
    const int heightPx,
    const std::array<Direction, DirectionCount>& directions)
{
    std::vector<std::uint8_t> visited(materialFlags.size(), 0U);
    std::deque<std::size_t> pending;

    const auto enqueue = [&](const int x, const int y)
    {
        const std::size_t index = PixelIndex(widthPx, x, y);
        if (materialFlags.at(index) == MaterialNone && visited.at(index) == 0U)
        {
            visited.at(index) = 1U;
            pending.push_back(index);
        }
    };

    for (int x{0}; x < widthPx; ++x)
    {
        enqueue(x, 0);
        enqueue(x, heightPx - 1);
    }
    for (int y{0}; y < heightPx; ++y)
    {
        enqueue(0, y);
        enqueue(widthPx - 1, y);
    }

    int protectedPixels{0};
    while (!pending.empty())
    {
        const std::size_t index = pending.front();
        pending.pop_front();
        ++protectedPixels;

        const int x = static_cast<int>(index % static_cast<std::size_t>(widthPx));
        const int y = static_cast<int>(index / static_cast<std::size_t>(widthPx));
        for (const Direction& direction : directions)
        {
            const int nextX = x + direction.dx;
            const int nextY = y + direction.dy;
            if (IsInside(widthPx, heightPx, nextX, nextY))
            {
                enqueue(nextX, nextY);
            }
        }
    }
    return protectedPixels;
}

std::uint8_t FindFirstOccupiedFlags(
    const std::vector<std::uint8_t>& materialFlags,
    const int widthPx,
    const int heightPx,
    const int startX,
    const int startY,
    const Direction direction,
    const int maxGapPx)
{
    for (int distance{1}; distance <= maxGapPx; ++distance)
    {
        const int x = startX + direction.dx * distance;
        const int y = startY + direction.dy * distance;
        if (!IsInside(widthPx, heightPx, x, y))
        {
            return MaterialNone;
        }

        const std::uint8_t flags = materialFlags.at(PixelIndex(widthPx, x, y));
        if (flags != MaterialNone)
        {
            return flags;
        }
    }
    return MaterialNone;
}

template <std::size_t DirectionCount>
bool HasOpposingMaterials(
    const std::vector<std::uint8_t>& materialFlags,
    const int widthPx,
    const int heightPx,
    const int x,
    const int y,
    const std::uint8_t firstMaterial,
    const std::uint8_t secondMaterial,
    const int maxGapPx,
    const std::array<Direction, DirectionCount>& directions)
{
    for (const Direction& direction : directions)
    {
        const Direction opposite{-direction.dx, -direction.dy};
        const std::uint8_t forward = FindFirstOccupiedFlags(
            materialFlags,
            widthPx,
            heightPx,
            x,
            y,
            direction,
            maxGapPx);
        const std::uint8_t backward = FindFirstOccupiedFlags(
            materialFlags,
            widthPx,
            heightPx,
            x,
            y,
            opposite,
            maxGapPx);
        const bool ordered = (forward & firstMaterial) != 0U
            && (backward & secondMaterial) != 0U;
        const bool reversed = (forward & secondMaterial) != 0U
            && (backward & firstMaterial) != 0U;
        if (ordered || reversed)
        {
            return true;
        }
    }
    return false;
}

template <std::size_t DirectionCount>
void DetectCandidateGaps(
    const std::vector<std::uint8_t>& materialFlags,
    const int widthPx,
    const int heightPx,
    const int maxGapPx,
    const std::array<Direction, DirectionCount>& directions,
    MaterialClosureCandidateLayer& result)
{
    for (int y{0}; y < heightPx; ++y)
    {
        for (int x{0}; x < widthPx; ++x)
        {
            if (materialFlags.at(PixelIndex(widthPx, x, y)) != MaterialNone)
            {
                continue;
            }

            const bool colorFill = HasOpposingMaterials(
                materialFlags,
                widthPx,
                heightPx,
                x,
                y,
                MaterialColor,
                MaterialFill,
                maxGapPx,
                directions);
            const bool modelSupport = HasOpposingMaterials(
                materialFlags,
                widthPx,
                heightPx,
                x,
                y,
                MaterialModel,
                MaterialSupport,
                maxGapPx,
                directions);
            const bool colorSupport = HasOpposingMaterials(
                materialFlags,
                widthPx,
                heightPx,
                x,
                y,
                MaterialColor,
                MaterialSupport,
                maxGapPx,
                directions);

            result.colorFillGapPixels += colorFill ? 1 : 0;
            result.modelSupportGapPixels += modelSupport ? 1 : 0;
            result.colorSupportGapPixels += colorSupport ? 1 : 0;
            result.gapPixels += (colorFill || modelSupport || colorSupport) ? 1 : 0;
        }
    }
}

}  // namespace

MaterialClosureCandidateLayer DetectMaterialClosureCandidateLayer(
    const std::vector<std::uint8_t>& rgbwsvPixels,
    const int widthPx,
    const int heightPx,
    const int layerIndex,
    const double zMm,
    const int connectivity,
    const int maxGapPx)
{
    if (widthPx <= 0 || heightPx <= 0)
    {
        throw std::invalid_argument("material closure candidate dimensions must be positive");
    }
    if (layerIndex < 0)
    {
        throw std::invalid_argument("material closure candidate layer index must not be negative");
    }
    if (connectivity != 4 && connectivity != 8)
    {
        throw std::invalid_argument("material closure candidate connectivity must be 4 or 8");
    }
    if (maxGapPx <= 0)
    {
        throw std::invalid_argument("material closure candidate max gap must be positive");
    }

    const std::size_t expectedSize = static_cast<std::size_t>(widthPx)
        * static_cast<std::size_t>(heightPx) * channelCount;
    if (rgbwsvPixels.size() != expectedSize)
    {
        throw std::invalid_argument("material closure candidate RGBWSV buffer size mismatch");
    }

    MaterialClosureCandidateLayer result;
    result.layerIndex = layerIndex;
    result.zMm = zMm;

    const std::vector<std::uint8_t> materialFlags = BuildMaterialFlags(
        rgbwsvPixels,
        widthPx,
        heightPx);
    if (connectivity == 8)
    {
        result.externalBackgroundProtectedPixels = CountExternalBackground(
            materialFlags,
            widthPx,
            heightPx,
            floodDirections8);
        DetectCandidateGaps(
            materialFlags,
            widthPx,
            heightPx,
            maxGapPx,
            opposingDirections8,
            result);
    }
    else
    {
        result.externalBackgroundProtectedPixels = CountExternalBackground(
            materialFlags,
            widthPx,
            heightPx,
            floodDirections4);
        DetectCandidateGaps(
            materialFlags,
            widthPx,
            heightPx,
            maxGapPx,
            opposingDirections4,
            result);
    }
    return result;
}

}  // namespace slicer_core
