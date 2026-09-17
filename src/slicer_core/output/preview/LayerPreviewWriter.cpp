// 层预览写出：从 slicer.cpp 搬出（F-09 第 1 步）。
//
// 本次搬运是「只移位、不改行为」：函数体逐字节搬过来，判据是字节级基线逐产物全等。
//
// 排列顺序的缘由：`build_preview_image`（私有）调用
// `canonical_preview_channel`（公开），所以不能把私有函数整体前置。
// 解法是先包含本单元的头文件，4 个公开入口的声明在匿名命名空间之前即可见，
// 于是 13 个私有函数得以保留内部链接、且无需重排。
//
// 注：仓库内已有第二套功能等价的 PNG/PPM 编码器
// （`output/rgbwsv/RgbwsvPreviewIo.h` + `RgbwsvPackageWriter.cpp`，8 个函数一一对应）。
// 合并属行为变更，不在本步做；见 analysis/04 的 F-47。

#include "slicer_core/output/preview/LayerPreviewWriter.h"

#include "slicer_core/TiffReadApi.h"                    // rgbwsv_channel_count
#include "slicer_core/output/rgbwsvt/RgbwsvtProtocol.h"  // kRgbwsvtChannelCount

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <ios>
#include <sstream>
#include <stdexcept>

namespace slicer_core::preview {

namespace {

struct PreviewImage {
    std::string channel;
    std::string type;
    std::string prefix;
    std::vector<std::array<std::uint8_t, 3>> pixels;
    int non_zero_pixels{0};
    int max_value{0};
};

std::string preview_extension(const std::string& format) {
    return format == "png" ? "png" : "ppm";
}

std::string preview_file_name(const std::string& prefix, const int layer_index, const std::string& format) {
    std::ostringstream stream;
    stream << "preview/" << prefix << "_" << std::setw(6) << std::setfill('0') << layer_index << "."
           << preview_extension(format);
    return stream.str();
}

std::uint8_t visible_from_print_value(const std::uint8_t value) {
    return static_cast<std::uint8_t>(255U - value);
}

void write_ppm(
    const std::filesystem::path& path,
    const GridSpec& grid,
    const std::vector<std::array<std::uint8_t, 3>>& rgb_pixels) {
    if (rgb_pixels.size() != static_cast<std::size_t>(grid.width_px) * grid.height_px) {
        throw std::runtime_error("preview pixel buffer size does not match grid");
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    if (!output) {
        throw std::runtime_error("failed to write preview image: " + path.string());
    }
    output << "P6\n" << grid.width_px << ' ' << grid.height_px << "\n255\n";
    for (int y = grid.height_px - 1; y >= 0; --y) {
        output.write(reinterpret_cast<const char*>(rgb_pixels.data() + static_cast<std::size_t>(y) * grid.width_px), static_cast<std::streamsize>(grid.width_px) * 3);
    }
}

void append_be_u32(std::vector<std::uint8_t>& data, const std::uint32_t value) {
    data.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    data.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    data.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t byte) {
    crc ^= byte;
    for (int i{0}; i < 8; ++i) {
        const std::uint32_t mask = 0U - (crc & 1U);
        crc = (crc >> 1U) ^ (0xedb88320U & mask);
    }
    return crc;
}

std::uint32_t crc32_chunk(const std::array<char, 4>& type, const std::vector<std::uint8_t>& payload) {
    std::uint32_t crc{0xffffffffU};
    for (const char c : type) {
        crc = crc32_update(crc, static_cast<std::uint8_t>(c));
    }
    for (const std::uint8_t byte : payload) {
        crc = crc32_update(crc, byte);
    }
    return crc ^ 0xffffffffU;
}

std::uint32_t adler32(const std::vector<std::uint8_t>& data) {
    std::uint32_t a{1};
    std::uint32_t b{0};
    for (const std::uint8_t byte : data) {
        a = (a + byte) % 65521U;
        b = (b + a) % 65521U;
    }
    return (b << 16U) | a;
}

std::vector<std::uint8_t> zlib_store_blocks(const std::vector<std::uint8_t>& raw) {
    std::vector<std::uint8_t> result;
    result.reserve(raw.size() + raw.size() / 65535U * 5U + 16U);
    result.push_back(0x78U);
    result.push_back(0x01U);
    std::size_t offset{0};
    while (offset < raw.size()) {
        const std::size_t block_size = std::min<std::size_t>(65535U, raw.size() - offset);
        const bool final_block = offset + block_size == raw.size();
        result.push_back(final_block ? 0x01U : 0x00U);
        const auto len = static_cast<std::uint16_t>(block_size);
        const auto nlen = static_cast<std::uint16_t>(~len);
        result.push_back(static_cast<std::uint8_t>(len & 0xffU));
        result.push_back(static_cast<std::uint8_t>((len >> 8U) & 0xffU));
        result.push_back(static_cast<std::uint8_t>(nlen & 0xffU));
        result.push_back(static_cast<std::uint8_t>((nlen >> 8U) & 0xffU));
        result.insert(result.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                      raw.begin() + static_cast<std::ptrdiff_t>(offset + block_size));
        offset += block_size;
    }
    append_be_u32(result, adler32(raw));
    return result;
}

void write_png_chunk(
    std::ofstream& output,
    const std::array<char, 4>& type,
    const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> length;
    append_be_u32(length, static_cast<std::uint32_t>(payload.size()));
    output.write(reinterpret_cast<const char*>(length.data()), static_cast<std::streamsize>(length.size()));
    output.write(type.data(), static_cast<std::streamsize>(type.size()));
    if (!payload.empty()) {
        output.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    }
    std::vector<std::uint8_t> crc;
    append_be_u32(crc, crc32_chunk(type, payload));
    output.write(reinterpret_cast<const char*>(crc.data()), static_cast<std::streamsize>(crc.size()));
}

void write_png(
    const std::filesystem::path& path,
    const GridSpec& grid,
    const std::vector<std::array<std::uint8_t, 3>>& rgb_pixels) {
    if (rgb_pixels.size() != static_cast<std::size_t>(grid.width_px) * grid.height_px) {
        throw std::runtime_error("preview pixel buffer size does not match grid");
    }
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(grid.height_px) * (static_cast<std::size_t>(grid.width_px) * 3U + 1U));
    for (int y{0}; y < grid.height_px; ++y) {
        raw.push_back(0U);
        for (int x{0}; x < grid.width_px; ++x) {
            const auto& pixel = rgb_pixels.at(static_cast<std::size_t>(grid.height_px - 1 - y) * grid.width_px + x);
            raw.insert(raw.end(), pixel.begin(), pixel.end());
        }
    }

    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    if (!output) {
        throw std::runtime_error("failed to write preview image: " + path.string());
    }
    const std::array<std::uint8_t, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    output.write(reinterpret_cast<const char*>(signature.data()), static_cast<std::streamsize>(signature.size()));

    std::vector<std::uint8_t> ihdr;
    append_be_u32(ihdr, static_cast<std::uint32_t>(grid.width_px));
    append_be_u32(ihdr, static_cast<std::uint32_t>(grid.height_px));
    ihdr.push_back(8U);
    ihdr.push_back(2U);
    ihdr.push_back(0U);
    ihdr.push_back(0U);
    ihdr.push_back(0U);
    write_png_chunk(output, {'I', 'H', 'D', 'R'}, ihdr);
    write_png_chunk(output, {'I', 'D', 'A', 'T'}, zlib_store_blocks(raw));
    write_png_chunk(output, {'I', 'E', 'N', 'D'}, {});
}

void write_preview_image(
    const std::filesystem::path& path,
    const GridSpec& grid,
    const std::vector<std::array<std::uint8_t, 3>>& rgb_pixels,
    const std::string& format) {
    if (format == "png") {
        write_png(path, grid, rgb_pixels);
        return;
    }
    write_ppm(path, grid, rgb_pixels);
}

PreviewImage build_preview_image(
    const std::string& requested_channel,
    const GridSpec& grid,
    const PreviewConfig& preview_config,
    const std::vector<std::uint8_t>& layer,
    const std::vector<std::uint8_t>* texture_preview_mask,
    const bool transfer_enabled) {
    const std::string channel = canonical_preview_channel(requested_channel);
    PreviewImage image;
    image.channel = channel;
    image.type = channel;
    image.prefix = channel;
    image.pixels.resize(static_cast<std::size_t>(grid.width_px) * grid.height_px);
    if (channel == "rgb") {
        image.type = "model_rgb";
        image.prefix = "model_rgb";
    } else if (channel == "texture_rgb") {
        image.type = "texture_rgb";
        image.prefix = "texture_rgb";
    } else if (channel == "support") {
        image.type = "support_s";
        image.prefix = "support_s";
    } else if (channel == "white") {
        image.type = "white_w";
        image.prefix = "white_w";
    } else if (channel == "varnish") {
        image.type = "varnish_v";
        image.prefix = "varnish_v";
    } else if (channel == "transfer") {
        image.type = "transfer_t";
        image.prefix = "transfer_t";
    }

    for (std::size_t i{0}; i < image.pixels.size(); ++i) {
        const std::size_t base{i * (transfer_enabled ? kRgbwsvtChannelCount : rgbwsv_channel_count)};
        std::array<std::uint8_t, 3> pixel{};
        int display_value{0};
        if (channel == "rgb") {
            const bool has_rgb_print =
                layer.at(base + 0U) < 255U || layer.at(base + 1U) < 255U || layer.at(base + 2U) < 255U;
            if (has_rgb_print) {
                pixel = {layer.at(base + 0U), layer.at(base + 1U), layer.at(base + 2U)};
                display_value = 255;
            } else {
                pixel = preview_config.empty_color;
            }
        } else if (channel == "texture_rgb") {
            const bool has_texture_preview_pixel =
                texture_preview_mask == nullptr ? (layer.at(base + 0U) < 255U || layer.at(base + 1U) < 255U
                                                   || layer.at(base + 2U) < 255U)
                                                : texture_preview_mask->at(i) != 0;
            if (has_texture_preview_pixel) {
                pixel = {layer.at(base + 0U), layer.at(base + 1U), layer.at(base + 2U)};
                display_value = 255;
            } else {
                pixel = preview_config.empty_color;
            }
        } else if (channel == "support" || channel == "white" || channel == "varnish"
                   || channel == "transfer") {
            const std::size_t channel_offset =
                channel == "support" ? 4U : (channel == "white" ? 3U : (channel == "varnish" ? 5U : 6U));
            const auto& print_color =
                channel == "support"
                    ? preview_config.support_color
                    : (channel == "white" ? preview_config.white_color
                       : (channel == "varnish" ? preview_config.varnish_color : preview_config.transfer_color));
            const std::uint8_t visibility{visible_from_print_value(layer.at(base + channel_offset))};
            display_value = visibility;
            for (std::size_t c{0}; c < pixel.size(); ++c) {
                const int empty_component{preview_config.empty_color.at(c)};
                const int print_component{print_color.at(c)};
                pixel.at(c) = static_cast<std::uint8_t>(
                    (empty_component * (255 - visibility) + print_component * visibility + 127) / 255);
            }
        }
        if (display_value > 0) {
            ++image.non_zero_pixels;
        }
        image.max_value = std::max(image.max_value, display_value);
        image.pixels.at(i) = pixel;
    }
    return image;
}

}  // namespace

std::string layer_file_name(const int layer_index) {
    std::ostringstream stream;
    stream << "layers/layer_" << std::setw(6) << std::setfill('0') << layer_index << ".tiff";
    return stream.str();
}

std::string canonical_preview_channel(const std::string& channel) {
    if (channel == "model_rgb") {
        return "rgb";
    }
    if (channel == "model_rgb_true_color" || channel == "true_rgb") {
        return "texture_rgb";
    }
    if (channel == "s") {
        return "support";
    }
    if (channel == "w") {
        return "white";
    }
    if (channel == "v") {
        return "varnish";
    }
    if (channel == "t") {
        return "transfer";
    }
    return channel;
}

Json::Array write_layer_previews(
    const PreviewConfig& preview_config,
    const std::filesystem::path& package_dir,
    const GridSpec& grid,
    const int layer_index,
    const std::vector<std::uint8_t>& layer,
    const std::vector<std::uint8_t>* texture_preview_mask,
    const bool transfer_enabled) {
    Json::Array result;
    for (const std::string& requested_channel : preview_config.channels) {
        PreviewImage image = build_preview_image(
            requested_channel,
            grid,
            preview_config,
            layer,
            texture_preview_mask,
            transfer_enabled);
        if (preview_config.only_non_empty_layers && image.non_zero_pixels == 0) {
            continue;
        }
        const std::string relative_path = preview_file_name(image.prefix, layer_index, preview_config.format);
        write_preview_image(package_dir / relative_path, grid, image.pixels, preview_config.format);
        result.push_back(Json::object({
            {"layerIndex", layer_index},
            {"channel", image.channel},
            {"type", image.type},
            {"kind", "single"},
            {"format", preview_config.format},
            {"path", relative_path},
            {"printPixels", image.non_zero_pixels},
            {"displayNonZeroPixels", image.non_zero_pixels},
            {"nonZeroPixels", image.non_zero_pixels},
            {"maxValue", image.max_value},
        }));
    }
    return result;
}

bool should_write_preview(const PreviewConfig& preview, const int layer_index, const int layer_count) {
    if (!preview.enabled) {
        return false;
    }
    if (preview.has_layer_range
        && (layer_index < preview.layer_range.at(0) || layer_index > preview.layer_range.at(1))) {
        return false;
    }
    const int interval{std::max(1, preview.interval)};
    return layer_index == 0 || layer_index + 1 == layer_count || (layer_index % interval) == 0;
}

}  // namespace slicer_core::preview
