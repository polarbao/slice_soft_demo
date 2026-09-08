#pragma once

#include "slicer_core/TiffReadStructureInternal.h"
#include "slicer_core/output/tiff/TiffRowLayout.h"

namespace slicer_core::tiff_read_internal
{

inline TiffRowOrder ReadRowOrder(
    const std::vector<std::uint8_t>& data, const ParsedTiffEntries& entries)
{
    std::uint16_t orientation = 1U;
    if (const auto entry = FindOptionalEntry(entries, 274U))
    {
        const auto values = ReadU16Array(data, *entry, 274U);
        if (values.size() != 1U) throw std::runtime_error("TIFF Orientation count is invalid");
        orientation = values.front();
    }
    std::string description;
    if (const auto entry = FindOptionalEntry(entries, 270U))
    {
        if (entry->type != TiffFieldType::Ascii
            || entry->count == 0U || entry->count > 128U)
            throw std::runtime_error("TIFF ImageDescription type/count is invalid");
        if (entry->count <= 4U)
        {
            for (std::uint32_t i = 0; i < entry->count; ++i)
                description.push_back(static_cast<char>((entry->valueOrOffset >> (8U * i)) & 255U));
        }
        else
        {
            const std::size_t offset = entry->valueOrOffset;
            if (offset > data.size() || entry->count > data.size() - offset)
                throw std::runtime_error("TIFF ImageDescription is outside file");
            description.assign(reinterpret_cast<const char*>(data.data() + offset), entry->count);
        }
        if (description.back() != '\0')
            throw std::runtime_error("TIFF ImageDescription is not terminated");
        description.pop_back();
    }
    return ReadTiffRowDescription(description, 6U, orientation);
}

} // namespace slicer_core::tiff_read_internal
