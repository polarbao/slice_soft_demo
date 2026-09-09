#pragma once
#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>
#ifdef SLICESOFT_XPAD_RAW_TIFF
#include <tiffio.h>

// Independent physical row check: no application Reader or row-order normalization.
inline void CheckRawPaddingRows(const std::filesystem::path& path,
    const std::vector<std::uint8_t>& original, int width, int height, int columns, int rows, int channels)
{
#ifdef _WIN32
    auto* raw = TIFFOpenW(path.c_str(), "r");
#else
    auto* raw = TIFFOpen(path.c_str(), "r");
#endif
    const std::unique_ptr<TIFF, decltype(&TIFFClose)> file(raw, TIFFClose);
    if (!file) throw std::runtime_error("raw TIFF open failed");
    std::uint16_t orientation = 0;
    TIFFGetFieldDefaulted(raw, TIFFTAG_ORIENTATION, &orientation);
    const auto stride = static_cast<std::size_t>(width + columns) * channels;
    if (orientation != 1 || TIFFIsTiled(raw) || TIFFScanlineSize64(raw) != stride)
        throw std::runtime_error("raw padding test requires top-left stripped TIFF");
    std::vector<std::uint8_t> actual(stride), expected(stride, 255);
    for (int y = 0; y < height + rows; ++y)
    {
        std::fill(expected.begin(), expected.end(), 255);
        if (y < height)
        {
            const auto begin = original.begin() + static_cast<std::ptrdiff_t>(height - 1 - y) * width * channels;
            std::copy_n(begin, static_cast<std::size_t>(width) * channels, expected.begin() + columns * channels);
        }
        if (TIFFReadScanline(raw, actual.data(), static_cast<std::uint32_t>(y), 0) != 1 || actual != expected)
            throw std::runtime_error("raw TIFF body/left/bottom row mismatch");
    }
}
#endif
