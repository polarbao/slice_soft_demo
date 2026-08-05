#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace slicer_core::tiff_read_internal
{

inline std::vector<std::uint8_t> DecodePackBitsBlock(
    const std::span<const std::uint8_t> encoded,
    const std::size_t expectedByteCount,
    const std::filesystem::path& path)
{
    std::vector<std::uint8_t> decoded;
    decoded.reserve(expectedByteCount);
    std::size_t index{0U};
    while (index < encoded.size())
    {
        const std::uint8_t control = encoded[index++];
        if (control <= 127U)
        {
            const std::size_t count = static_cast<std::size_t>(control) + 1U;
            if (index + count > encoded.size()
                || decoded.size() + count > expectedByteCount)
            {
                throw std::runtime_error(
                    "malformed TIFF PackBits literal packet: "
                    + path.string());
            }
            decoded.insert(
                decoded.end(),
                encoded.begin() + static_cast<std::ptrdiff_t>(index),
                encoded.begin() + static_cast<std::ptrdiff_t>(index + count));
            index += count;
            continue;
        }
        if (control == 128U)
        {
            continue;
        }
        if (index >= encoded.size())
        {
            throw std::runtime_error(
                "malformed TIFF PackBits repeat packet: " + path.string());
        }
        const std::size_t count = 257U - control;
        if (decoded.size() + count > expectedByteCount)
        {
            throw std::runtime_error(
                "TIFF PackBits output exceeds expected dimensions: "
                + path.string());
        }
        decoded.insert(decoded.end(), count, encoded[index++]);
    }
    if (decoded.size() != expectedByteCount)
    {
        throw std::runtime_error(
            "TIFF PackBits output does not match dimensions: " + path.string());
    }
    return decoded;
}

}  // namespace slicer_core::tiff_read_internal
