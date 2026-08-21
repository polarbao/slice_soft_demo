#pragma once

#include <cstdint>

namespace slicer_core
{

/** @brief Stable internal classification for one generated support pixel. */
enum class SupportType : std::uint8_t
{
    None = 0,
    BottomProjection = 1,
    UnsupportedIsland = 2,
    FullVerticalProjection = 3,
    InternalVoid = 4,
    UpperProjection = 5,
    ProjectionBase = 6,
};

/**
 * @brief Return the semantic overwrite priority for a support type.
 *
 * Enum values intentionally do not encode priority. Existing retained support
 * generation and bounded replay must use this function when types collide.
 */
[[nodiscard]] constexpr int SupportTypePriority(
    const SupportType type) noexcept
{
    switch (type)
    {
        case SupportType::InternalVoid:
            return 6;
        case SupportType::UnsupportedIsland:
            return 5;
        case SupportType::FullVerticalProjection:
            return 4;
        case SupportType::UpperProjection:
            return 3;
        case SupportType::BottomProjection:
            return 2;
        case SupportType::ProjectionBase:
            return 1;
        case SupportType::None:
            return 0;
    }
    return 0;
}

}  // namespace slicer_core
