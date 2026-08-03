#include "slicer_core/tiff_io.h"

#include "slicer_core/output/tiff/TiffWriterFactory.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

namespace slicer_core {
namespace {

enum class TiffType : std::uint16_t {
    ascii = 2,
    short_value = 3,
    long_value = 4
};

struct IfdEntry {
    std::uint16_t tag{0};
    TiffType type{TiffType::long_value};
    std::uint32_t count{0};
    std::vector<std::uint8_t> value;
};

struct ParsedEntry {
    TiffType type{TiffType::long_value};
    std::uint32_t count{0};
    std::uint32_t value_or_offset{0};
};

using ParsedEntries = std::vector<std::pair<std::uint16_t, ParsedEntry>>;

void append_u16(std::vector<std::uint8_t>& data, const std::uint16_t value) {
    data.push_back(static_cast<std::uint8_t>(value & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void append_u32(std::vector<std::uint8_t>& data, const std::uint32_t value) {
    data.push_back(static_cast<std::uint8_t>(value & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& data, const std::size_t offset) {
    if (offset + 2 > data.size()) {
        throw std::runtime_error("truncated TIFF while reading uint16");
    }
    return static_cast<std::uint16_t>(data.at(offset) | (static_cast<std::uint16_t>(data.at(offset + 1)) << 8U));
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& data, const std::size_t offset) {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("truncated TIFF while reading uint32");
    }
    return static_cast<std::uint32_t>(data.at(offset))
        | (static_cast<std::uint32_t>(data.at(offset + 1)) << 8U)
        | (static_cast<std::uint32_t>(data.at(offset + 2)) << 16U)
        | (static_cast<std::uint32_t>(data.at(offset + 3)) << 24U);
}

std::uint16_t read_inline_u16(const ParsedEntry& entry) {
    return static_cast<std::uint16_t>(entry.value_or_offset & 0xffffU);
}

std::uint32_t type_size(const TiffType type) {
    switch (type) {
        case TiffType::ascii:
            return 1;
        case TiffType::short_value:
            return 2;
        case TiffType::long_value:
            return 4;
    }
    throw std::runtime_error("unsupported TIFF field type");
}

std::vector<std::uint8_t> shorts(const std::vector<std::uint16_t>& values) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(values.size() * 2);
    for (const std::uint16_t value : values) {
        append_u16(bytes, value);
    }
    return bytes;
}

std::vector<std::uint8_t> longs(const std::vector<std::uint32_t>& values) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(values.size() * 4);
    for (const std::uint32_t value : values) {
        append_u32(bytes, value);
    }
    return bytes;
}

std::vector<std::uint8_t> ascii(const std::string& text) {
    std::vector<std::uint8_t> bytes{text.begin(), text.end()};
    bytes.push_back(0);
    return bytes;
}

void write_entry(
    std::vector<std::uint8_t>& ifd,
    std::vector<std::uint8_t>& extra_data,
    const IfdEntry& entry,
    const std::uint32_t extra_base_offset) {
    append_u16(ifd, entry.tag);
    append_u16(ifd, static_cast<std::uint16_t>(entry.type));
    append_u32(ifd, entry.count);

    if (entry.value.size() <= 4) {
        ifd.insert(ifd.end(), entry.value.begin(), entry.value.end());
        while ((ifd.size() % 12) != 2) {
            ifd.push_back(0);
        }
        return;
    }

    const std::uint32_t value_offset = extra_base_offset + static_cast<std::uint32_t>(extra_data.size());
    append_u32(ifd, value_offset);
    extra_data.insert(extra_data.end(), entry.value.begin(), entry.value.end());
}

std::vector<std::uint16_t> read_u16_array(
    const std::vector<std::uint8_t>& data,
    const ParsedEntry& entry,
    const std::uint16_t tag) {
    if (entry.type != TiffType::short_value) {
        throw std::runtime_error("TIFF tag has unexpected type: " + std::to_string(tag));
    }
    std::vector<std::uint16_t> result;
    result.reserve(entry.count);
    const std::size_t byteCount = static_cast<std::size_t>(entry.count) * type_size(entry.type);
    if (byteCount <= 4U) {
        for (std::uint32_t i{0}; i < entry.count; ++i) {
            result.push_back(static_cast<std::uint16_t>(
                (entry.value_or_offset >> (i * 16U)) & 0xffffU));
        }
        return result;
    }
    if (static_cast<std::size_t>(entry.value_or_offset) + byteCount > data.size()) {
        throw std::runtime_error("TIFF tag data outside file: " + std::to_string(tag));
    }
    for (std::uint32_t i{0}; i < entry.count; ++i) {
        result.push_back(read_u16(data, static_cast<std::size_t>(entry.value_or_offset) + i * 2U));
    }
    return result;
}

std::vector<std::uint32_t> read_u32_array(
    const std::vector<std::uint8_t>& data,
    const ParsedEntry& entry,
    const std::uint16_t tag) {
    if (entry.type != TiffType::short_value
        && entry.type != TiffType::long_value) {
        throw std::runtime_error("TIFF tag has unexpected type: " + std::to_string(tag));
    }
    std::vector<std::uint32_t> result;
    result.reserve(entry.count);
    const std::size_t byteCount = static_cast<std::size_t>(entry.count) * type_size(entry.type);
    if (byteCount <= 4U) {
        for (std::uint32_t i{0}; i < entry.count; ++i) {
            result.push_back(
                entry.type == TiffType::short_value
                    ? static_cast<std::uint16_t>(
                          (entry.value_or_offset >> (i * 16U)) & 0xffffU)
                    : entry.value_or_offset);
        }
        return result;
    }
    if (static_cast<std::size_t>(entry.value_or_offset) + byteCount > data.size()) {
        throw std::runtime_error("TIFF tag data outside file: " + std::to_string(tag));
    }
    for (std::uint32_t i{0}; i < entry.count; ++i) {
        const std::size_t value_offset =
            static_cast<std::size_t>(entry.value_or_offset)
            + static_cast<std::size_t>(i) * type_size(entry.type);
        result.push_back(
            entry.type == TiffType::short_value
                ? read_u16(data, value_offset)
                : read_u32(data, value_offset));
    }
    return result;
}

ParsedEntries parse_ifd_entries(const std::vector<std::uint8_t>& data, const std::filesystem::path& path) {
    if (data.size() < 8 || data.at(0) != 'I' || data.at(1) != 'I' || read_u16(data, 2) != 42) {
        throw std::runtime_error("unsupported or invalid TIFF header: " + path.string());
    }

    const std::uint32_t ifd_offset{read_u32(data, 4)};
    if (static_cast<std::size_t>(ifd_offset) + 2 > data.size()) {
        throw std::runtime_error("TIFF IFD offset outside file: " + path.string());
    }
    const std::uint16_t entry_count{read_u16(data, ifd_offset)};
    ParsedEntries entries;
    entries.reserve(entry_count);
    for (std::uint16_t i{0}; i < entry_count; ++i) {
        const std::size_t offset = static_cast<std::size_t>(ifd_offset) + 2U + static_cast<std::size_t>(i) * 12U;
        if (offset + 12 > data.size()) {
            throw std::runtime_error("truncated TIFF IFD: " + path.string());
        }
        const std::uint16_t tag{read_u16(data, offset)};
        const auto type = static_cast<TiffType>(read_u16(data, offset + 2U));
        const std::uint32_t count{read_u32(data, offset + 4U)};
        const std::uint32_t value_or_offset{read_u32(data, offset + 8U)};
        entries.push_back({tag, {type, count, value_or_offset}});
    }
    return entries;
}

std::optional<ParsedEntry> find_optional_entry(const ParsedEntries& entries, const std::uint16_t tag) {
    const auto found = std::find_if(entries.begin(), entries.end(), [tag](const auto& item) {
        return item.first == tag;
    });
    if (found == entries.end()) {
        return std::nullopt;
    }
    return found->second;
}

ParsedEntry find_required_entry(const ParsedEntries& entries, const std::uint16_t tag) {
    const auto found = find_optional_entry(entries, tag);
    if (!found.has_value()) {
        throw std::runtime_error("missing required TIFF tag: " + std::to_string(tag));
    }
    return found.value();
}

void validate_common_spec(
    TiffReadResult& result,
    const std::vector<std::uint16_t>& bits_per_sample,
    const std::vector<std::uint16_t>& sample_formats,
    const std::filesystem::path& path) {
    if (result.spec.samples_per_pixel != rgbwsv_channel_count || bits_per_sample.size() != rgbwsv_channel_count
        || sample_formats.size() != rgbwsv_channel_count) {
        throw std::runtime_error("TIFF is not a six-channel RGBWSV image: " + path.string());
    }
    for (std::size_t i{0}; i < bits_per_sample.size(); ++i) {
        if (bits_per_sample.at(i) != 8 || sample_formats.at(i) != 1) {
            throw std::runtime_error("TIFF channel is not uint8: " + path.string());
        }
    }
    if (result.spec.planar_config != 1) {
        throw std::runtime_error("TIFF planar config is not contiguous: " + path.string());
    }
}

void update_channel_stats(TiffReadResult& result, const std::uint16_t channel, const std::uint8_t value) {
    result.channel_checksums.at(channel) += value;
    TiffChannelStats& stats = result.channel_stats.at(channel);
    stats.min_value = std::min(stats.min_value, static_cast<int>(value));
    stats.max_value = std::max(stats.max_value, static_cast<int>(value));
    if (value == 255U) {
        ++stats.empty_pixels;
    } else {
        ++stats.print_pixels;
        if (value == 0U) {
            ++stats.full_print_pixels;
        } else {
            ++stats.partial_print_pixels;
        }
    }
}

void initialize_read_pixels(TiffReadResult& result) {
    const std::size_t pixel_count =
        static_cast<std::size_t>(result.spec.width) * result.spec.height * result.spec.samples_per_pixel;
    result.pixels.assign(pixel_count, 255);
}

constexpr std::uint16_t TiffCompressionTagValue(
    const TiffCompressionMode mode)
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

TiffCompressionMode ReadTiffCompressionMode(
    const std::vector<std::uint8_t>& data,
    const ParsedEntries& entries,
    const std::filesystem::path& path)
{
    const std::uint16_t value = read_u16_array(
        data,
        find_required_entry(entries, 259U),
        259U)
                                    .at(0);
    if (value == 1U)
    {
        return TiffCompressionMode::None;
    }
    if (value == 32773U)
    {
        return TiffCompressionMode::PackBits;
    }
    throw std::runtime_error(
        "unsupported TIFF compression: " + std::to_string(value)
        + ": " + path.string());
}

void EncodePackBitsRow(
    const std::span<const std::uint8_t> row,
    std::vector<std::uint8_t>& encoded)
{
    std::size_t index{0U};
    while (index < row.size())
    {
        std::size_t runLength{1U};
        while (index + runLength < row.size()
               && runLength < 128U
               && row[index + runLength] == row[index])
        {
            ++runLength;
        }

        if (runLength >= 3U)
        {
            encoded.push_back(
                static_cast<std::uint8_t>(257U - runLength));
            encoded.push_back(row[index]);
            index += runLength;
            continue;
        }

        const std::size_t literalStart = index;
        index += runLength;
        while (index < row.size()
               && index - literalStart < 128U)
        {
            runLength = 1U;
            while (index + runLength < row.size()
                   && runLength < 128U
                   && row[index + runLength] == row[index])
            {
                ++runLength;
            }
            if (runLength >= 3U)
            {
                break;
            }
            if (index - literalStart + runLength > 128U)
            {
                break;
            }
            index += runLength;
        }

        const std::size_t literalCount = index - literalStart;
        encoded.push_back(
            static_cast<std::uint8_t>(literalCount - 1U));
        encoded.insert(
            encoded.end(),
            row.begin() + static_cast<std::ptrdiff_t>(literalStart),
            row.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

std::vector<std::uint8_t> EncodePackBitsBlock(
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
        const std::size_t offset =
            static_cast<std::size_t>(row) * rowByteCount;
        EncodePackBitsRow(bytes.subspan(offset, rowByteCount), encoded);
    }
    return encoded;
}

std::vector<std::uint8_t> DecodePackBitsBlock(
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
            const std::size_t literalCount =
                static_cast<std::size_t>(control) + 1U;
            if (index + literalCount > encoded.size()
                || decoded.size() + literalCount > expectedByteCount)
            {
                throw std::runtime_error(
                    "malformed TIFF PackBits literal packet: "
                    + path.string());
            }
            decoded.insert(
                decoded.end(),
                encoded.begin() + static_cast<std::ptrdiff_t>(index),
                encoded.begin()
                    + static_cast<std::ptrdiff_t>(index + literalCount));
            index += literalCount;
            continue;
        }
        if (control == 128U)
        {
            continue;
        }
        if (index >= encoded.size())
        {
            throw std::runtime_error(
                "malformed TIFF PackBits repeat packet: "
                + path.string());
        }
        const std::size_t repeatCount = 257U - control;
        if (decoded.size() + repeatCount > expectedByteCount)
        {
            throw std::runtime_error(
                "TIFF PackBits output exceeds expected dimensions: "
                + path.string());
        }
        decoded.insert(decoded.end(), repeatCount, encoded[index++]);
    }
    if (decoded.size() != expectedByteCount)
    {
        throw std::runtime_error(
            "TIFF PackBits output does not match dimensions: "
            + path.string());
    }
    return decoded;
}

}  // namespace

std::string tiff_storage_mode_string(const TiffStorageMode mode) {
    switch (mode) {
        case TiffStorageMode::Stripped:
            return "stripped";
        case TiffStorageMode::Tiled:
            return "tiled";
    }
    return "unknown";
}

std::string TiffCompressionModeString(const TiffCompressionMode mode)
{
    switch (mode)
    {
        case TiffCompressionMode::None:
            return "none";
        case TiffCompressionMode::PackBits:
            return "packbits";
    }
    return "unknown";
}

void write_rgbwsv_tiled_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels) {
    if (spec.width == 0 || spec.height == 0 || spec.tile_width == 0 || spec.tile_height == 0) {
        throw std::runtime_error("invalid TIFF dimensions");
    }
    if (spec.samples_per_pixel != rgbwsv_channel_count || spec.bits_per_sample != 8 || spec.planar_config != 1) {
        throw std::runtime_error("P0 00B TIFF writer only supports RGBWSV uint8 contiguous pixels");
    }
    const std::size_t expected_pixels =
        static_cast<std::size_t>(spec.width) * spec.height * spec.samples_per_pixel;
    if (pixels.size() != expected_pixels) {
        throw std::runtime_error("pixel buffer size does not match TIFF dimensions");
    }

    const std::uint32_t tiles_x{(spec.width + spec.tile_width - 1U) / spec.tile_width};
    const std::uint32_t tiles_y{(spec.height + spec.tile_height - 1U) / spec.tile_height};
    const std::uint32_t tile_count{tiles_x * tiles_y};
    const std::uint32_t tile_byte_count{
        spec.tile_width * spec.tile_height * spec.samples_per_pixel};

    std::vector<std::uint8_t> tile_data;
    std::vector<std::uint8_t> tile_scratch(tile_byte_count, 255U);
    std::vector<std::uint32_t> tile_offsets;
    std::vector<std::uint32_t> tile_byte_counts;
    tile_offsets.reserve(tile_count);
    tile_byte_counts.reserve(tile_count);
    constexpr std::uint32_t header_size{8};
    for (std::uint32_t tile_y{0}; tile_y < tiles_y; ++tile_y) {
        for (std::uint32_t tile_x{0}; tile_x < tiles_x; ++tile_x) {
            std::fill(tile_scratch.begin(), tile_scratch.end(), 255U);
            for (std::uint32_t y{0}; y < spec.tile_height; ++y) {
                const std::uint32_t image_y{tile_y * spec.tile_height + y};
                if (image_y >= spec.height) {
                    continue;
                }
                for (std::uint32_t x{0}; x < spec.tile_width; ++x) {
                    const std::uint32_t image_x{tile_x * spec.tile_width + x};
                    if (image_x >= spec.width) {
                        continue;
                    }
                    for (std::uint16_t c{0}; c < spec.samples_per_pixel; ++c) {
                        const std::size_t source_index =
                            (static_cast<std::size_t>(image_y) * spec.width + image_x) * spec.samples_per_pixel + c;
                        const std::uint8_t value{pixels[source_index]};
                        const std::size_t target_index =
                            (static_cast<std::size_t>(y) * spec.tile_width + x)
                                * spec.samples_per_pixel
                            + c;
                        tile_scratch.at(target_index) = value;
                    }
                }
            }

            tile_offsets.push_back(
                header_size + static_cast<std::uint32_t>(tile_data.size()));
            if (spec.compression_mode == TiffCompressionMode::PackBits)
            {
                const std::vector<std::uint8_t> encoded =
                    EncodePackBitsBlock(
                        tile_scratch,
                        static_cast<std::size_t>(spec.tile_width)
                            * spec.samples_per_pixel,
                        spec.tile_height);
                tile_byte_counts.push_back(
                    static_cast<std::uint32_t>(encoded.size()));
                tile_data.insert(
                    tile_data.end(),
                    encoded.begin(),
                    encoded.end());
            }
            else
            {
                tile_byte_counts.push_back(tile_byte_count);
                tile_data.insert(
                    tile_data.end(),
                    tile_scratch.begin(),
                    tile_scratch.end());
            }
        }
    }

    const std::uint32_t ifd_offset = header_size + static_cast<std::uint32_t>(tile_data.size());
    std::vector<IfdEntry> entries{
        {256, TiffType::long_value, 1, longs({spec.width})},
        {257, TiffType::long_value, 1, longs({spec.height})},
        {258, TiffType::short_value, spec.samples_per_pixel, shorts({8, 8, 8, 8, 8, 8})},
        {259,
         TiffType::short_value,
         1,
         shorts({TiffCompressionTagValue(spec.compression_mode)})},
        {262, TiffType::short_value, 1, shorts({2})},
        {270, TiffType::ascii, 7, ascii("RGBWSV")},
        {277, TiffType::short_value, 1, shorts({spec.samples_per_pixel})},
        {284, TiffType::short_value, 1, shorts({spec.planar_config})},
        {305, TiffType::ascii, 19, ascii("slice_soft_demo p0")},
        {322, TiffType::long_value, 1, longs({spec.tile_width})},
        {323, TiffType::long_value, 1, longs({spec.tile_height})},
        {324, TiffType::long_value, tile_count, longs(tile_offsets)},
        {325, TiffType::long_value, tile_count, longs(tile_byte_counts)},
        {338, TiffType::short_value, 3, shorts({0, 0, 0})},
        {339, TiffType::short_value, spec.samples_per_pixel, shorts({1, 1, 1, 1, 1, 1})}};
    std::sort(entries.begin(), entries.end(), [](const IfdEntry& a, const IfdEntry& b) { return a.tag < b.tag; });

    std::vector<std::uint8_t> ifd;
    std::vector<std::uint8_t> extra_data;
    append_u16(ifd, static_cast<std::uint16_t>(entries.size()));
    const std::uint32_t extra_base_offset =
        ifd_offset + 2U + static_cast<std::uint32_t>(entries.size() * 12U) + 4U;
    for (const auto& entry : entries) {
        write_entry(ifd, extra_data, entry, extra_base_offset);
    }
    append_u32(ifd, 0);
    ifd.insert(ifd.end(), extra_data.begin(), extra_data.end());

    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    if (!output) {
        throw std::runtime_error("failed to open TIFF for writing: " + path.string());
    }
    output.put('I');
    output.put('I');
    output.put(42);
    output.put(0);
    output.put(static_cast<char>(ifd_offset & 0xffU));
    output.put(static_cast<char>((ifd_offset >> 8U) & 0xffU));
    output.put(static_cast<char>((ifd_offset >> 16U) & 0xffU));
    output.put(static_cast<char>((ifd_offset >> 24U) & 0xffU));
    output.write(reinterpret_cast<const char*>(tile_data.data()), static_cast<std::streamsize>(tile_data.size()));
    output.write(reinterpret_cast<const char*>(ifd.data()), static_cast<std::streamsize>(ifd.size()));
}

void write_rgbwsv_stripped_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels) {
    if (spec.width == 0 || spec.height == 0 || spec.rows_per_strip == 0) {
        throw std::runtime_error("invalid TIFF dimensions");
    }
    if (spec.samples_per_pixel != rgbwsv_channel_count || spec.bits_per_sample != 8 || spec.planar_config != 1) {
        throw std::runtime_error("P0 03B TIFF writer only supports RGBWSV uint8 contiguous pixels");
    }
    const std::size_t expected_pixels =
        static_cast<std::size_t>(spec.width) * spec.height * spec.samples_per_pixel;
    if (pixels.size() != expected_pixels) {
        throw std::runtime_error("pixel buffer size does not match TIFF dimensions");
    }

    const std::uint32_t strip_count{(spec.height + spec.rows_per_strip - 1U) / spec.rows_per_strip};
    std::vector<std::uint32_t> strip_offsets;
    std::vector<std::uint32_t> strip_byte_counts;
    strip_offsets.reserve(strip_count);
    strip_byte_counts.reserve(strip_count);

    constexpr std::uint32_t header_size{8};
    std::vector<std::uint8_t> strip_data;
    strip_data.reserve(pixels.size());
    for (std::uint32_t strip_index{0}; strip_index < strip_count; ++strip_index) {
        const std::uint32_t start_row{strip_index * spec.rows_per_strip};
        const std::uint32_t rows{
            std::min(spec.rows_per_strip, static_cast<std::uint32_t>(spec.height - start_row))};
        const std::uint32_t byte_count{rows * spec.width * spec.samples_per_pixel};
        strip_offsets.push_back(header_size + static_cast<std::uint32_t>(strip_data.size()));
        const std::size_t source_offset =
            static_cast<std::size_t>(start_row) * spec.width * spec.samples_per_pixel;
        if (spec.compression_mode == TiffCompressionMode::PackBits)
        {
            const std::vector<std::uint8_t> encoded =
                EncodePackBitsBlock(
                    pixels.subspan(source_offset, byte_count),
                    static_cast<std::size_t>(spec.width)
                        * spec.samples_per_pixel,
                    rows);
            strip_byte_counts.push_back(
                static_cast<std::uint32_t>(encoded.size()));
            strip_data.insert(
                strip_data.end(),
                encoded.begin(),
                encoded.end());
        }
        else
        {
            strip_byte_counts.push_back(byte_count);
            strip_data.insert(
                strip_data.end(),
                pixels.begin() + static_cast<std::ptrdiff_t>(source_offset),
                pixels.begin()
                    + static_cast<std::ptrdiff_t>(source_offset + byte_count));
        }
    }

    const std::uint32_t ifd_offset = header_size + static_cast<std::uint32_t>(strip_data.size());
    std::vector<IfdEntry> entries{
        {256, TiffType::long_value, 1, longs({spec.width})},
        {257, TiffType::long_value, 1, longs({spec.height})},
        {258, TiffType::short_value, spec.samples_per_pixel, shorts({8, 8, 8, 8, 8, 8})},
        {259,
         TiffType::short_value,
         1,
         shorts({TiffCompressionTagValue(spec.compression_mode)})},
        {262, TiffType::short_value, 1, shorts({2})},
        {270, TiffType::ascii, 7, ascii("RGBWSV")},
        {273, TiffType::long_value, strip_count, longs(strip_offsets)},
        {277, TiffType::short_value, 1, shorts({spec.samples_per_pixel})},
        {278, TiffType::long_value, 1, longs({spec.rows_per_strip})},
        {279, TiffType::long_value, strip_count, longs(strip_byte_counts)},
        {284, TiffType::short_value, 1, shorts({spec.planar_config})},
        {305, TiffType::ascii, 19, ascii("slice_soft_demo p0")},
        {338, TiffType::short_value, 3, shorts({0, 0, 0})},
        {339, TiffType::short_value, spec.samples_per_pixel, shorts({1, 1, 1, 1, 1, 1})}};
    std::sort(entries.begin(), entries.end(), [](const IfdEntry& a, const IfdEntry& b) { return a.tag < b.tag; });

    std::vector<std::uint8_t> ifd;
    std::vector<std::uint8_t> extra_data;
    append_u16(ifd, static_cast<std::uint16_t>(entries.size()));
    const std::uint32_t extra_base_offset =
        ifd_offset + 2U + static_cast<std::uint32_t>(entries.size() * 12U) + 4U;
    for (const auto& entry : entries) {
        write_entry(ifd, extra_data, entry, extra_base_offset);
    }
    append_u32(ifd, 0);
    ifd.insert(ifd.end(), extra_data.begin(), extra_data.end());

    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    if (!output) {
        throw std::runtime_error("failed to open TIFF for writing: " + path.string());
    }
    output.put('I');
    output.put('I');
    output.put(42);
    output.put(0);
    output.put(static_cast<char>(ifd_offset & 0xffU));
    output.put(static_cast<char>((ifd_offset >> 8U) & 0xffU));
    output.put(static_cast<char>((ifd_offset >> 16U) & 0xffU));
    output.put(static_cast<char>((ifd_offset >> 24U) & 0xffU));
    output.write(reinterpret_cast<const char*>(strip_data.data()), static_cast<std::streamsize>(strip_data.size()));
    output.write(reinterpret_cast<const char*>(ifd.data()), static_cast<std::streamsize>(ifd.size()));
}

void write_rgbwsv_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    const std::span<const std::uint8_t> pixels) {
    WriteRgbwsvTiffWithConfiguredBackend(path, spec, pixels);
}

TiffReadResult read_rgbwsv_tiled_tiff(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("failed to open TIFF for reading: " + path.string());
    }
    const std::vector<std::uint8_t> data{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const ParsedEntries entries = parse_ifd_entries(data, path);

    TiffReadResult result;
    result.spec.storage_mode = TiffStorageMode::Tiled;
    result.spec.width = read_u32_array(data, find_required_entry(entries, 256), 256).at(0);
    result.spec.height = read_u32_array(data, find_required_entry(entries, 257), 257).at(0);
    result.spec.compression_mode =
        ReadTiffCompressionMode(data, entries, path);
    const auto bits_per_sample = read_u16_array(data, find_required_entry(entries, 258), 258);
    result.spec.samples_per_pixel = read_u16_array(data, find_required_entry(entries, 277), 277).at(0);
    result.spec.planar_config = read_u16_array(data, find_required_entry(entries, 284), 284).at(0);
    result.spec.tile_width = read_u32_array(data, find_required_entry(entries, 322), 322).at(0);
    result.spec.tile_height = read_u32_array(data, find_required_entry(entries, 323), 323).at(0);
    const auto sample_formats = read_u16_array(data, find_required_entry(entries, 339), 339);
    validate_common_spec(result, bits_per_sample, sample_formats, path);
    initialize_read_pixels(result);

    const auto tile_offsets = read_u32_array(data, find_required_entry(entries, 324), 324);
    const auto tile_byte_counts = read_u32_array(data, find_required_entry(entries, 325), 325);
    if (tile_offsets.size() != tile_byte_counts.size()) {
        throw std::runtime_error("TIFF tile offset/count mismatch: " + path.string());
    }
    const std::uint32_t tiles_x{(result.spec.width + result.spec.tile_width - 1U) / result.spec.tile_width};
    const std::uint32_t tiles_y{(result.spec.height + result.spec.tile_height - 1U) / result.spec.tile_height};
    if (tile_offsets.size() != static_cast<std::size_t>(tiles_x * tiles_y)) {
        throw std::runtime_error("TIFF tile count does not match dimensions: " + path.string());
    }

    for (std::uint32_t tile_y{0}; tile_y < tiles_y; ++tile_y) {
        for (std::uint32_t tile_x{0}; tile_x < tiles_x; ++tile_x) {
            const std::uint32_t tile_index{tile_y * tiles_x + tile_x};
            const std::size_t tile_offset{tile_offsets.at(tile_index)};
            const std::size_t tile_bytes{tile_byte_counts.at(tile_index)};
            if (tile_offset + tile_bytes > data.size()) {
                throw std::runtime_error("TIFF tile data outside file: " + path.string());
            }
            const std::size_t expected_tile_bytes =
                static_cast<std::size_t>(result.spec.tile_width) * result.spec.tile_height
                * result.spec.samples_per_pixel;
            if (result.spec.compression_mode == TiffCompressionMode::None
                && tile_bytes != expected_tile_bytes) {
                throw std::runtime_error("TIFF tile byte count does not match tile dimensions: " + path.string());
            }
            std::vector<std::uint8_t> decoded_tile;
            std::span<const std::uint8_t> tile_payload{
                data.data() + tile_offset,
                tile_bytes};
            if (result.spec.compression_mode == TiffCompressionMode::PackBits)
            {
                decoded_tile = DecodePackBitsBlock(
                    tile_payload,
                    expected_tile_bytes,
                    path);
                tile_payload = decoded_tile;
            }
            for (std::uint32_t y{0}; y < result.spec.tile_height; ++y) {
                const std::uint32_t image_y{tile_y * result.spec.tile_height + y};
                for (std::uint32_t x{0}; x < result.spec.tile_width; ++x) {
                    const std::uint32_t image_x{tile_x * result.spec.tile_width + x};
                    for (std::uint16_t c{0}; c < result.spec.samples_per_pixel; ++c) {
                        const std::size_t value_offset =
                            (static_cast<std::size_t>(y) * result.spec.tile_width + x)
                                * result.spec.samples_per_pixel
                            + c;
                        const std::uint8_t value = tile_payload[value_offset];
                        if (image_y >= result.spec.height || image_x >= result.spec.width) {
                            if (value != 255U) {
                                throw std::runtime_error("TIFF tile padding is not 255: " + path.string());
                            }
                            continue;
                        }
                        const std::size_t pixel_index =
                            (static_cast<std::size_t>(image_y) * result.spec.width + image_x)
                                * result.spec.samples_per_pixel
                            + c;
                        result.pixels.at(pixel_index) = value;
                        update_channel_stats(result, c, value);
                    }
                }
            }
        }
    }

    return result;
}

TiffReadResult read_rgbwsv_stripped_tiff(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("failed to open TIFF for reading: " + path.string());
    }
    const std::vector<std::uint8_t> data{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const ParsedEntries entries = parse_ifd_entries(data, path);

    TiffReadResult result;
    result.spec.storage_mode = TiffStorageMode::Stripped;
    result.spec.width = read_u32_array(data, find_required_entry(entries, 256), 256).at(0);
    result.spec.height = read_u32_array(data, find_required_entry(entries, 257), 257).at(0);
    result.spec.compression_mode =
        ReadTiffCompressionMode(data, entries, path);
    const auto bits_per_sample = read_u16_array(data, find_required_entry(entries, 258), 258);
    result.spec.samples_per_pixel = read_u16_array(data, find_required_entry(entries, 277), 277).at(0);
    result.spec.rows_per_strip = read_u32_array(data, find_required_entry(entries, 278), 278).at(0);
    result.spec.planar_config = read_u16_array(data, find_required_entry(entries, 284), 284).at(0);
    const auto sample_formats = read_u16_array(data, find_required_entry(entries, 339), 339);
    validate_common_spec(result, bits_per_sample, sample_formats, path);
    initialize_read_pixels(result);

    if (result.spec.rows_per_strip == 0) {
        throw std::runtime_error("TIFF rowsPerStrip is invalid: " + path.string());
    }
    const auto strip_offsets = read_u32_array(data, find_required_entry(entries, 273), 273);
    const auto strip_byte_counts = read_u32_array(data, find_required_entry(entries, 279), 279);
    if (strip_offsets.size() != strip_byte_counts.size()) {
        throw std::runtime_error("TIFF strip offset/count mismatch: " + path.string());
    }
    const std::uint32_t expected_strip_count{
        (result.spec.height + result.spec.rows_per_strip - 1U) / result.spec.rows_per_strip};
    if (strip_offsets.size() != static_cast<std::size_t>(expected_strip_count)) {
        throw std::runtime_error("TIFF strip count does not match dimensions: " + path.string());
    }

    for (std::uint32_t strip_index{0}; strip_index < expected_strip_count; ++strip_index) {
        const std::uint32_t start_row{strip_index * result.spec.rows_per_strip};
        const std::uint32_t rows{
            std::min(result.spec.rows_per_strip, static_cast<std::uint32_t>(result.spec.height - start_row))};
        const std::size_t expected_strip_bytes =
            static_cast<std::size_t>(rows) * result.spec.width * result.spec.samples_per_pixel;
        const std::size_t strip_offset{strip_offsets.at(strip_index)};
        const std::size_t strip_bytes{strip_byte_counts.at(strip_index)};
        if (strip_offset + strip_bytes > data.size()) {
            throw std::runtime_error("TIFF strip data outside file: " + path.string());
        }
        if (result.spec.compression_mode == TiffCompressionMode::None
            && strip_bytes != expected_strip_bytes) {
            throw std::runtime_error("TIFF strip byte count does not match dimensions: " + path.string());
        }
        std::vector<std::uint8_t> decoded_strip;
        std::span<const std::uint8_t> strip_payload{
            data.data() + strip_offset,
            strip_bytes};
        if (result.spec.compression_mode == TiffCompressionMode::PackBits)
        {
            decoded_strip = DecodePackBitsBlock(
                strip_payload,
                expected_strip_bytes,
                path);
            strip_payload = decoded_strip;
        }
        for (std::uint32_t y{0}; y < rows; ++y) {
            const std::uint32_t image_y{start_row + y};
            for (std::uint32_t x{0}; x < result.spec.width; ++x) {
                for (std::uint16_t c{0}; c < result.spec.samples_per_pixel; ++c) {
                    const std::size_t value_offset =
                        (static_cast<std::size_t>(y) * result.spec.width + x)
                            * result.spec.samples_per_pixel
                        + c;
                    const std::uint8_t value = strip_payload[value_offset];
                    const std::size_t pixel_index =
                        (static_cast<std::size_t>(image_y) * result.spec.width + x) * result.spec.samples_per_pixel
                        + c;
                    result.pixels.at(pixel_index) = value;
                    update_channel_stats(result, c, value);
                }
            }
        }
    }

    return result;
}

TiffReadResult read_rgbwsv_tiff(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("failed to open TIFF for reading: " + path.string());
    }
    const std::vector<std::uint8_t> data{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const ParsedEntries entries = parse_ifd_entries(data, path);
    const bool has_tiles = find_optional_entry(entries, 324).has_value() || find_optional_entry(entries, 325).has_value();
    const bool has_strips =
        find_optional_entry(entries, 273).has_value() || find_optional_entry(entries, 279).has_value();
    if (has_tiles && !has_strips) {
        return read_rgbwsv_tiled_tiff(path);
    }
    if (has_strips && !has_tiles) {
        return read_rgbwsv_stripped_tiff(path);
    }
    throw std::runtime_error("TIFF storage structure is ambiguous or missing: " + path.string());
}

}  // namespace slicer_core
