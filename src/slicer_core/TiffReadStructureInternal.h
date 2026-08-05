#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace slicer_core::tiff_read_internal
{
enum class TiffFieldType : std::uint16_t
{
    Ascii = 2,
    Short = 3,
    Long = 4
};
struct ParsedTiffEntry
{
    TiffFieldType type{TiffFieldType::Long};
    std::uint32_t count{0};
    std::uint32_t valueOrOffset{0};
};
using ParsedTiffEntries =
    std::vector<std::pair<std::uint16_t, ParsedTiffEntry>>;

inline std::uint16_t ReadU16(
    const std::vector<std::uint8_t>& data,
    const std::size_t offset)
{
    if (offset + 2U > data.size())
    {
        throw std::runtime_error("truncated TIFF while reading uint16");
    }
    return static_cast<std::uint16_t>(
        data.at(offset)
        | (static_cast<std::uint16_t>(data.at(offset + 1U)) << 8U));
}
inline std::uint32_t ReadU32(
    const std::vector<std::uint8_t>& data,
    const std::size_t offset)
{
    if (offset + 4U > data.size())
    {
        throw std::runtime_error("truncated TIFF while reading uint32");
    }
    return static_cast<std::uint32_t>(data.at(offset))
        | (static_cast<std::uint32_t>(data.at(offset + 1U)) << 8U)
        | (static_cast<std::uint32_t>(data.at(offset + 2U)) << 16U)
        | (static_cast<std::uint32_t>(data.at(offset + 3U)) << 24U);
}
inline std::uint32_t FieldTypeSize(const TiffFieldType type)
{
    switch (type)
    {
        case TiffFieldType::Ascii:
            return 1U;
        case TiffFieldType::Short:
            return 2U;
        case TiffFieldType::Long:
            return 4U;
    }
    throw std::runtime_error("unsupported TIFF field type");
}
inline std::vector<std::uint16_t> ReadU16Array(
    const std::vector<std::uint8_t>& data,
    const ParsedTiffEntry& entry,
    const std::uint16_t tag)
{
    if (entry.type != TiffFieldType::Short)
    {
        throw std::runtime_error(
            "TIFF tag has unexpected type: " + std::to_string(tag));
    }
    std::vector<std::uint16_t> result;
    result.reserve(entry.count);
    const std::size_t byteCount =
        static_cast<std::size_t>(entry.count) * FieldTypeSize(entry.type);
    if (byteCount <= 4U)
    {
        for (std::uint32_t index{0U}; index < entry.count; ++index)
        {
            result.push_back(static_cast<std::uint16_t>(
                (entry.valueOrOffset >> (index * 16U)) & 0xffffU));
        }
        return result;
    }
    if (static_cast<std::size_t>(entry.valueOrOffset) + byteCount > data.size())
    {
        throw std::runtime_error(
            "TIFF tag data outside file: " + std::to_string(tag));
    }
    for (std::uint32_t index{0U}; index < entry.count; ++index)
    {
        result.push_back(ReadU16(
            data,
            static_cast<std::size_t>(entry.valueOrOffset) + index * 2U));
    }
    return result;
}
inline std::vector<std::uint32_t> ReadU32Array(
    const std::vector<std::uint8_t>& data,
    const ParsedTiffEntry& entry,
    const std::uint16_t tag)
{
    if (entry.type != TiffFieldType::Short
        && entry.type != TiffFieldType::Long)
    {
        throw std::runtime_error(
            "TIFF tag has unexpected type: " + std::to_string(tag));
    }
    std::vector<std::uint32_t> result;
    result.reserve(entry.count);
    const std::size_t byteCount =
        static_cast<std::size_t>(entry.count) * FieldTypeSize(entry.type);
    for (std::uint32_t index{0U}; index < entry.count; ++index)
    {
        if (byteCount <= 4U)
        {
            result.push_back(
                entry.type == TiffFieldType::Short
                    ? static_cast<std::uint16_t>(
                          (entry.valueOrOffset >> (index * 16U)) & 0xffffU)
                    : entry.valueOrOffset);
            continue;
        }
        const std::size_t offset = static_cast<std::size_t>(entry.valueOrOffset)
            + static_cast<std::size_t>(index) * FieldTypeSize(entry.type);
        if (offset + FieldTypeSize(entry.type) > data.size())
        {
            throw std::runtime_error(
                "TIFF tag data outside file: " + std::to_string(tag));
        }
        result.push_back(
            entry.type == TiffFieldType::Short
                ? ReadU16(data, offset)
                : ReadU32(data, offset));
    }
    return result;
}
inline ParsedTiffEntries ParseIfdEntries(
    const std::vector<std::uint8_t>& data,
    const std::filesystem::path& path)
{
    if (data.size() < 8U || data.at(0U) != 'I' || data.at(1U) != 'I'
        || ReadU16(data, 2U) != 42U)
    {
        throw std::runtime_error(
            "unsupported or invalid TIFF header: " + path.string());
    }
    const std::uint32_t ifdOffset{ReadU32(data, 4U)};
    if (static_cast<std::size_t>(ifdOffset) + 2U > data.size())
    {
        throw std::runtime_error(
            "TIFF IFD offset outside file: " + path.string());
    }
    const std::uint16_t count{ReadU16(data, ifdOffset)};
    ParsedTiffEntries entries;
    entries.reserve(count);
    for (std::uint16_t index{0U}; index < count; ++index)
    {
        const std::size_t offset = static_cast<std::size_t>(ifdOffset) + 2U
            + static_cast<std::size_t>(index) * 12U;
        if (offset + 12U > data.size())
        {
            throw std::runtime_error("truncated TIFF IFD: " + path.string());
        }
        entries.push_back({
            ReadU16(data, offset),
            {static_cast<TiffFieldType>(ReadU16(data, offset + 2U)),
             ReadU32(data, offset + 4U), ReadU32(data, offset + 8U)}});
    }
    return entries;
}
inline std::optional<ParsedTiffEntry> FindOptionalEntry(
    const ParsedTiffEntries& entries,
    const std::uint16_t tag)
{
    const auto found = std::find_if(
        entries.begin(), entries.end(),
        [tag](const auto& item) { return item.first == tag; });
    return found == entries.end()
        ? std::nullopt
        : std::optional<ParsedTiffEntry>{found->second};
}
inline ParsedTiffEntry FindRequiredEntry(
    const ParsedTiffEntries& entries,
    const std::uint16_t tag)
{
    const auto found = FindOptionalEntry(entries, tag);
    if (!found.has_value())
    {
        throw std::runtime_error(
            "missing required TIFF tag: " + std::to_string(tag));
    }
    return found.value();
}
}  // namespace slicer_core::tiff_read_internal
