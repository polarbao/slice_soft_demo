#include "slicer_core/api/implementation/PackageQueryFacadeInternal.h"

#include "miniz.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core::api::implementation::detail
{
namespace
{

void WriteLittleEndian16(std::ostream& output, const std::uint16_t value)
{
    output.put(static_cast<char>(value & 0xffU));
    output.put(static_cast<char>((value >> 8U) & 0xffU));
}

void WriteLittleEndian32(std::ostream& output, const std::uint32_t value)
{
    for (int shift = 0; shift <= 24; shift += 8)
    {
        output.put(static_cast<char>((value >> shift) & 0xffU));
    }
}

void WriteBmp(
    const std::filesystem::path& path,
    const MaterialPreviewResult& preview)
{
    constexpr std::uint32_t kFileHeaderSize{14U};
    constexpr std::uint32_t kInfoHeaderSize{40U};
    const std::uint32_t pixelBytes = preview.width * preview.height * 4U;
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error(
            "failed to open BMP output: " + path.generic_string());
    }
    output.put('B');
    output.put('M');
    WriteLittleEndian32(
        output,
        kFileHeaderSize + kInfoHeaderSize + pixelBytes);
    WriteLittleEndian16(output, 0U);
    WriteLittleEndian16(output, 0U);
    WriteLittleEndian32(output, kFileHeaderSize + kInfoHeaderSize);
    WriteLittleEndian32(output, kInfoHeaderSize);
    WriteLittleEndian32(output, preview.width);
    WriteLittleEndian32(output, preview.height);
    WriteLittleEndian16(output, 1U);
    WriteLittleEndian16(output, 32U);
    WriteLittleEndian32(output, 0U);
    WriteLittleEndian32(output, pixelBytes);
    WriteLittleEndian32(output, 0U);
    WriteLittleEndian32(output, 0U);
    WriteLittleEndian32(output, 0U);
    WriteLittleEndian32(output, 0U);

    for (std::uint32_t y = preview.height; y > 0U; --y)
    {
        const std::uint32_t sourceY = y - 1U;
        for (std::uint32_t x = 0U; x < preview.width; ++x)
        {
            const std::size_t offset =
                (static_cast<std::size_t>(sourceY) * preview.width + x)
                * 4U;
            output.put(static_cast<char>(preview.rgba.at(offset + 2U)));
            output.put(static_cast<char>(preview.rgba.at(offset + 1U)));
            output.put(static_cast<char>(preview.rgba.at(offset)));
            output.put(static_cast<char>(preview.rgba.at(offset + 3U)));
        }
    }
    if (!output)
    {
        throw std::runtime_error(
            "failed to write BMP output: " + path.generic_string());
    }
}

void WritePng(
    const std::filesystem::path& path,
    const MaterialPreviewResult& preview)
{
    std::size_t pngSize{0U};
    void* pngData = tdefl_write_image_to_png_file_in_memory(
        preview.rgba.data(),
        static_cast<int>(preview.width),
        static_cast<int>(preview.height),
        4,
        &pngSize);
    if (pngData == nullptr)
    {
        throw std::runtime_error("failed to encode PNG preview");
    }

    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        mz_free(pngData);
        throw std::runtime_error(
            "failed to open PNG output: " + path.generic_string());
    }
    output.write(
        static_cast<const char*>(pngData),
        static_cast<std::streamsize>(pngSize));
    mz_free(pngData);
    if (!output)
    {
        throw std::runtime_error(
            "failed to write PNG output: " + path.generic_string());
    }
}

void WritePpm(
    const std::filesystem::path& path,
    const MaterialPreviewResult& preview)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error(
            "failed to open PPM output: " + path.generic_string());
    }
    output << "P6\n" << preview.width << ' ' << preview.height
           << "\n255\n";
    for (std::size_t offset = 0U;
         offset < preview.rgba.size();
         offset += 4U)
    {
        output.put(static_cast<char>(preview.rgba.at(offset)));
        output.put(static_cast<char>(preview.rgba.at(offset + 1U)));
        output.put(static_cast<char>(preview.rgba.at(offset + 2U)));
    }
    if (!output)
    {
        throw std::runtime_error(
            "failed to write PPM output: " + path.generic_string());
    }
}

std::string LowercaseExtension(std::filesystem::path path)
{
    std::string extension = path.extension().string();
    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](const unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        });
    return extension;
}

}  // namespace

MaterialPreviewResult ResizePreview(
    MaterialPreviewResult source,
    const int maxWidth)
{
    if (maxWidth <= 0)
    {
        throw std::runtime_error("preview maxWidthPx must be positive");
    }
    if (source.width <= static_cast<std::uint32_t>(maxWidth))
    {
        return source;
    }

    const std::uint32_t targetWidth =
        static_cast<std::uint32_t>(maxWidth);
    const double scale = static_cast<double>(targetWidth)
        / static_cast<double>(source.width);
    const std::uint32_t targetHeight = std::max(
        1U,
        static_cast<std::uint32_t>(std::lround(
            static_cast<double>(source.height) * scale)));
    std::vector<std::uint8_t> resized(
        static_cast<std::size_t>(targetWidth)
            * static_cast<std::size_t>(targetHeight) * 4U,
        0U);
    for (std::uint32_t y = 0U; y < targetHeight; ++y)
    {
        const std::uint32_t sourceY = std::min(
            source.height - 1U,
            static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(y) * source.height
                / targetHeight));
        for (std::uint32_t x = 0U; x < targetWidth; ++x)
        {
            const std::uint32_t sourceX = std::min(
                source.width - 1U,
                static_cast<std::uint32_t>(
                    static_cast<std::uint64_t>(x) * source.width
                    / targetWidth));
            const std::size_t sourceOffset =
                (static_cast<std::size_t>(sourceY) * source.width + sourceX)
                * 4U;
            const std::size_t targetOffset =
                (static_cast<std::size_t>(y) * targetWidth + x) * 4U;
            std::copy_n(
                source.rgba.begin()
                    + static_cast<std::ptrdiff_t>(sourceOffset),
                4,
                resized.begin()
                    + static_cast<std::ptrdiff_t>(targetOffset));
        }
    }
    source.width = targetWidth;
    source.height = targetHeight;
    source.rgba = std::move(resized);
    return source;
}

void WritePreviewImage(
    const std::filesystem::path& path,
    const MaterialPreviewResult& preview)
{
    if (path.empty())
    {
        throw std::runtime_error("preview output path is empty");
    }
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path());
    }
    const std::string extension = LowercaseExtension(path);
    if (extension == ".bmp")
    {
        WriteBmp(path, preview);
        return;
    }
    if (extension == ".png")
    {
        WritePng(path, preview);
        return;
    }
    if (extension == ".ppm")
    {
        WritePpm(path, preview);
        return;
    }
    throw std::runtime_error(
        "preview output extension must be .bmp, .png, or .ppm");
}

}  // namespace slicer_core::api::implementation::detail
