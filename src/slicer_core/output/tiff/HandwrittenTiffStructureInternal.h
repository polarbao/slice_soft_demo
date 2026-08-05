#pragma once

#include "slicer_core/TiffReadApi.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace slicer_core::handwritten_tiff_internal
{

enum class TiffFieldType : std::uint16_t
{
    Ascii = 2,
    Short = 3,
    Long = 4
};

struct IfdEntry
{
    std::uint16_t tag{0U};
    TiffFieldType type{TiffFieldType::Long};
    std::uint32_t count{0U};
    std::vector<std::uint8_t> value;
};

inline void AppendU16(
    std::vector<std::uint8_t>& data,
    const std::uint16_t value)
{
    data.push_back(static_cast<std::uint8_t>(value & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

inline void AppendU32(
    std::vector<std::uint8_t>& data,
    const std::uint32_t value)
{
    data.push_back(static_cast<std::uint8_t>(value & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

inline std::vector<std::uint8_t> Shorts(
    const std::vector<std::uint16_t>& values)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(values.size() * 2U);
    for (const std::uint16_t value : values)
    {
        AppendU16(bytes, value);
    }
    return bytes;
}

inline std::vector<std::uint8_t> Longs(
    const std::vector<std::uint32_t>& values)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(values.size() * 4U);
    for (const std::uint32_t value : values)
    {
        AppendU32(bytes, value);
    }
    return bytes;
}

inline std::vector<std::uint8_t> Ascii(const std::string& text)
{
    std::vector<std::uint8_t> bytes{text.begin(), text.end()};
    bytes.push_back(0U);
    return bytes;
}

inline void WriteEntry(
    std::vector<std::uint8_t>& ifd,
    std::vector<std::uint8_t>& extraData,
    const IfdEntry& entry,
    const std::uint32_t extraBaseOffset)
{
    AppendU16(ifd, entry.tag);
    AppendU16(ifd, static_cast<std::uint16_t>(entry.type));
    AppendU32(ifd, entry.count);
    if (entry.value.size() <= 4U)
    {
        ifd.insert(ifd.end(), entry.value.begin(), entry.value.end());
        while ((ifd.size() % 12U) != 2U)
        {
            ifd.push_back(0U);
        }
        return;
    }
    AppendU32(
        ifd,
        extraBaseOffset + static_cast<std::uint32_t>(extraData.size()));
    extraData.insert(extraData.end(), entry.value.begin(), entry.value.end());
}

inline std::vector<std::uint8_t> BuildIfd(
    std::vector<IfdEntry> entries,
    const std::uint32_t ifdOffset)
{
    std::sort(
        entries.begin(), entries.end(),
        [](const IfdEntry& left, const IfdEntry& right)
        {
            return left.tag < right.tag;
        });
    std::vector<std::uint8_t> ifd;
    std::vector<std::uint8_t> extraData;
    AppendU16(ifd, static_cast<std::uint16_t>(entries.size()));
    const std::uint32_t extraBaseOffset = ifdOffset + 2U
        + static_cast<std::uint32_t>(entries.size() * 12U) + 4U;
    for (const IfdEntry& entry : entries)
    {
        WriteEntry(ifd, extraData, entry, extraBaseOffset);
    }
    AppendU32(ifd, 0U);
    ifd.insert(ifd.end(), extraData.begin(), extraData.end());
    return ifd;
}

inline std::uint16_t CompressionTagValue(const TiffCompressionMode mode)
{
    switch (mode)
    {
        case TiffCompressionMode::None:
            return 1U;
        case TiffCompressionMode::PackBits:
            return 32773U;
    }
    throw std::runtime_error("unsupported TIFF compression mode");
}

inline void WriteFile(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& payload,
    const std::vector<std::uint8_t>& ifd,
    const std::uint32_t ifdOffset)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    if (!output)
    {
        throw std::runtime_error(
            "failed to open TIFF for writing: " + path.string());
    }
    output.put('I');
    output.put('I');
    output.put(42);
    output.put(0);
    output.put(static_cast<char>(ifdOffset & 0xffU));
    output.put(static_cast<char>((ifdOffset >> 8U) & 0xffU));
    output.put(static_cast<char>((ifdOffset >> 16U) & 0xffU));
    output.put(static_cast<char>((ifdOffset >> 24U) & 0xffU));
    output.write(
        reinterpret_cast<const char*>(payload.data()),
        static_cast<std::streamsize>(payload.size()));
    output.write(
        reinterpret_cast<const char*>(ifd.data()),
        static_cast<std::streamsize>(ifd.size()));
}

}  // namespace slicer_core::handwritten_tiff_internal
