#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace slicer_core::handwritten_tiff_internal
{

inline void EncodePackBitsRow(
    const std::span<const std::uint8_t> row,
    std::vector<std::uint8_t>& encoded)
{
    std::size_t index{0U};
    while (index < row.size())
    {
        std::size_t runLength{1U};
        while (index + runLength < row.size() && runLength < 128U
               && row[index + runLength] == row[index])
        {
            ++runLength;
        }
        if (runLength >= 3U)
        {
            encoded.push_back(static_cast<std::uint8_t>(257U - runLength));
            encoded.push_back(row[index]);
            index += runLength;
            continue;
        }

        const std::size_t literalStart = index;
        index += runLength;
        while (index < row.size() && index - literalStart < 128U)
        {
            runLength = 1U;
            while (index + runLength < row.size() && runLength < 128U
                   && row[index + runLength] == row[index])
            {
                ++runLength;
            }
            if (runLength >= 3U
                || index - literalStart + runLength > 128U)
            {
                break;
            }
            index += runLength;
        }
        const std::size_t literalCount = index - literalStart;
        encoded.push_back(static_cast<std::uint8_t>(literalCount - 1U));
        encoded.insert(
            encoded.end(),
            row.begin() + static_cast<std::ptrdiff_t>(literalStart),
            row.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

inline std::vector<std::uint8_t> EncodePackBitsBlock(
    const std::span<const std::uint8_t> bytes,
    const std::size_t rowByteCount,
    const std::uint32_t rowCount)
{
    if (rowByteCount == 0U
        || rowByteCount * static_cast<std::size_t>(rowCount) != bytes.size())
    {
        throw std::runtime_error("invalid TIFF PackBits block dimensions");
    }
    std::vector<std::uint8_t> encoded;
    encoded.reserve(bytes.size() + bytes.size() / 128U + rowCount);
    for (std::uint32_t row{0U}; row < rowCount; ++row)
    {
        const std::size_t offset = static_cast<std::size_t>(row) * rowByteCount;
        EncodePackBitsRow(bytes.subspan(offset, rowByteCount), encoded);
    }
    return encoded;
}

}  // namespace slicer_core::handwritten_tiff_internal
