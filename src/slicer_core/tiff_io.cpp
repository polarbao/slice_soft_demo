#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <fstream>
#include <iterator>
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
    if (entry.count == 1) {
        result.push_back(read_inline_u16(entry));
        return result;
    }
    const std::size_t byte_count = static_cast<std::size_t>(entry.count) * type_size(entry.type);
    if (static_cast<std::size_t>(entry.value_or_offset) + byte_count > data.size()) {
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
    if (entry.type != TiffType::long_value) {
        throw std::runtime_error("TIFF tag has unexpected type: " + std::to_string(tag));
    }
    std::vector<std::uint32_t> result;
    result.reserve(entry.count);
    if (entry.count == 1) {
        result.push_back(entry.value_or_offset);
        return result;
    }
    const std::size_t byte_count = static_cast<std::size_t>(entry.count) * type_size(entry.type);
    if (static_cast<std::size_t>(entry.value_or_offset) + byte_count > data.size()) {
        throw std::runtime_error("TIFF tag data outside file: " + std::to_string(tag));
    }
    for (std::uint32_t i{0}; i < entry.count; ++i) {
        result.push_back(read_u32(data, static_cast<std::size_t>(entry.value_or_offset) + i * 4U));
    }
    return result;
}

}  // namespace

void write_rgbwsv_tiled_tiff(
    const std::filesystem::path& path,
    const TiffImageSpec& spec,
    const std::vector<std::uint8_t>& pixels) {
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
    tile_data.resize(static_cast<std::size_t>(tile_count) * tile_byte_count, 255);
    for (std::uint32_t tile_y{0}; tile_y < tiles_y; ++tile_y) {
        for (std::uint32_t tile_x{0}; tile_x < tiles_x; ++tile_x) {
            const std::uint32_t tile_index{tile_y * tiles_x + tile_x};
            const std::size_t tile_base{static_cast<std::size_t>(tile_index) * tile_byte_count};
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
                        const std::uint8_t value{pixels.at(source_index)};
                        const std::size_t target_index =
                            tile_base
                            + ((static_cast<std::size_t>(y) * spec.tile_width + x) * spec.samples_per_pixel + c);
                        tile_data.at(target_index) = value;
                    }
                }
            }
        }
    }

    std::vector<std::uint32_t> tile_offsets;
    std::vector<std::uint32_t> tile_byte_counts;
    tile_offsets.reserve(tile_count);
    tile_byte_counts.reserve(tile_count);
    constexpr std::uint32_t header_size{8};
    for (std::uint32_t i{0}; i < tile_count; ++i) {
        tile_offsets.push_back(header_size + i * tile_byte_count);
        tile_byte_counts.push_back(tile_byte_count);
    }

    const std::uint32_t ifd_offset = header_size + static_cast<std::uint32_t>(tile_data.size());
    std::vector<IfdEntry> entries{
        {256, TiffType::long_value, 1, longs({spec.width})},
        {257, TiffType::long_value, 1, longs({spec.height})},
        {258, TiffType::short_value, spec.samples_per_pixel, shorts({8, 8, 8, 8, 8, 8})},
        {259, TiffType::short_value, 1, shorts({1})},
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

TiffReadResult read_rgbwsv_tiled_tiff(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("failed to open TIFF for reading: " + path.string());
    }
    const std::vector<std::uint8_t> data{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (data.size() < 8 || data.at(0) != 'I' || data.at(1) != 'I' || read_u16(data, 2) != 42) {
        throw std::runtime_error("unsupported or invalid TIFF header: " + path.string());
    }

    const std::uint32_t ifd_offset{read_u32(data, 4)};
    if (static_cast<std::size_t>(ifd_offset) + 2 > data.size()) {
        throw std::runtime_error("TIFF IFD offset outside file: " + path.string());
    }
    const std::uint16_t entry_count{read_u16(data, ifd_offset)};
    std::vector<std::pair<std::uint16_t, ParsedEntry>> entries;
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

    const auto find_entry = [&](const std::uint16_t tag) -> ParsedEntry {
        const auto found = std::find_if(entries.begin(), entries.end(), [tag](const auto& item) {
            return item.first == tag;
        });
        if (found == entries.end()) {
            throw std::runtime_error("missing required TIFF tag: " + std::to_string(tag));
        }
        return found->second;
    };

    TiffReadResult result;
    result.spec.width = read_u32_array(data, find_entry(256), 256).at(0);
    result.spec.height = read_u32_array(data, find_entry(257), 257).at(0);
    const auto bits_per_sample = read_u16_array(data, find_entry(258), 258);
    result.spec.samples_per_pixel = read_u16_array(data, find_entry(277), 277).at(0);
    result.spec.planar_config = read_u16_array(data, find_entry(284), 284).at(0);
    result.spec.tile_width = read_u32_array(data, find_entry(322), 322).at(0);
    result.spec.tile_height = read_u32_array(data, find_entry(323), 323).at(0);
    const auto sample_formats = read_u16_array(data, find_entry(339), 339);

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

    const auto tile_offsets = read_u32_array(data, find_entry(324), 324);
    const auto tile_byte_counts = read_u32_array(data, find_entry(325), 325);
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
            for (std::uint32_t y{0}; y < result.spec.tile_height; ++y) {
                const std::uint32_t image_y{tile_y * result.spec.tile_height + y};
                for (std::uint32_t x{0}; x < result.spec.tile_width; ++x) {
                    const std::uint32_t image_x{tile_x * result.spec.tile_width + x};
                    for (std::uint16_t c{0}; c < result.spec.samples_per_pixel; ++c) {
                        const std::size_t value_offset =
                            tile_offset
                            + ((static_cast<std::size_t>(y) * result.spec.tile_width + x)
                               * result.spec.samples_per_pixel
                               + c);
                        const std::uint8_t value = data.at(value_offset);
                        if (image_y >= result.spec.height || image_x >= result.spec.width) {
                            if (value != 255U) {
                                throw std::runtime_error("TIFF tile padding is not 255: " + path.string());
                            }
                            continue;
                        }
                        result.channel_checksums.at(c) += value;
                        TiffChannelStats& stats = result.channel_stats.at(c);
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
                }
            }
        }
    }

    return result;
}

}  // namespace slicer_core
