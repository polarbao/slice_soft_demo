#pragma once

#include "slicer_core/TiffReadApi.h"

#include <algorithm>
#include <span>
#include <stdexcept>

namespace slicer_core
{

inline const char* TiffRowOrderName(const TiffRowOrder order)
{
    switch (order)
    {
        case TiffRowOrder::MinYFirst: return "min_y_first";
        case TiffRowOrder::MaxYFirst: return "max_y_first";
    }
    throw std::invalid_argument("TIFF rowOrder is invalid");
}

inline TiffRowOrder ParseTiffRowOrder(const std::string_view name)
{
    if (name == "min_y_first") return TiffRowOrder::MinYFirst;
    if (name == "max_y_first") return TiffRowOrder::MaxYFirst;
    throw std::invalid_argument("TIFF rowOrder is invalid: " + std::string{name});
}

inline std::string TiffImageDescription(const TiffImageSpec& spec)
{
    std::string description = spec.samples_per_pixel == 7U ? "RGBWSVT" : "RGBWSV";
    if (spec.row_order != TiffRowOrder::MinYFirst)
        description += ";rowOrder=" + std::string{TiffRowOrderName(spec.row_order)};
    return description;
}

inline TiffRowOrder ReadTiffRowDescription(
    const std::string_view description, const std::uint16_t channels,
    const std::uint16_t orientation)
{
    if (orientation != 1U)
        throw std::runtime_error("TIFF Orientation must be top-left (1)");
    const std::string prefix = channels == 7U ? "RGBWSVT" : "RGBWSV";
    if (description == prefix || (channels == 6U && description.empty()))
        return TiffRowOrder::MinYFirst;
    if (description == prefix + ";rowOrder=max_y_first")
        return TiffRowOrder::MaxYFirst;
    throw std::runtime_error("TIFF ImageDescription/rowOrder is unsupported");
}

// Involutive mapping: stored row <-> canonical minY-first row. X is unchanged.
inline std::uint32_t TiffCanonicalRow(const TiffImageSpec& spec, const std::uint32_t row)
{
    return spec.row_order == TiffRowOrder::MaxYFirst ? spec.height - 1U - row : row;
}

inline void CopyTiffStripToCanonical(
    const TiffImageSpec& spec, const std::span<const std::uint8_t> strip,
    const std::uint32_t startRow, const std::uint32_t rows,
    const std::span<std::uint8_t> destination)
{
    const std::size_t stride = static_cast<std::size_t>(spec.width) * spec.samples_per_pixel;
    for (std::uint32_t row = 0; row < rows; ++row)
        std::copy_n(strip.data() + row * stride, stride,
            destination.data() + TiffCanonicalRow(spec, startRow + row) * stride);
}

// One reusable strip, never an additional full image or layer stack.
class TiffStoredStrip
{
public:
    std::span<const std::uint8_t> Get(
        const TiffImageSpec& spec, const std::span<const std::uint8_t> pixels,
        const std::uint32_t startRow, const std::uint32_t rows)
    {
        const std::size_t stride = static_cast<std::size_t>(spec.width) * spec.samples_per_pixel;
        if (spec.row_order == TiffRowOrder::MinYFirst)
            return pixels.subspan(startRow * stride, rows * stride);
        scratch_.resize(rows * stride);
        for (std::uint32_t row = 0; row < rows; ++row)
            std::copy_n(pixels.data() + TiffCanonicalRow(spec, startRow + row) * stride,
                stride, scratch_.data() + row * stride);
        return scratch_;
    }
private:
    std::vector<std::uint8_t> scratch_;
};

} // namespace slicer_core
