#include "slicer_core/slicer.h"

#include "slicer_core/diagnostics/MaterialClosureCandidateDetector.h"
#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/json_value.h"
#include "slicer_core/material/MaterialClosureRepair.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionAdmission.h"
#include "slicer_core/model.h"
#include "slicer_core/reports/MaterialClosureReport.h"
#include "slicer_core/reports/ReportBase.h"
#include "slicer_core/support/SupportShapePipeline.h"
#include "slicer_core/support/SupportShapePolicy.h"
#include "slicer_core/support/SupportShapeReport.h"
#include "slicer_core/texture_image.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace slicer_core {
namespace {

constexpr double mm_per_inch{25.4};

using SlicerClock = std::chrono::steady_clock;

double ElapsedMsSince(const SlicerClock::time_point& start)
{
    return std::chrono::duration<double, std::milli>(SlicerClock::now() - start).count();
}

void NotifyProgress(
    const SliceRunOptions& options,
    const SlicerClock::time_point& runStart,
    const std::string& phase,
    const int current,
    const int total,
    const int percent)
{
    if (!options.progress_callback)
    {
        return;
    }

    options.progress_callback(SliceRunProgress{
        phase,
        current,
        total,
        std::clamp(percent, 0, 100),
        ElapsedMsSince(runStart)});
}

bool ShouldNotifyLayerProgress(const int completedLayers, const int layerCount)
{
    if (completedLayers <= 1 || completedLayers >= layerCount)
    {
        return true;
    }

    const int interval = std::max(1, (layerCount + 99) / 100);
    return completedLayers % interval == 0;
}

struct GridSpec {
    int width_px{0};
    int height_px{0};
    int layer_count{0};
    double pixel_size_x_mm{0.0};
    double pixel_size_y_mm{0.0};
    double origin_x_mm{0.0};
    double origin_y_mm{0.0};
};

struct Segment2 {
    double x0{0.0};
    double y0{0.0};
    double x1{0.0};
    double y1{0.0};
};

struct RasterResult {
    std::vector<std::uint8_t> mask;
    int odd_intersection_rows{0};
    int filled_spans{0};
};

struct ChannelStats {
    std::uint64_t print_pixels{0};
    std::uint64_t full_print_pixels{0};
    std::uint64_t partial_print_pixels{0};
    std::uint64_t empty_pixels{0};
    int min_value{255};
    int max_value{0};
};

struct SupportComponentSummary {
    int area_px{0};
    int min_x{0};
    int min_y{0};
    int max_x{0};
    int max_y{0};
};

struct SupportConnectivityDiagnostics {
    bool enabled{false};
    int component_count{0};
    int largest_component_pixels{0};
    int small_component_count{0};
    int tiny_component_count{0};
    std::vector<SupportComponentSummary> components;
};

struct LayerSemanticStats {
    int texture_surface_pixels{0};
    int model_fill_pixels{0};
    int support_pixels{0};
    int internal_void_support_pixels{0};
    int outer_varnish_pixels{0};
    int outer_surface_varnish_pixels{0};
    int inner_surface_varnish_pixels{0};
};

struct LayerDiagnostics {
    int layer_index{0};
    double z_mm{0.0};
    int segment_count{0};
    int odd_intersection_rows{0};
    int filled_spans{0};
    int model_pixels{0};
    int support_pixels{0};
    int rgb_non_zero_pixels{0};
    int white_non_zero_pixels{0};
    int support_non_zero_pixels{0};
    int varnish_non_zero_pixels{0};
    int island_count{0};
    int island_pixels{0};
    int unsupported_pixels{0};
    int filtered_island_count{0};
    int filtered_island_pixels{0};
    int bottom_projection_support_pixels{0};
    int unsupported_island_support_pixels{0};
    int full_vertical_projection_support_pixels{0};
    int internal_void_support_pixels{0};
    int upper_projection_support_pixels{0};
    SupportConnectivityDiagnostics support_connectivity;
    LayerSemanticStats semantic;
    std::array<ChannelStats, rgbwsv_channel_count> channel_stats{};
};

struct PreviewImage {
    std::string channel;
    std::string type;
    std::string prefix;
    std::vector<std::array<std::uint8_t, 3>> pixels;
    int non_zero_pixels{0};
    int max_value{0};
};

struct ReliefReportData {
    int total_columns{0};
    int hit_columns{0};
    int empty_columns{0};
    int multi_hit_columns{0};
    double z_min_mm{0.0};
    double z_max_mm{0.0};
    double thickness_min_mm{0.0};
    double thickness_max_mm{0.0};
    bool has_hits{false};
    Json::Array warnings;
};

struct ReliefColumnInfo {
    bool has_model{false};
    int lower_layer{-1};
    int upper_layer{-1};
    double z_min_mm{0.0};
    double z_max_mm{0.0};
    int hit_count{0};
    bool multi_hit{false};
    int top_triangle_index{-1};
    std::array<double, 3> top_barycentric{0.0, 0.0, 0.0};
};

struct ReliefSamplingResult {
    std::vector<std::vector<std::uint8_t>> model_masks;
    std::vector<ReliefColumnInfo> columns;
    ReliefReportData report;
};

enum class SupportType : std::uint8_t {
    None = 0,
    BottomProjection = 1,
    UnsupportedIsland = 2,
    FullVerticalProjection = 3,
    InternalVoid = 4,
    UpperProjection = 5,
};

struct SupportPlacementPolicy {
    std::string requested_placement{"lower"};
    std::string effective_placement{"lower"};
    bool placement_explicit{false};
    bool lower_enabled{true};
    bool upper_enabled{false};
    bool unsupported_only_enabled{false};
    bool full_vertical_projection_enabled{false};
    bool advanced_debug{false};
};

struct IslandComponent {
    int layer_index{0};
    int component_id{0};
    int area_px{0};
    int overlap_px{0};
    double overlap_ratio{0.0};
    bool filtered{false};
    std::vector<int> pixels;
};

struct SupportGenerationResult {
    std::vector<std::vector<std::uint8_t>> support_masks;
    std::vector<std::vector<SupportType>> support_type_maps;
    int support_pixels{0};
    int island_count{0};
    int island_pixels{0};
    int unsupported_pixels{0};
    int filtered_island_count{0};
    int filtered_island_pixels{0};
    int bottom_projection_support_pixels{0};
    int unsupported_island_support_pixels{0};
    int full_vertical_projection_support_pixels{0};
    int internal_void_support_pixels{0};
    int upper_projection_support_pixels{0};
    int layers_with_islands{0};
    int layers_with_support{0};
};

struct UpperSupportBoundaryInfo
{
    bool includes_outer_varnish_shell{false};
    std::string source{"model_envelope"};
};

struct SurfaceVarnishMasks {
    std::vector<std::vector<std::uint8_t>> outer_surface_masks;
    std::vector<std::vector<std::uint8_t>> inner_surface_masks;
};

struct TextureColumnColor {
    bool has_color{false};
    std::array<std::uint8_t, 3> rgb{0, 0, 0};
    bool sampled_texture{false};
    bool used_fallback{false};
    bool uv_out_of_range{false};
};

struct TextureReportData {
    bool enabled{false};
    std::string apply_mode;
    std::string non_surface_rgb_policy{"model_material"};
    int top_surface_layers{1};
    std::string source{"filesystem"};
    int faces_with_uv{0};
    int faces_without_uv{0};
    std::uint64_t sampled_pixels{0};
    std::uint64_t fallback_pixels{0};
    std::uint64_t uv_out_of_range_pixels{0};
    int loaded_textures{0};
    int missing_textures{0};
    Json::Array materials;
    Json::Array texture_files;
    Json::Array warnings;
};

struct ColumnLayerRange {
    bool has_model{false};
    int lower_layer{-1};
    int upper_layer{-1};
};

struct MaterialPixel {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t w{255};
    std::uint8_t v{255};
};

enum class ModelFillMaterial
{
    Rgb,
    White,
    Varnish,
    None,
};

struct MaterialPolicyReportData {
    bool enabled{false};
    std::uint64_t rgb_print_pixels{0};
    std::uint64_t white_print_pixels{0};
    std::uint64_t varnish_print_pixels{0};
    Json::Array warnings;
};

enum class MaterialRole {
    Rgb,
    White,
    Varnish,
    Ignore,
    SupportCandidate,
    Support,
};

struct MaterialRoleColumn {
    bool has_role{false};
    MaterialRole role{MaterialRole::Rgb};
    std::array<std::uint8_t, 3> rgb{0, 0, 0};
};

struct MaterialRoleMappingReportData {
    bool enabled{false};
    std::string input_format;
    std::string default_role{"rgb"};
    bool allow_input_support_material{false};
    int material_count{0};
    int mapped_rgb{0};
    int mapped_white{0};
    int mapped_varnish{0};
    int mapped_ignore{0};
    int mapped_support_candidate{0};
    int mapped_support{0};
    int faces_with_mapped_material{0};
    int faces_without_mapped_material{0};
    Json::Array rules;
    Json::Array materials;
    Json::Array warnings;
};

struct RuntimeMaterialTexture {
    MaterialInfo material;
    TextureImage image;
    bool loaded{false};
};

struct TextureRuntime {
    TextureReportData report;
    std::map<std::string, RuntimeMaterialTexture> materials;
};

constexpr std::array<const char*, rgbwsv_channel_count> channel_names{"R", "G", "B", "W", "S", "V"};

std::string layer_file_name(const int layer_index) {
    std::ostringstream stream;
    stream << "layers/layer_" << std::setw(6) << std::setfill('0') << layer_index << ".tiff";
    return stream.str();
}

std::string preview_extension(const std::string& format) {
    return format == "png" ? "png" : "ppm";
}

std::string preview_file_name(const std::string& prefix, const int layer_index, const std::string& format) {
    std::ostringstream stream;
    stream << "preview/" << prefix << "_" << std::setw(6) << std::setfill('0') << layer_index << "."
           << preview_extension(format);
    return stream.str();
}

int ComputeOuterVarnishThicknessPx(const SliceConfig& config)
{
    if (!config.outer_varnish.enabled || config.outer_varnish.thickness_mm <= 0.0)
    {
        return 0;
    }
    return std::max(
        1,
        static_cast<int>(std::ceil(config.outer_varnish.thickness_mm * 1000.0
                                   / config.outer_varnish.pixel_pitch_um)));
}

double ComputeOuterVarnishEffectiveThicknessMm(const SliceConfig& config, const int thicknessPx)
{
    if (thicknessPx <= 0)
    {
        return 0.0;
    }
    return static_cast<double>(thicknessPx) * config.outer_varnish.pixel_pitch_um / 1000.0;
}

GridSpec make_grid_spec(const SliceConfig& config, const BoundingBox& bbox) {
    GridSpec grid;
    grid.pixel_size_x_mm = mm_per_inch / static_cast<double>(config.output.dpi_x);
    grid.pixel_size_y_mm = mm_per_inch / static_cast<double>(config.output.dpi_y);
    const int outerVarnishPaddingPx =
        config.outer_varnish.allow_xy_expansion ? ComputeOuterVarnishThicknessPx(config) : 0;
    grid.origin_x_mm = bbox.min.x - static_cast<double>(outerVarnishPaddingPx) * grid.pixel_size_x_mm;
    grid.origin_y_mm = bbox.min.y - static_cast<double>(outerVarnishPaddingPx) * grid.pixel_size_y_mm;
    const double width_mm{std::max(0.001, bbox.max.x - bbox.min.x)
                          + 2.0 * static_cast<double>(outerVarnishPaddingPx) * grid.pixel_size_x_mm};
    const double height_mm{std::max(0.001, bbox.max.y - bbox.min.y)
                           + 2.0 * static_cast<double>(outerVarnishPaddingPx) * grid.pixel_size_y_mm};
    const double z_max{std::max(config.output.layer_thickness_mm, bbox.max.z + config.support.offset_mm)};
    grid.width_px = std::max(1, static_cast<int>(std::ceil(width_mm / grid.pixel_size_x_mm)));
    grid.height_px = std::max(1, static_cast<int>(std::ceil(height_mm / grid.pixel_size_y_mm)));
    grid.layer_count = std::max(1, static_cast<int>(std::ceil(z_max / config.output.layer_thickness_mm)));
    return grid;
}

Json bbox_to_json(const BoundingBox& bbox) {
    return Json::object({
        {"min", Json::array({bbox.min.x, bbox.min.y, bbox.min.z})},
        {"max", Json::array({bbox.max.x, bbox.max.y, bbox.max.z})},
    });
}

void write_json_file(const std::filesystem::path& path, const Json& value) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path};
    if (!output) {
        throw std::runtime_error("failed to write JSON file: " + path.string());
    }
    output << value.dump(2) << '\n';
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
    for (const auto& pixel : rgb_pixels) {
        output.write(reinterpret_cast<const char*>(pixel.data()), static_cast<std::streamsize>(pixel.size()));
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
            const auto& pixel = rgb_pixels.at(static_cast<std::size_t>(y) * grid.width_px + x);
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
    return channel;
}

PreviewImage build_preview_image(
    const std::string& requested_channel,
    const GridSpec& grid,
    const PreviewConfig& preview_config,
    const std::vector<std::uint8_t>& layer,
    const std::vector<std::uint8_t>* texture_preview_mask) {
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
    }

    for (std::size_t i{0}; i < image.pixels.size(); ++i) {
        const std::size_t base{i * rgbwsv_channel_count};
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
        } else if (channel == "support" || channel == "white" || channel == "varnish") {
            const std::size_t channel_offset =
                channel == "support" ? 4U : (channel == "white" ? 3U : 5U);
            const auto& print_color =
                channel == "support"
                    ? preview_config.support_color
                    : (channel == "white" ? preview_config.white_color : preview_config.varnish_color);
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

Json::Array write_layer_previews(
    const PreviewConfig& preview_config,
    const std::filesystem::path& package_dir,
    const GridSpec& grid,
    const int layer_index,
    const std::vector<std::uint8_t>& layer,
    const std::vector<std::uint8_t>* texture_preview_mask) {
    Json::Array result;
    for (const std::string& requested_channel : preview_config.channels) {
        PreviewImage image = build_preview_image(
            requested_channel,
            grid,
            preview_config,
            layer,
            texture_preview_mask);
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

std::size_t mask_index(const GridSpec& grid, const int x, const int y) {
    return static_cast<std::size_t>(y) * grid.width_px + x;
}

std::vector<std::uint8_t> BuildExternalEmptyMask(
    const GridSpec& grid,
    const std::vector<std::uint8_t>& modelMask)
{
    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::uint8_t> externalEmpty(pixelCount, 0);
    std::vector<int> stack;
    stack.reserve(pixelCount);

    const auto pushExternalEmpty = [&](const int x, const int y)
    {
        if (x < 0 || x >= grid.width_px || y < 0 || y >= grid.height_px)
        {
            return;
        }
        const std::size_t index = mask_index(grid, x, y);
        if (modelMask.at(index) != 0 || externalEmpty.at(index) != 0)
        {
            return;
        }
        externalEmpty.at(index) = 1;
        stack.push_back(static_cast<int>(index));
    };

    for (int x{0}; x < grid.width_px; ++x)
    {
        pushExternalEmpty(x, 0);
        pushExternalEmpty(x, grid.height_px - 1);
    }
    for (int y{0}; y < grid.height_px; ++y)
    {
        pushExternalEmpty(0, y);
        pushExternalEmpty(grid.width_px - 1, y);
    }

    constexpr std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}}, {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};

    while (!stack.empty())
    {
        const int current = stack.back();
        stack.pop_back();
        const int x = current % grid.width_px;
        const int y = current / grid.width_px;
        for (const auto& neighbor : neighbors8)
        {
            pushExternalEmpty(x + neighbor.at(0), y + neighbor.at(1));
        }
    }

    return externalEmpty;
}

std::vector<std::uint8_t> DilateMaskSquare(
    const GridSpec& grid,
    const std::vector<std::uint8_t>& sourceMask,
    const int radiusPx)
{
    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::uint8_t> dilated(pixelCount, 0);
    if (radiusPx <= 0)
    {
        return sourceMask;
    }

    for (int y{0}; y < grid.height_px; ++y)
    {
        for (int x{0}; x < grid.width_px; ++x)
        {
            const std::size_t index = mask_index(grid, x, y);
            if (sourceMask.at(index) == 0)
            {
                continue;
            }
            const int minX = std::max(0, x - radiusPx);
            const int maxX = std::min(grid.width_px - 1, x + radiusPx);
            const int minY = std::max(0, y - radiusPx);
            const int maxY = std::min(grid.height_px - 1, y + radiusPx);
            for (int ny{minY}; ny <= maxY; ++ny)
            {
                for (int nx{minX}; nx <= maxX; ++nx)
                {
                    dilated.at(mask_index(grid, nx, ny)) = 1;
                }
            }
        }
    }

    return dilated;
}

std::vector<std::vector<std::uint8_t>> BuildOuterVarnishMasks(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& modelMasks)
{
    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::vector<std::uint8_t>> outerVarnishMasks(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<std::uint8_t>(pixelCount, 0));
    const int thicknessPx = ComputeOuterVarnishThicknessPx(config);
    if (thicknessPx <= 0)
    {
        return outerVarnishMasks;
    }

    for (int layerIndex{0}; layerIndex < grid.layer_count; ++layerIndex)
    {
        const std::vector<std::uint8_t>& modelMask = modelMasks.at(layerIndex);
        const bool hasModel = std::any_of(modelMask.begin(), modelMask.end(), [](const std::uint8_t value)
        {
            return value != 0;
        });
        if (!hasModel)
        {
            continue;
        }

        const std::vector<std::uint8_t> externalEmpty = BuildExternalEmptyMask(grid, modelMask);
        const std::vector<std::uint8_t> dilatedModel = DilateMaskSquare(grid, modelMask, thicknessPx);
        std::vector<std::uint8_t>& varnishMask = outerVarnishMasks.at(layerIndex);
        for (std::size_t index{0}; index < pixelCount; ++index)
        {
            if (modelMask.at(index) == 0 && externalEmpty.at(index) != 0 && dilatedModel.at(index) != 0)
            {
                varnishMask.at(index) = 1;
            }
        }
    }

    return outerVarnishMasks;
}

UpperSupportBoundaryInfo ResolveUpperSupportBoundaryInfo(const SliceConfig& config)
{
    UpperSupportBoundaryInfo info;
    if (config.support.upper.outside == "outer_varnish_shell"
        && config.outer_varnish.enabled
        && ComputeOuterVarnishThicknessPx(config) > 0)
    {
        info.includes_outer_varnish_shell = true;
        info.source = "model_envelope_plus_outer_varnish_shell";
    }
    return info;
}

std::vector<std::vector<std::uint8_t>> BuildUpperSupportBoundaryMasks(
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    const std::vector<std::vector<std::uint8_t>>& outerVarnishMasks,
    const UpperSupportBoundaryInfo& boundaryInfo)
{
    if (!boundaryInfo.includes_outer_varnish_shell)
    {
        return modelMasks;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::vector<std::uint8_t>> boundaryMasks(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<std::uint8_t>(pixelCount, 0));
    for (int layerIndex{0}; layerIndex < grid.layer_count; ++layerIndex)
    {
        for (std::size_t index{0}; index < pixelCount; ++index)
        {
            if (modelMasks.at(layerIndex).at(index) != 0 || outerVarnishMasks.at(layerIndex).at(index) != 0)
            {
                boundaryMasks.at(layerIndex).at(index) = 1;
            }
        }
    }
    return boundaryMasks;
}

SurfaceVarnishMasks BuildSurfaceVarnishMasks(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& modelMasks)
{
    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    SurfaceVarnishMasks masks;
    masks.outer_surface_masks.resize(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<std::uint8_t>(pixelCount, 0));
    masks.inner_surface_masks.resize(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<std::uint8_t>(pixelCount, 0));
    if (!config.surface_varnish.enabled
        || (!config.surface_varnish.outer_surface && !config.surface_varnish.inner_surface))
    {
        return masks;
    }

    const int radiusPx = std::max(1, config.surface_varnish.thickness_px);
    for (int layerIndex{0}; layerIndex < grid.layer_count; ++layerIndex)
    {
        const std::vector<std::uint8_t>& modelMask = modelMasks.at(layerIndex);
        const bool hasModel = std::any_of(modelMask.begin(), modelMask.end(), [](const std::uint8_t value)
        {
            return value != 0;
        });
        if (!hasModel)
        {
            continue;
        }

        const std::vector<std::uint8_t> externalEmpty = BuildExternalEmptyMask(grid, modelMask);
        for (int y{0}; y < grid.height_px; ++y)
        {
            for (int x{0}; x < grid.width_px; ++x)
            {
                const std::size_t index = mask_index(grid, x, y);
                if (modelMask.at(index) == 0)
                {
                    continue;
                }

                bool touchesExternalEmpty{false};
                bool touchesInternalEmpty{false};
                for (int dy{-radiusPx}; dy <= radiusPx; ++dy)
                {
                    for (int dx{-radiusPx}; dx <= radiusPx; ++dx)
                    {
                        if (dx == 0 && dy == 0)
                        {
                            continue;
                        }
                        const int nx{x + dx};
                        const int ny{y + dy};
                        if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px)
                        {
                            touchesExternalEmpty = true;
                            continue;
                        }
                        const std::size_t neighborIndex = mask_index(grid, nx, ny);
                        if (modelMask.at(neighborIndex) != 0)
                        {
                            continue;
                        }
                        if (externalEmpty.at(neighborIndex) != 0)
                        {
                            touchesExternalEmpty = true;
                        }
                        else
                        {
                            touchesInternalEmpty = true;
                        }
                    }
                }

                if (config.surface_varnish.outer_surface && touchesExternalEmpty)
                {
                    masks.outer_surface_masks.at(layerIndex).at(index) = 1;
                }
                if (config.surface_varnish.inner_surface && touchesInternalEmpty)
                {
                    masks.inner_surface_masks.at(layerIndex).at(index) = 1;
                }
            }
        }
    }

    return masks;
}

int ApplyOuterVarnishSupportPriority(
    const std::vector<std::vector<std::uint8_t>>& outerVarnishMasks,
    SupportGenerationResult& supportGeneration,
    std::vector<std::vector<std::size_t>>* clearedSupportIndices)
{
    if (clearedSupportIndices != nullptr)
    {
        clearedSupportIndices->assign(supportGeneration.support_masks.size(), {});
    }

    int clearedSupportPixels{0};
    for (std::size_t layerIndex{0}; layerIndex < supportGeneration.support_masks.size(); ++layerIndex)
    {
        std::vector<std::uint8_t>& supportMask = supportGeneration.support_masks.at(layerIndex);
        std::vector<SupportType>& supportTypeMap = supportGeneration.support_type_maps.at(layerIndex);
        const std::vector<std::uint8_t>& varnishMask = outerVarnishMasks.at(layerIndex);
        for (std::size_t index{0}; index < supportMask.size(); ++index)
        {
            if (varnishMask.at(index) == 0 || supportMask.at(index) == 0)
            {
                continue;
            }
            supportMask.at(index) = 0;
            supportTypeMap.at(index) = SupportType::None;
            if (clearedSupportIndices != nullptr)
            {
                clearedSupportIndices->at(layerIndex).push_back(index);
            }
            ++clearedSupportPixels;
        }
    }
    return clearedSupportPixels;
}

bool edge_intersects_plane(const Vec3& a, const Vec3& b, const double z_mm) {
    return (a.z <= z_mm && b.z > z_mm) || (b.z <= z_mm && a.z > z_mm);
}

Vec3 interpolate_at_z(const Vec3& a, const Vec3& b, const double z_mm) {
    const double t{(z_mm - a.z) / (b.z - a.z)};
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, z_mm};
}

std::vector<Segment2> slice_triangles_to_segments(const std::vector<Triangle>& triangles, const double z_mm) {
    std::vector<Segment2> segments;
    segments.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        std::vector<Vec3> points;
        points.reserve(2);
        if (edge_intersects_plane(triangle.a, triangle.b, z_mm)) {
            points.push_back(interpolate_at_z(triangle.a, triangle.b, z_mm));
        }
        if (edge_intersects_plane(triangle.b, triangle.c, z_mm)) {
            points.push_back(interpolate_at_z(triangle.b, triangle.c, z_mm));
        }
        if (edge_intersects_plane(triangle.c, triangle.a, z_mm)) {
            points.push_back(interpolate_at_z(triangle.c, triangle.a, z_mm));
        }
        if (points.size() == 2) {
            const double dx{points.at(0).x - points.at(1).x};
            const double dy{points.at(0).y - points.at(1).y};
            if ((dx * dx + dy * dy) > 1.0e-18) {
                segments.push_back({points.at(0).x, points.at(0).y, points.at(1).x, points.at(1).y});
            }
        }
    }
    return segments;
}

void fill_span(std::vector<std::uint8_t>& mask, const GridSpec& grid, const double x0, const double x1, const int y) {
    const double left{std::min(x0, x1)};
    const double right{std::max(x0, x1)};
    int start_x{static_cast<int>(std::ceil((left - grid.origin_x_mm) / grid.pixel_size_x_mm - 0.5))};
    int end_x{static_cast<int>(std::floor((right - grid.origin_x_mm) / grid.pixel_size_x_mm - 0.5))};
    start_x = std::max(0, start_x);
    end_x = std::min(grid.width_px - 1, end_x);
    for (int x{start_x}; x <= end_x; ++x) {
        mask.at(mask_index(grid, x, y)) = 1;
    }
}

RasterResult rasterize_segments(const GridSpec& grid, const std::vector<Segment2>& segments) {
    RasterResult result;
    result.mask.resize(static_cast<std::size_t>(grid.width_px) * grid.height_px, 0);
    std::vector<double> intersections;
    intersections.reserve(segments.size());

    for (int y{0}; y < grid.height_px; ++y) {
        const double y_mm{grid.origin_y_mm + (static_cast<double>(y) + 0.5) * grid.pixel_size_y_mm};
        intersections.clear();
        for (const Segment2& segment : segments) {
            const double min_y{std::min(segment.y0, segment.y1)};
            const double max_y{std::max(segment.y0, segment.y1)};
            if (y_mm < min_y || y_mm >= max_y || std::abs(segment.y1 - segment.y0) < 1.0e-12) {
                continue;
            }
            const double t{(y_mm - segment.y0) / (segment.y1 - segment.y0)};
            intersections.push_back(segment.x0 + (segment.x1 - segment.x0) * t);
        }
        std::sort(intersections.begin(), intersections.end());
        if ((intersections.size() % 2U) != 0U) {
            ++result.odd_intersection_rows;
        }
        for (std::size_t i{0}; i + 1U < intersections.size(); i += 2U) {
            fill_span(result.mask, grid, intersections.at(i), intersections.at(i + 1U), y);
            ++result.filled_spans;
        }
    }
    return result;
}

std::vector<std::vector<std::uint8_t>> sample_model_masks(
    const ModelReport& model_report,
    const GridSpec& grid,
    const double layer_thickness_mm,
    std::vector<LayerDiagnostics>& diagnostics) {
    std::vector<std::vector<std::uint8_t>> masks;
    masks.reserve(grid.layer_count);
    diagnostics.clear();
    diagnostics.reserve(grid.layer_count);
    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index) {
        const double z_mm{(static_cast<double>(layer_index) + 0.5) * layer_thickness_mm};
        LayerDiagnostics layer_diagnostics;
        layer_diagnostics.layer_index = layer_index;
        layer_diagnostics.z_mm = z_mm;
        if (z_mm < model_report.bbox_mm.min.z || z_mm > model_report.bbox_mm.max.z) {
            masks.emplace_back(static_cast<std::size_t>(grid.width_px) * grid.height_px, 0);
            diagnostics.push_back(layer_diagnostics);
            continue;
        }
        const std::vector<Segment2> segments = slice_triangles_to_segments(model_report.triangles, z_mm);
        RasterResult raster = rasterize_segments(grid, segments);
        layer_diagnostics.segment_count = static_cast<int>(segments.size());
        layer_diagnostics.odd_intersection_rows = raster.odd_intersection_rows;
        layer_diagnostics.filled_spans = raster.filled_spans;
        masks.push_back(std::move(raster.mask));
        diagnostics.push_back(layer_diagnostics);
    }
    return masks;
}

bool point_in_triangle_xy(
    const Vec3& p,
    const Triangle& triangle,
    double& w0,
    double& w1,
    double& w2) {
    const double denominator =
        (triangle.b.y - triangle.c.y) * (triangle.a.x - triangle.c.x)
        + (triangle.c.x - triangle.b.x) * (triangle.a.y - triangle.c.y);
    if (std::abs(denominator) < 1.0e-12) {
        return false;
    }
    w0 = ((triangle.b.y - triangle.c.y) * (p.x - triangle.c.x)
          + (triangle.c.x - triangle.b.x) * (p.y - triangle.c.y))
        / denominator;
    w1 = ((triangle.c.y - triangle.a.y) * (p.x - triangle.c.x)
          + (triangle.a.x - triangle.c.x) * (p.y - triangle.c.y))
        / denominator;
    w2 = 1.0 - w0 - w1;
    constexpr double epsilon{-1.0e-9};
    return w0 >= epsilon && w1 >= epsilon && w2 >= epsilon;
}

int first_layer_at_or_above_z(const double z_mm, const double layer_thickness_mm) {
    return static_cast<int>(std::ceil(z_mm / layer_thickness_mm - 0.5));
}

int last_layer_at_or_below_z(const double z_mm, const double layer_thickness_mm) {
    return static_cast<int>(std::floor(z_mm / layer_thickness_mm - 0.5));
}

ReliefSamplingResult sample_relief_heightfield_masks(
    const SliceConfig& config,
    const ModelReport& model_report,
    const GridSpec& grid,
    std::vector<LayerDiagnostics>& diagnostics) {
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<double> z_min(pixel_count, std::numeric_limits<double>::max());
    std::vector<double> z_max(pixel_count, std::numeric_limits<double>::lowest());
    std::vector<int> hit_count(pixel_count, 0);

    ReliefSamplingResult result;
    result.columns.resize(pixel_count);
    ReliefReportData& relief_report = result.report;
    relief_report.total_columns = static_cast<int>(pixel_count);

    for (std::size_t triangle_index{0}; triangle_index < model_report.triangles.size(); ++triangle_index) {
        const Triangle& triangle = model_report.triangles.at(triangle_index);
        const double min_x = std::min({triangle.a.x, triangle.b.x, triangle.c.x});
        const double max_x = std::max({triangle.a.x, triangle.b.x, triangle.c.x});
        const double min_y = std::min({triangle.a.y, triangle.b.y, triangle.c.y});
        const double max_y = std::max({triangle.a.y, triangle.b.y, triangle.c.y});
        int start_x = static_cast<int>(std::floor((min_x - grid.origin_x_mm) / grid.pixel_size_x_mm)) - 1;
        int end_x = static_cast<int>(std::ceil((max_x - grid.origin_x_mm) / grid.pixel_size_x_mm)) + 1;
        int start_y = static_cast<int>(std::floor((min_y - grid.origin_y_mm) / grid.pixel_size_y_mm)) - 1;
        int end_y = static_cast<int>(std::ceil((max_y - grid.origin_y_mm) / grid.pixel_size_y_mm)) + 1;
        start_x = std::max(0, start_x);
        start_y = std::max(0, start_y);
        end_x = std::min(grid.width_px - 1, end_x);
        end_y = std::min(grid.height_px - 1, end_y);
        if (start_x > end_x || start_y > end_y) {
            continue;
        }

        for (int y{start_y}; y <= end_y; ++y) {
            const double y_mm = grid.origin_y_mm + (static_cast<double>(y) + 0.5) * grid.pixel_size_y_mm;
            for (int x{start_x}; x <= end_x; ++x) {
                const double x_mm = grid.origin_x_mm + (static_cast<double>(x) + 0.5) * grid.pixel_size_x_mm;
                double w0{0.0};
                double w1{0.0};
                double w2{0.0};
                if (!point_in_triangle_xy({x_mm, y_mm, 0.0}, triangle, w0, w1, w2)) {
                    continue;
                }
                const double z_mm = w0 * triangle.a.z + w1 * triangle.b.z + w2 * triangle.c.z;
                const std::size_t index = mask_index(grid, x, y);
                z_min.at(index) = std::min(z_min.at(index), z_mm);
                if (z_mm >= z_max.at(index)) {
                    z_max.at(index) = z_mm;
                    ReliefColumnInfo& column = result.columns.at(index);
                    column.top_triangle_index = static_cast<int>(triangle_index);
                    column.top_barycentric = {w0, w1, w2};
                }
                ++hit_count.at(index);
            }
        }
    }

    result.model_masks.resize(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<std::uint8_t>(pixel_count, 0));
    diagnostics.clear();
    diagnostics.reserve(grid.layer_count);
    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index) {
        LayerDiagnostics layer_diagnostics;
        layer_diagnostics.layer_index = layer_index;
        layer_diagnostics.z_mm = (static_cast<double>(layer_index) + 0.5) * config.output.layer_thickness_mm;
        diagnostics.push_back(layer_diagnostics);
    }

    for (std::size_t index{0}; index < pixel_count; ++index) {
        if (hit_count.at(index) == 0) {
            continue;
        }
        ReliefColumnInfo& column = result.columns.at(index);
        column.has_model = true;
        column.z_min_mm = z_min.at(index);
        column.z_max_mm = z_max.at(index);
        column.hit_count = hit_count.at(index);
        column.multi_hit = hit_count.at(index) > 1;

        ++relief_report.hit_columns;
        if (column.multi_hit) {
            ++relief_report.multi_hit_columns;
        }
        if (!relief_report.has_hits) {
            relief_report.z_min_mm = z_min.at(index);
            relief_report.z_max_mm = z_max.at(index);
            relief_report.thickness_min_mm = z_max.at(index) - z_min.at(index);
            relief_report.thickness_max_mm = relief_report.thickness_min_mm;
            relief_report.has_hits = true;
        } else {
            relief_report.z_min_mm = std::min(relief_report.z_min_mm, z_min.at(index));
            relief_report.z_max_mm = std::max(relief_report.z_max_mm, z_max.at(index));
            const double thickness_mm = z_max.at(index) - z_min.at(index);
            relief_report.thickness_min_mm = std::min(relief_report.thickness_min_mm, thickness_mm);
            relief_report.thickness_max_mm = std::max(relief_report.thickness_max_mm, thickness_mm);
        }

        const double start_z =
            config.relief.fill_mode == "surface_to_base" ? config.relief.base_z_mm : z_min.at(index);
        const double end_z = z_max.at(index);
        int start_layer = first_layer_at_or_above_z(start_z, config.output.layer_thickness_mm);
        int end_layer = last_layer_at_or_below_z(end_z, config.output.layer_thickness_mm);
        start_layer = std::max(0, start_layer);
        end_layer = std::min(grid.layer_count - 1, end_layer);
        if (start_layer > end_layer) {
            continue;
        }
        column.lower_layer = start_layer;
        column.upper_layer = end_layer;
        for (int layer_index{start_layer}; layer_index <= end_layer; ++layer_index) {
            result.model_masks.at(layer_index).at(index) = 1;
        }
    }
    relief_report.empty_columns = relief_report.total_columns - relief_report.hit_columns;
    return result;
}

std::vector<int> compute_first_model_layers(const std::vector<std::vector<std::uint8_t>>& model_masks, const GridSpec& grid) {
    std::vector<int> first_model_layer(static_cast<std::size_t>(grid.width_px) * grid.height_px, -1);
    for (int layer_index{0}; layer_index < static_cast<int>(model_masks.size()); ++layer_index) {
        const auto& mask = model_masks.at(layer_index);
        for (std::size_t i{0}; i < mask.size(); ++i) {
            if (mask.at(i) != 0 && first_model_layer.at(i) < 0) {
                first_model_layer.at(i) = layer_index;
            }
        }
    }
    return first_model_layer;
}

std::vector<int> compute_relief_lower_layers(const std::vector<ReliefColumnInfo>& columns) {
    std::vector<int> lower_layers(columns.size(), -1);
    for (std::size_t i{0}; i < columns.size(); ++i) {
        if (columns.at(i).has_model && columns.at(i).lower_layer >= 0) {
            lower_layers.at(i) = columns.at(i).lower_layer;
        }
    }
    return lower_layers;
}

std::vector<ColumnLayerRange> compute_relief_column_ranges(const std::vector<ReliefColumnInfo>& columns) {
    std::vector<ColumnLayerRange> ranges(columns.size());
    for (std::size_t i{0}; i < columns.size(); ++i) {
        const ReliefColumnInfo& column = columns.at(i);
        if (column.has_model && column.lower_layer >= 0 && column.upper_layer >= column.lower_layer) {
            ranges.at(i) = {true, column.lower_layer, column.upper_layer};
        }
    }
    return ranges;
}

std::vector<ColumnLayerRange> compute_mask_column_ranges(
    const std::vector<std::vector<std::uint8_t>>& model_masks,
    const GridSpec& grid) {
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<ColumnLayerRange> ranges(pixel_count);
    for (int layer_index{0}; layer_index < static_cast<int>(model_masks.size()); ++layer_index) {
        const auto& mask = model_masks.at(layer_index);
        for (std::size_t i{0}; i < pixel_count; ++i) {
            if (mask.at(i) == 0) {
                continue;
            }
            ColumnLayerRange& range = ranges.at(i);
            if (!range.has_model) {
                range.has_model = true;
                range.lower_layer = layer_index;
            }
            range.upper_layer = layer_index;
        }
    }
    return ranges;
}

bool support_mode_includes_bottom_projection(const std::string& mode) {
    return mode == "bottom_projection" || mode == "bottom_projection_plus_unsupported";
}

bool support_mode_includes_unsupported(const std::string& mode) {
    return mode == "unsupported_only" || mode == "bottom_projection_plus_unsupported";
}

int support_type_priority(const SupportType type) {
    switch (type) {
        case SupportType::InternalVoid:
            return 5;
        case SupportType::UnsupportedIsland:
            return 4;
        case SupportType::FullVerticalProjection:
            return 3;
        case SupportType::UpperProjection:
            return 2;
        case SupportType::BottomProjection:
            return 1;
        case SupportType::None:
            return 0;
    }
    return 0;
}

std::string support_type_name(const SupportType type) {
    switch (type) {
        case SupportType::BottomProjection:
            return "bottom_projection";
        case SupportType::UnsupportedIsland:
            return "unsupported_island";
        case SupportType::FullVerticalProjection:
            return "full_vertical_projection";
        case SupportType::InternalVoid:
            return "internal_void";
        case SupportType::UpperProjection:
            return "upper_projection";
        case SupportType::None:
            return "none";
    }
    return "none";
}

SupportPlacementPolicy ResolveSupportPlacementPolicy(const SliceConfig& config)
{
    SupportPlacementPolicy policy;
    policy.requested_placement = config.support.placement;
    policy.effective_placement = config.support.placement;
    policy.placement_explicit = config.support.placement_explicit;

    if (config.support.placement_explicit)
    {
        policy.lower_enabled = config.support.placement == "lower" || config.support.placement == "both";
        policy.upper_enabled = config.support.placement == "upper" || config.support.placement == "both";
        policy.unsupported_only_enabled = config.support.placement == "unsupported_only";
        policy.full_vertical_projection_enabled = config.support.placement == "full_vertical_projection";
        policy.advanced_debug = policy.full_vertical_projection_enabled;
        return policy;
    }

    policy.requested_placement = "legacy_mode";
    policy.effective_placement = config.support.mode;
    policy.lower_enabled = support_mode_includes_bottom_projection(config.support.mode);
    policy.upper_enabled = false;
    policy.unsupported_only_enabled = support_mode_includes_unsupported(config.support.mode);
    policy.full_vertical_projection_enabled = config.support.mode == "full_vertical_projection";
    policy.advanced_debug = policy.full_vertical_projection_enabled;
    return policy;
}

void set_support_pixel(
    std::vector<std::uint8_t>& support_mask,
    std::vector<SupportType>& support_type_map,
    const std::size_t index,
    const SupportType type) {
    support_mask.at(index) = 1;
    if (support_type_priority(type) >= support_type_priority(support_type_map.at(index))) {
        support_type_map.at(index) = type;
    }
}

void SynchronizeSupportShapeTypeMaps(
    const std::vector<std::vector<std::uint8_t>>& originalSupportMasks,
    const std::vector<std::vector<std::uint8_t>>& optimizedSupportMasks,
    std::vector<std::vector<SupportType>>& supportTypeMaps)
{
    for (std::size_t layerIndex{0}; layerIndex < optimizedSupportMasks.size(); ++layerIndex)
    {
        const std::vector<std::uint8_t>& originalMask = originalSupportMasks.at(layerIndex);
        const std::vector<std::uint8_t>& optimizedMask = optimizedSupportMasks.at(layerIndex);
        std::vector<SupportType>& typeMap = supportTypeMaps.at(layerIndex);
        for (std::size_t index{0}; index < optimizedMask.size(); ++index)
        {
            if (optimizedMask.at(index) == 0)
            {
                typeMap.at(index) = SupportType::None;
            }
            else if (originalMask.at(index) == 0 && typeMap.at(index) == SupportType::None)
            {
                typeMap.at(index) = SupportType::BottomProjection;
            }
        }
    }
}

void AddInternalVoidSupportForLayer(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& modelMask,
    std::vector<std::uint8_t>& supportMask,
    std::vector<SupportType>& supportTypeMap)
{
    if (!config.support.internal_void.enabled)
    {
        return;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::uint8_t> externalEmpty(pixelCount, 0);
    std::vector<int> stack;
    stack.reserve(pixelCount);

    const auto pushExternalEmpty = [&](const int x, const int y)
    {
        if (x < 0 || x >= grid.width_px || y < 0 || y >= grid.height_px)
        {
            return;
        }
        const std::size_t index = mask_index(grid, x, y);
        if (modelMask.at(index) != 0 || externalEmpty.at(index) != 0)
        {
            return;
        }
        externalEmpty.at(index) = 1;
        stack.push_back(static_cast<int>(index));
    };

    for (int x{0}; x < grid.width_px; ++x)
    {
        pushExternalEmpty(x, 0);
        pushExternalEmpty(x, grid.height_px - 1);
    }
    for (int y{0}; y < grid.height_px; ++y)
    {
        pushExternalEmpty(0, y);
        pushExternalEmpty(grid.width_px - 1, y);
    }

    const std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}}, {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    const std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};

    while (!stack.empty())
    {
        const int current = stack.back();
        stack.pop_back();
        const int x = current % grid.width_px;
        const int y = current / grid.width_px;
        if (config.support.connectivity == 8)
        {
            for (const auto& neighbor : neighbors8)
            {
                pushExternalEmpty(x + neighbor.at(0), y + neighbor.at(1));
            }
        }
        else
        {
            for (const auto& neighbor : neighbors4)
            {
                pushExternalEmpty(x + neighbor.at(0), y + neighbor.at(1));
            }
        }
    }

    std::vector<std::uint8_t> visited(pixelCount, 0);
    for (std::size_t start{0}; start < pixelCount; ++start)
    {
        if (modelMask.at(start) != 0 || externalEmpty.at(start) != 0 || visited.at(start) != 0)
        {
            continue;
        }

        std::vector<int> component;
        component.reserve(64);
        stack.push_back(static_cast<int>(start));
        visited.at(start) = 1;
        while (!stack.empty())
        {
            const int current = stack.back();
            stack.pop_back();
            component.push_back(current);
            const int x = current % grid.width_px;
            const int y = current / grid.width_px;
            const auto pushComponentNeighbor = [&](const int nx, const int ny)
            {
                if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px)
                {
                    return;
                }
                const std::size_t next = mask_index(grid, nx, ny);
                if (modelMask.at(next) == 0 && externalEmpty.at(next) == 0 && visited.at(next) == 0)
                {
                    visited.at(next) = 1;
                    stack.push_back(static_cast<int>(next));
                }
            };
            if (config.support.connectivity == 8)
            {
                for (const auto& neighbor : neighbors8)
                {
                    pushComponentNeighbor(x + neighbor.at(0), y + neighbor.at(1));
                }
            }
            else
            {
                for (const auto& neighbor : neighbors4)
                {
                    pushComponentNeighbor(x + neighbor.at(0), y + neighbor.at(1));
                }
            }
        }

        if (static_cast<int>(component.size()) < config.support.internal_void.min_area_px)
        {
            continue;
        }
        for (const int pixel : component)
        {
            set_support_pixel(
                supportMask,
                supportTypeMap,
                static_cast<std::size_t>(pixel),
                SupportType::InternalVoid);
        }
    }
}

void AddUpperProjectionSupport(
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& modelMasks,
    const std::vector<ColumnLayerRange>& columnRanges,
    std::vector<std::vector<std::uint8_t>>& supportMasks,
    std::vector<std::vector<SupportType>>& supportTypeMaps)
{
    for (std::size_t index{0}; index < columnRanges.size(); ++index)
    {
        const ColumnLayerRange& range = columnRanges.at(index);
        if (!range.has_model || range.upper_layer < 0)
        {
            continue;
        }
        for (int layerIndex{range.upper_layer + 1}; layerIndex < grid.layer_count; ++layerIndex)
        {
            if (modelMasks.at(layerIndex).at(index) != 0)
            {
                continue;
            }
            set_support_pixel(
                supportMasks.at(layerIndex),
                supportTypeMaps.at(layerIndex),
                index,
                SupportType::UpperProjection);
        }
    }
}

SupportConnectivityDiagnostics analyze_support_connectivity(
    const std::vector<std::uint8_t>& support_mask,
    const GridSpec& grid,
    const int connectivity) {
    constexpr int tiny_component_area_px{8};
    constexpr int small_component_area_px{512};
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    SupportConnectivityDiagnostics diagnostics;
    diagnostics.enabled = true;
    std::vector<std::uint8_t> visited(pixel_count, 0);
    const std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}}, {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    const std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};

    for (std::size_t start{0}; start < pixel_count; ++start) {
        if (support_mask.at(start) == 0 || visited.at(start) != 0) {
            continue;
        }

        SupportComponentSummary component;
        component.min_x = grid.width_px;
        component.min_y = grid.height_px;
        component.max_x = -1;
        component.max_y = -1;
        std::vector<int> stack{static_cast<int>(start)};
        visited.at(start) = 1;
        while (!stack.empty()) {
            const int current = stack.back();
            stack.pop_back();
            const int x = current % grid.width_px;
            const int y = current / grid.width_px;
            ++component.area_px;
            component.min_x = std::min(component.min_x, x);
            component.min_y = std::min(component.min_y, y);
            component.max_x = std::max(component.max_x, x);
            component.max_y = std::max(component.max_y, y);

            if (connectivity == 8) {
                for (const auto& neighbor : neighbors8) {
                    const int nx{x + neighbor.at(0)};
                    const int ny{y + neighbor.at(1)};
                    if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px) {
                        continue;
                    }
                    const std::size_t next = mask_index(grid, nx, ny);
                    if (support_mask.at(next) != 0 && visited.at(next) == 0) {
                        visited.at(next) = 1;
                        stack.push_back(static_cast<int>(next));
                    }
                }
            } else {
                for (const auto& neighbor : neighbors4) {
                    const int nx{x + neighbor.at(0)};
                    const int ny{y + neighbor.at(1)};
                    if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px) {
                        continue;
                    }
                    const std::size_t next = mask_index(grid, nx, ny);
                    if (support_mask.at(next) != 0 && visited.at(next) == 0) {
                        visited.at(next) = 1;
                        stack.push_back(static_cast<int>(next));
                    }
                }
            }
        }

        ++diagnostics.component_count;
        diagnostics.largest_component_pixels =
            std::max(diagnostics.largest_component_pixels, component.area_px);
        if (component.area_px <= tiny_component_area_px) {
            ++diagnostics.tiny_component_count;
        } else if (component.area_px <= small_component_area_px) {
            ++diagnostics.small_component_count;
        }
        diagnostics.components.push_back(component);
    }

    std::sort(diagnostics.components.begin(), diagnostics.components.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.area_px > rhs.area_px;
    });
    return diagnostics;
}

std::vector<int> compute_last_model_layers(const std::vector<std::vector<std::uint8_t>>& model_masks, const GridSpec& grid) {
    std::vector<int> last_model_layer(static_cast<std::size_t>(grid.width_px) * grid.height_px, -1);
    for (int layer_index{0}; layer_index < static_cast<int>(model_masks.size()); ++layer_index) {
        const auto& mask = model_masks.at(layer_index);
        for (std::size_t i{0}; i < mask.size(); ++i) {
            if (mask.at(i) != 0) {
                last_model_layer.at(i) = layer_index;
            }
        }
    }
    return last_model_layer;
}

std::vector<std::uint8_t> make_supported_base_mask(
    const std::vector<std::uint8_t>& previous_model_mask,
    const std::vector<std::uint8_t>& previous_support_mask,
    const GridSpec& grid,
    const int dilation_px) {
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::uint8_t> base(pixel_count, 0);
    for (std::size_t i{0}; i < pixel_count; ++i) {
        if (previous_model_mask.at(i) != 0 || previous_support_mask.at(i) != 0) {
            base.at(i) = 1;
        }
    }
    for (int iteration{0}; iteration < dilation_px; ++iteration) {
        std::vector<std::uint8_t> dilated = base;
        for (int y{0}; y < grid.height_px; ++y) {
            for (int x{0}; x < grid.width_px; ++x) {
                const std::size_t index = mask_index(grid, x, y);
                if (base.at(index) == 0) {
                    continue;
                }
                for (int dy{-1}; dy <= 1; ++dy) {
                    for (int dx{-1}; dx <= 1; ++dx) {
                        const int nx{x + dx};
                        const int ny{y + dy};
                        if (nx >= 0 && nx < grid.width_px && ny >= 0 && ny < grid.height_px) {
                            dilated.at(mask_index(grid, nx, ny)) = 1;
                        }
                    }
                }
            }
        }
        base = std::move(dilated);
    }
    return base;
}

std::vector<IslandComponent> find_island_components(
    const std::vector<std::uint8_t>& model_mask,
    const std::vector<std::uint8_t>& base_mask,
    const GridSpec& grid,
    const SliceConfig& config,
    const int layer_index) {
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    std::vector<std::uint8_t> visited(pixel_count, 0);
    std::vector<IslandComponent> islands;
    int component_id{0};
    const std::array<std::array<int, 2>, 8> neighbors8{{
        {{-1, -1}}, {{0, -1}}, {{1, -1}}, {{-1, 0}}, {{1, 0}}, {{-1, 1}}, {{0, 1}}, {{1, 1}},
    }};
    const std::array<std::array<int, 2>, 4> neighbors4{{
        {{0, -1}}, {{-1, 0}}, {{1, 0}}, {{0, 1}},
    }};

    for (int y{0}; y < grid.height_px; ++y) {
        for (int x{0}; x < grid.width_px; ++x) {
            const std::size_t start = mask_index(grid, x, y);
            if (model_mask.at(start) == 0 || visited.at(start) != 0) {
                continue;
            }

            IslandComponent component;
            component.layer_index = layer_index;
            component.component_id = component_id++;
            std::vector<int> stack{static_cast<int>(start)};
            visited.at(start) = 1;
            while (!stack.empty()) {
                const int current = stack.back();
                stack.pop_back();
                component.pixels.push_back(current);
                const int cx = current % grid.width_px;
                const int cy = current / grid.width_px;
                if (base_mask.at(static_cast<std::size_t>(current)) != 0) {
                    ++component.overlap_px;
                }

                if (config.support.connectivity == 8) {
                    for (const auto& neighbor : neighbors8) {
                        const int nx{cx + neighbor.at(0)};
                        const int ny{cy + neighbor.at(1)};
                        if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px) {
                            continue;
                        }
                        const std::size_t next = mask_index(grid, nx, ny);
                        if (model_mask.at(next) != 0 && visited.at(next) == 0) {
                            visited.at(next) = 1;
                            stack.push_back(static_cast<int>(next));
                        }
                    }
                } else {
                    for (const auto& neighbor : neighbors4) {
                        const int nx{cx + neighbor.at(0)};
                        const int ny{cy + neighbor.at(1)};
                        if (nx < 0 || nx >= grid.width_px || ny < 0 || ny >= grid.height_px) {
                            continue;
                        }
                        const std::size_t next = mask_index(grid, nx, ny);
                        if (model_mask.at(next) != 0 && visited.at(next) == 0) {
                            visited.at(next) = 1;
                            stack.push_back(static_cast<int>(next));
                        }
                    }
                }
            }

            component.area_px = static_cast<int>(component.pixels.size());
            component.overlap_ratio = component.area_px > 0
                ? static_cast<double>(component.overlap_px) / static_cast<double>(component.area_px)
                : 0.0;
            if (component.overlap_ratio < config.support.min_overlap_ratio) {
                component.filtered = component.area_px < config.support.min_island_area_px;
                islands.push_back(std::move(component));
            }
        }
    }
    return islands;
}

SupportGenerationResult generate_support_masks(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::vector<std::uint8_t>>& model_masks,
    const std::vector<std::vector<std::uint8_t>>& upper_boundary_masks,
    const std::vector<int>& support_source_layers,
    const std::vector<ColumnLayerRange>& column_ranges,
    const std::vector<ColumnLayerRange>& upper_boundary_column_ranges,
    std::vector<LayerDiagnostics>& diagnostics) {
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    const SupportPlacementPolicy placement_policy = ResolveSupportPlacementPolicy(config);
    SupportGenerationResult result;
    result.support_masks.resize(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<std::uint8_t>(pixel_count, 0));
    result.support_type_maps.resize(
        static_cast<std::size_t>(grid.layer_count),
        std::vector<SupportType>(pixel_count, SupportType::None));

    if (!config.support.enabled) {
        return result;
    }

    if (placement_policy.lower_enabled) {
        for (std::size_t index{0}; index < support_source_layers.size(); ++index) {
            const int lower_layer = support_source_layers.at(index);
            for (int layer_index{0}; layer_index < lower_layer; ++layer_index) {
                if (model_masks.at(layer_index).at(index) == 0) {
                    set_support_pixel(
                        result.support_masks.at(layer_index),
                        result.support_type_maps.at(layer_index),
                        index,
                        SupportType::BottomProjection);
                }
            }
        }
    }

    if (placement_policy.full_vertical_projection_enabled) {
        const std::vector<int> last_model_layers = compute_last_model_layers(model_masks, grid);
        for (std::size_t index{0}; index < last_model_layers.size(); ++index) {
            const int last_layer = last_model_layers.at(index);
            for (int layer_index{0}; layer_index < last_layer; ++layer_index) {
                if (model_masks.at(layer_index).at(index) == 0) {
                    set_support_pixel(
                        result.support_masks.at(layer_index),
                        result.support_type_maps.at(layer_index),
                        index,
                        SupportType::FullVerticalProjection);
                }
            }
        }
    }

    if (placement_policy.upper_enabled) {
        AddUpperProjectionSupport(
            grid,
            upper_boundary_masks,
            upper_boundary_column_ranges,
            result.support_masks,
            result.support_type_maps);
    }

    if (placement_policy.unsupported_only_enabled) {
        for (int layer_index{1}; layer_index < grid.layer_count; ++layer_index) {
            const std::vector<std::uint8_t> base_mask = make_supported_base_mask(
                model_masks.at(layer_index - 1),
                result.support_masks.at(layer_index - 1),
                grid,
                config.support.xy_dilation_px);
            std::vector<IslandComponent> islands = find_island_components(
                model_masks.at(layer_index),
                base_mask,
                grid,
                config,
                layer_index);

            for (const IslandComponent& island : islands) {
                if (island.filtered) {
                    ++diagnostics.at(layer_index).filtered_island_count;
                    diagnostics.at(layer_index).filtered_island_pixels += island.area_px;
                    continue;
                }
                ++diagnostics.at(layer_index).island_count;
                diagnostics.at(layer_index).island_pixels += island.area_px;
                diagnostics.at(layer_index).unsupported_pixels += island.area_px;
                for (int target_layer{0}; target_layer < layer_index; ++target_layer) {
                    for (const int pixel : island.pixels) {
                        const std::size_t index = static_cast<std::size_t>(pixel);
                        if (model_masks.at(target_layer).at(index) == 0) {
                            set_support_pixel(
                                result.support_masks.at(target_layer),
                                result.support_type_maps.at(target_layer),
                                index,
                                SupportType::UnsupportedIsland);
                        }
                    }
                }
            }
        }
    }

    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index)
    {
        AddInternalVoidSupportForLayer(
            config,
            grid,
            model_masks.at(layer_index),
            result.support_masks.at(layer_index),
            result.support_type_maps.at(layer_index));
    }

    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index) {
        bool layer_has_support{false};
        const auto& support_mask = result.support_masks.at(layer_index);
        const auto& support_type_map = result.support_type_maps.at(layer_index);
        for (std::size_t index{0}; index < pixel_count; ++index) {
            if (support_mask.at(index) == 0) {
                continue;
            }
            layer_has_support = true;
            ++result.support_pixels;
            switch (support_type_map.at(index)) {
                case SupportType::BottomProjection:
                    ++result.bottom_projection_support_pixels;
                    ++diagnostics.at(layer_index).bottom_projection_support_pixels;
                    break;
                case SupportType::UnsupportedIsland:
                    ++result.unsupported_island_support_pixels;
                    ++diagnostics.at(layer_index).unsupported_island_support_pixels;
                    break;
                case SupportType::FullVerticalProjection:
                    ++result.full_vertical_projection_support_pixels;
                    ++diagnostics.at(layer_index).full_vertical_projection_support_pixels;
                    break;
                case SupportType::InternalVoid:
                    ++result.internal_void_support_pixels;
                    ++diagnostics.at(layer_index).internal_void_support_pixels;
                    break;
                case SupportType::UpperProjection:
                    ++result.upper_projection_support_pixels;
                    ++diagnostics.at(layer_index).upper_projection_support_pixels;
                    break;
                case SupportType::None:
                    break;
            }
        }
        if (layer_has_support) {
            ++result.layers_with_support;
        }
        if (diagnostics.at(layer_index).island_count > 0 || diagnostics.at(layer_index).filtered_island_count > 0) {
            ++result.layers_with_islands;
            result.island_count += diagnostics.at(layer_index).island_count;
            result.island_pixels += diagnostics.at(layer_index).island_pixels;
            result.unsupported_pixels += diagnostics.at(layer_index).unsupported_pixels;
            result.filtered_island_count += diagnostics.at(layer_index).filtered_island_count;
            result.filtered_island_pixels += diagnostics.at(layer_index).filtered_island_pixels;
        }
        diagnostics.at(layer_index).support_connectivity =
            analyze_support_connectivity(support_mask, grid, config.support.connectivity);
    }

    return result;
}

void RecalculateSupportGenerationStats(
    SupportGenerationResult& result,
    std::vector<LayerDiagnostics>& diagnostics,
    const GridSpec& grid,
    const SliceConfig& config)
{
    result.support_pixels = 0;
    result.bottom_projection_support_pixels = 0;
    result.unsupported_island_support_pixels = 0;
    result.full_vertical_projection_support_pixels = 0;
    result.internal_void_support_pixels = 0;
    result.upper_projection_support_pixels = 0;
    result.layers_with_support = 0;
    const std::size_t pixel_count = static_cast<std::size_t>(grid.width_px) * grid.height_px;
    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index)
    {
        bool layer_has_support{false};
        diagnostics.at(layer_index).bottom_projection_support_pixels = 0;
        diagnostics.at(layer_index).unsupported_island_support_pixels = 0;
        diagnostics.at(layer_index).full_vertical_projection_support_pixels = 0;
        diagnostics.at(layer_index).internal_void_support_pixels = 0;
        diagnostics.at(layer_index).upper_projection_support_pixels = 0;
        const auto& support_mask = result.support_masks.at(layer_index);
        const auto& support_type_map = result.support_type_maps.at(layer_index);
        for (std::size_t index{0}; index < pixel_count; ++index)
        {
            if (support_mask.at(index) == 0)
            {
                continue;
            }
            layer_has_support = true;
            ++result.support_pixels;
            switch (support_type_map.at(index))
            {
                case SupportType::BottomProjection:
                    ++result.bottom_projection_support_pixels;
                    ++diagnostics.at(layer_index).bottom_projection_support_pixels;
                    break;
                case SupportType::UnsupportedIsland:
                    ++result.unsupported_island_support_pixels;
                    ++diagnostics.at(layer_index).unsupported_island_support_pixels;
                    break;
                case SupportType::FullVerticalProjection:
                    ++result.full_vertical_projection_support_pixels;
                    ++diagnostics.at(layer_index).full_vertical_projection_support_pixels;
                    break;
                case SupportType::InternalVoid:
                    ++result.internal_void_support_pixels;
                    ++diagnostics.at(layer_index).internal_void_support_pixels;
                    break;
                case SupportType::UpperProjection:
                    ++result.upper_projection_support_pixels;
                    ++diagnostics.at(layer_index).upper_projection_support_pixels;
                    break;
                case SupportType::None:
                    break;
            }
        }
        if (layer_has_support)
        {
            ++result.layers_with_support;
        }
        diagnostics.at(layer_index).support_connectivity =
            analyze_support_connectivity(support_mask, grid, config.support.connectivity);
    }
}

const RuntimeMaterialTexture* find_runtime_material(
    const TextureRuntime& runtime,
    const std::string& material_name) {
    const auto found = runtime.materials.find(material_name);
    if (found == runtime.materials.end()) {
        return nullptr;
    }
    return &found->second;
}

Json rgb_to_json(const std::array<std::uint8_t, 3>& rgb) {
    return Json::array({static_cast<int>(rgb.at(0)), static_cast<int>(rgb.at(1)), static_cast<int>(rgb.at(2))});
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

MaterialRole material_role_from_string(const std::string& role) {
    if (role == "white") {
        return MaterialRole::White;
    }
    if (role == "varnish") {
        return MaterialRole::Varnish;
    }
    if (role == "ignore") {
        return MaterialRole::Ignore;
    }
    if (role == "support_candidate") {
        return MaterialRole::SupportCandidate;
    }
    if (role == "support") {
        return MaterialRole::Support;
    }
    return MaterialRole::Rgb;
}

std::string material_role_to_string(const MaterialRole role) {
    switch (role) {
        case MaterialRole::Rgb:
            return "rgb";
        case MaterialRole::White:
            return "white";
        case MaterialRole::Varnish:
            return "varnish";
        case MaterialRole::Ignore:
            return "ignore";
        case MaterialRole::SupportCandidate:
            return "support_candidate";
        case MaterialRole::Support:
            return "support";
    }
    return "rgb";
}

MaterialRole map_input_material_to_role(const std::string& material_name, const MaterialRoleMappingConfig& config) {
    const std::string lower_name = lower_copy(material_name);
    for (const MaterialRoleRuleConfig& rule : config.rules) {
        if (lower_name.find(lower_copy(rule.match_name_contains)) != std::string::npos) {
            MaterialRole role = material_role_from_string(rule.role);
            if (role == MaterialRole::Support && !config.allow_input_support_material) {
                return MaterialRole::SupportCandidate;
            }
            return role;
        }
    }
    return material_role_from_string(config.default_role);
}

void increment_role_count(MaterialRoleMappingReportData& report, const MaterialRole role) {
    switch (role) {
        case MaterialRole::Rgb:
            ++report.mapped_rgb;
            break;
        case MaterialRole::White:
            ++report.mapped_white;
            break;
        case MaterialRole::Varnish:
            ++report.mapped_varnish;
            break;
        case MaterialRole::Ignore:
            ++report.mapped_ignore;
            break;
        case MaterialRole::SupportCandidate:
            ++report.mapped_support_candidate;
            break;
        case MaterialRole::Support:
            ++report.mapped_support;
            break;
    }
}

const MaterialInfo* find_material_info_by_name(const ModelReport& model_report, const std::string& material_name) {
    const auto found = std::find_if(model_report.material_infos.begin(), model_report.material_infos.end(), [&](const MaterialInfo& material) {
        return material.name == material_name;
    });
    if (found == model_report.material_infos.end()) {
        return nullptr;
    }
    return &*found;
}

std::array<std::uint8_t, 3> material_rgb_for_role(
    const SliceConfig& config,
    const ModelReport& model_report,
    const std::vector<TextureColumnColor>* texture_columns,
    const std::size_t pixel_index,
    const std::string& material_name) {
    if (texture_columns != nullptr && pixel_index < texture_columns->size() && texture_columns->at(pixel_index).has_color) {
        return texture_columns->at(pixel_index).rgb;
    }
    const MaterialInfo* material = find_material_info_by_name(model_report, material_name);
    if (material != nullptr && material->has_diffuse) {
        return material->diffuse_rgb;
    }
    if (config.texture.enabled) {
        return config.texture.fallback_rgb;
    }
    return config.material.rgb;
}

MaterialRoleMappingReportData build_material_role_mapping_report(
    const SliceConfig& config,
    const ModelReport& model_report) {
    MaterialRoleMappingReportData report;
    report.enabled = config.material_role_mapping.enabled;
    report.input_format = model_report.format;
    report.default_role = config.material_role_mapping.default_role;
    report.allow_input_support_material = config.material_role_mapping.allow_input_support_material;
    for (const MaterialRoleRuleConfig& rule : config.material_role_mapping.rules) {
        report.rules.push_back(Json::object({
            {"matchNameContains", rule.match_name_contains},
            {"role", rule.role},
        }));
    }
    if (!config.material_role_mapping.enabled) {
        return report;
    }
    report.material_count = static_cast<int>(model_report.material_infos.size());
    for (const MaterialInfo& material : model_report.material_infos) {
        const MaterialRole role = map_input_material_to_role(material.name, config.material_role_mapping);
        increment_role_count(report, role);
        report.materials.push_back(Json::object({
            {"name", material.name},
            {"role", material_role_to_string(role)},
            {"hasDiffuse", material.has_diffuse},
            {"diffuseRgb", rgb_to_json(material.diffuse_rgb)},
            {"hasTexture", material.has_texture},
            {"texturePath", material.diffuse_texture_path.generic_string()},
        }));
        if (role == MaterialRole::SupportCandidate) {
            report.warnings.push_back("input material treated as support_candidate and did not write S: " + material.name);
        }
    }
    for (const TriangleTextureInfo& triangle : model_report.triangle_textures) {
        if (triangle.material_name.empty()) {
            ++report.faces_without_mapped_material;
        } else {
            ++report.faces_with_mapped_material;
        }
    }
    return report;
}

std::vector<MaterialRoleColumn> build_material_role_columns(
    const SliceConfig& config,
    const ModelReport& model_report,
    const std::vector<ReliefColumnInfo>& columns,
    const std::vector<TextureColumnColor>* texture_columns) {
    std::vector<MaterialRoleColumn> result(columns.size());
    if (!config.material_role_mapping.enabled) {
        return result;
    }
    for (std::size_t index{0}; index < columns.size(); ++index) {
        const ReliefColumnInfo& column = columns.at(index);
        if (!column.has_model || column.top_triangle_index < 0
            || column.top_triangle_index >= static_cast<int>(model_report.triangle_textures.size())) {
            continue;
        }
        const TriangleTextureInfo& texture_info =
            model_report.triangle_textures.at(static_cast<std::size_t>(column.top_triangle_index));
        MaterialRoleColumn& role_column = result.at(index);
        role_column.has_role = true;
        role_column.role = map_input_material_to_role(texture_info.material_name, config.material_role_mapping);
        role_column.rgb = material_rgb_for_role(config, model_report, texture_columns, index, texture_info.material_name);
    }
    return result;
}

TextureRuntime prepare_texture_runtime(const SliceConfig& config, const ModelReport& model_report) {
    TextureRuntime runtime;
    runtime.report.enabled = config.texture.enabled;
    runtime.report.apply_mode = config.texture.apply_mode;
    runtime.report.non_surface_rgb_policy = config.texture.non_surface_rgb_policy;
    runtime.report.top_surface_layers = config.texture.top_surface_layers;
    runtime.report.faces_with_uv = static_cast<int>(model_report.faces_with_uv);
    runtime.report.faces_without_uv = static_cast<int>(model_report.faces_without_uv);
    if (!config.texture.enabled) {
        return runtime;
    }

    for (const MaterialInfo& material : model_report.material_infos) {
        RuntimeMaterialTexture runtime_material;
        runtime_material.material = material;
        if (material.has_texture) {
            runtime.report.texture_files.push_back(material.diffuse_texture_path.generic_string());
            if (material.texture_source == "3mf_internal") {
                runtime.report.source = "3mf_internal";
            }
            if (!material.texture_exists) {
                ++runtime.report.missing_textures;
                runtime.report.warnings.push_back("missing texture: " + material.diffuse_texture_path.generic_string());
                if (config.texture.missing_texture_policy == "fail_fast") {
                    throw std::runtime_error("texture file does not exist: " + material.diffuse_texture_path.string());
                }
            } else {
                try {
                    runtime_material.image = load_texture_image(material.diffuse_texture_path);
                    runtime_material.loaded = true;
                    ++runtime.report.loaded_textures;
                } catch (const std::exception& error) {
                    ++runtime.report.missing_textures;
                    runtime.report.warnings.push_back("texture decode failed: " + material.diffuse_texture_path.generic_string());
                    if (config.texture.missing_texture_policy == "fail_fast") {
                        throw;
                    }
                    (void)error;
                }
            }
        }
        runtime.report.materials.push_back(Json::object({
            {"name", material.name},
            {"hasDiffuse", material.has_diffuse},
            {"diffuseRgb", rgb_to_json(material.diffuse_rgb)},
            {"hasTexture", material.has_texture},
            {"texturePath", material.diffuse_texture_path.generic_string()},
            {"source", material.texture_source},
            {"textureLoaded", runtime_material.loaded},
        }));
        runtime.materials.emplace(material.name, std::move(runtime_material));
    }

    if (model_report.material_infos.empty()) {
        runtime.report.warnings.push_back("model has no loaded MTL material info; fallback RGB will be used");
    }
    return runtime;
}

std::array<std::uint8_t, 3> fallback_texture_rgb(
    const SliceConfig& config,
    const RuntimeMaterialTexture* material) {
    if (material != nullptr && material->material.has_diffuse) {
        return material->material.diffuse_rgb;
    }
    return config.texture.fallback_rgb;
}

std::vector<TextureColumnColor> build_relief_texture_columns(
    const SliceConfig& config,
    const ModelReport& model_report,
    const std::vector<ReliefColumnInfo>& columns,
    TextureRuntime& runtime) {
    std::vector<TextureColumnColor> result(columns.size());
    if (!config.texture.enabled) {
        return result;
    }

    const TextureSampleOptions sample_options{
        config.texture.sampler,
        config.texture.uv_address_mode,
        config.texture.flip_v};

    for (std::size_t index{0}; index < columns.size(); ++index) {
        const ReliefColumnInfo& column = columns.at(index);
        if (!column.has_model || column.top_triangle_index < 0
            || column.top_triangle_index >= static_cast<int>(model_report.triangle_textures.size())) {
            continue;
        }

        const TriangleTextureInfo& texture_info =
            model_report.triangle_textures.at(static_cast<std::size_t>(column.top_triangle_index));
        const RuntimeMaterialTexture* material = find_runtime_material(runtime, texture_info.material_name);
        TextureColumnColor& color = result.at(index);
        color.has_color = true;

        if (texture_info.has_uv && material != nullptr && material->loaded) {
            const double u = column.top_barycentric.at(0) * texture_info.uv.at(0).u
                + column.top_barycentric.at(1) * texture_info.uv.at(1).u
                + column.top_barycentric.at(2) * texture_info.uv.at(2).u;
            const double v = column.top_barycentric.at(0) * texture_info.uv.at(0).v
                + column.top_barycentric.at(1) * texture_info.uv.at(1).v
                + column.top_barycentric.at(2) * texture_info.uv.at(2).v;
            bool uv_out_of_range{false};
            color.rgb = sample_texture_rgb(material->image, u, v, sample_options, uv_out_of_range);
            color.sampled_texture = true;
            color.uv_out_of_range = uv_out_of_range;
        } else {
            color.rgb = fallback_texture_rgb(config, material);
            color.used_fallback = true;
        }
    }
    return result;
}

void write_model_pixel(std::vector<std::uint8_t>& pixels, const std::size_t base, const SliceConfig& config) {
    if (config.material.material_channel == "V") {
        pixels.at(base + 5U) = config.material.varnish_value;
        return;
    }
    if (config.material.material_channel == "W") {
        pixels.at(base + 3U) = config.material.white_value;
        return;
    }
    if (config.material.material_channel == "RGB") {
        pixels.at(base + 0U) = config.material.rgb.at(0);
        pixels.at(base + 1U) = config.material.rgb.at(1);
        pixels.at(base + 2U) = config.material.rgb.at(2);
        return;
    }

    pixels.at(base + 0U) = config.material.rgb.at(0);
    pixels.at(base + 1U) = config.material.rgb.at(1);
    pixels.at(base + 2U) = config.material.rgb.at(2);
    pixels.at(base + 3U) = config.material.white_value;
    pixels.at(base + 4U) = config.background.value;
    pixels.at(base + 5U) = config.material.varnish_value;
}

std::array<std::uint8_t, 3> NonSurfaceRgb(const SliceConfig& config)
{
    if (config.texture.non_surface_rgb_policy == "empty")
    {
        return {config.background.value, config.background.value, config.background.value};
    }
    if (config.texture.non_surface_rgb_policy == "fallback_rgb")
    {
        return config.texture.fallback_rgb;
    }
    return config.material.rgb;
}

void write_non_surface_texture_pixel(
    std::vector<std::uint8_t>& pixels,
    const std::size_t base,
    const SliceConfig& config)
{
    if (config.texture.non_surface_rgb_policy == "model_material"
        || config.texture.non_surface_rgb_policy == "material_policy")
    {
        write_model_pixel(pixels, base, config);
        return;
    }

    const std::array<std::uint8_t, 3> rgb = NonSurfaceRgb(config);
    pixels.at(base + 0U) = rgb.at(0);
    pixels.at(base + 1U) = rgb.at(1);
    pixels.at(base + 2U) = rgb.at(2);
    if (config.material.material_channel == "W")
    {
        pixels.at(base + 3U) = config.material.white_value;
    }
    else if (config.material.material_channel == "V")
    {
        pixels.at(base + 5U) = config.material.varnish_value;
    }
    else if (config.material.material_channel == "auto")
    {
        pixels.at(base + 3U) = config.material.white_value;
        pixels.at(base + 5U) = config.material.varnish_value;
    }
}

bool ModelFillUsesExplicitPolicy(const SliceConfig& config)
{
    return config.model_fill.enabled && !config.model_fill.legacy_rgb_fallback;
}

ModelFillMaterial ResolveProfileDefaultModelFillMaterial(const SliceConfig& config)
{
    if (config.material_process_profile.enabled)
    {
        if (config.material_process_profile.white.enabled
            && config.material_process_profile.white.mode != "disabled")
        {
            return ModelFillMaterial::White;
        }
        if (config.material_process_profile.varnish.enabled
            && config.material_process_profile.varnish.mode != "disabled")
        {
            return ModelFillMaterial::Varnish;
        }
    }

    if (config.material_policy.enabled)
    {
        if (config.material_policy.white.enabled && config.material_policy.white.mode != "disabled")
        {
            return ModelFillMaterial::White;
        }
        if (config.material_policy.varnish.enabled && config.material_policy.varnish.mode != "disabled")
        {
            return ModelFillMaterial::Varnish;
        }
    }

    if (config.material.material_channel == "W")
    {
        return ModelFillMaterial::White;
    }
    if (config.material.material_channel == "V")
    {
        return ModelFillMaterial::Varnish;
    }
    return ModelFillMaterial::Rgb;
}

std::string ModelFillMaterialToString(const ModelFillMaterial material)
{
    switch (material)
    {
        case ModelFillMaterial::Rgb:
            return "rgb";
        case ModelFillMaterial::White:
            return "white";
        case ModelFillMaterial::Varnish:
            return "varnish";
        case ModelFillMaterial::None:
            return "none";
    }
    return "none";
}

ModelFillMaterial ResolveModelFillMaterial(
    const SliceConfig& config,
    const MaterialRoleColumn* roleColumn)
{
    if (config.model_fill.material == "white")
    {
        return ModelFillMaterial::White;
    }
    if (config.model_fill.material == "varnish")
    {
        return ModelFillMaterial::Varnish;
    }
    if (config.model_fill.material == "rgb")
    {
        return ModelFillMaterial::Rgb;
    }
    if (config.model_fill.material == "material_role")
    {
        if (roleColumn == nullptr || !roleColumn->has_role)
        {
            return ResolveProfileDefaultModelFillMaterial(config);
        }
        switch (roleColumn->role)
        {
            case MaterialRole::Rgb:
                return ModelFillMaterial::Rgb;
            case MaterialRole::White:
                return ModelFillMaterial::White;
            case MaterialRole::Varnish:
                return ModelFillMaterial::Varnish;
            case MaterialRole::Support:
            case MaterialRole::Ignore:
            case MaterialRole::SupportCandidate:
                return ModelFillMaterial::None;
        }
    }
    return ResolveProfileDefaultModelFillMaterial(config);
}

bool ShouldApplyModelFill(
    const SliceConfig& config,
    const bool textureSurfacePixel,
    const ModelFillMaterial material)
{
    if (!ModelFillUsesExplicitPolicy(config) || material == ModelFillMaterial::None)
    {
        return false;
    }
    if (!textureSurfacePixel)
    {
        return true;
    }
    if (config.model_fill.scope == "below_texture_surface" || material == ModelFillMaterial::Rgb)
    {
        return false;
    }
    return true;
}

bool WriteModelFillPixel(
    std::vector<std::uint8_t>& pixels,
    const std::size_t base,
    const SliceConfig& config,
    const MaterialRoleColumn* roleColumn)
{
    const ModelFillMaterial material = ResolveModelFillMaterial(config, roleColumn);
    switch (material)
    {
        case ModelFillMaterial::Rgb:
            if (roleColumn != nullptr && roleColumn->has_role && roleColumn->role == MaterialRole::Rgb
                && config.model_fill.material == "material_role")
            {
                pixels.at(base + 0U) = roleColumn->rgb.at(0);
                pixels.at(base + 1U) = roleColumn->rgb.at(1);
                pixels.at(base + 2U) = roleColumn->rgb.at(2);
                return true;
            }
            {
                const std::array<std::uint8_t, 3> rgb = NonSurfaceRgb(config);
                pixels.at(base + 0U) = rgb.at(0);
                pixels.at(base + 1U) = rgb.at(1);
                pixels.at(base + 2U) = rgb.at(2);
            }
            return true;
        case ModelFillMaterial::White:
            pixels.at(base + 3U) = config.model_fill.value;
            return true;
        case ModelFillMaterial::Varnish:
            pixels.at(base + 5U) = config.model_fill.value;
            return true;
        case ModelFillMaterial::None:
            return false;
    }
    return false;
}

MaterialClosureRepairValues ResolveMaterialClosureRepairValues(const SliceConfig& config)
{
    MaterialClosureRepairValues values;
    values.modelFillRgb = NonSurfaceRgb(config);
    values.modelFillValue = config.model_fill.value;
    values.supportValue = config.support.value;
    switch (ResolveModelFillMaterial(config, nullptr))
    {
        case ModelFillMaterial::Rgb:
            values.modelFillMaterial = MaterialClosureModelFillMaterial::Rgb;
            break;
        case ModelFillMaterial::White:
            values.modelFillMaterial = MaterialClosureModelFillMaterial::White;
            break;
        case ModelFillMaterial::Varnish:
            values.modelFillMaterial = MaterialClosureModelFillMaterial::Varnish;
            break;
        case ModelFillMaterial::None:
            values.modelFillMaterial = MaterialClosureModelFillMaterial::None;
            break;
    }
    return values;
}

std::uint8_t ResolveSurfaceVarnishValue(const SliceConfig& config)
{
    if (config.surface_varnish.source == "material_policy"
        && config.material_policy.enabled
        && config.material_policy.varnish.enabled)
    {
        return config.material_policy.varnish.value;
    }
    return config.surface_varnish.value;
}

TextureColumnColor resolve_texture_color(
    const SliceConfig& config,
    const std::vector<TextureColumnColor>* texture_columns,
    const std::size_t pixel_index) {
    TextureColumnColor color;
    color.has_color = true;
    color.rgb = config.texture.fallback_rgb;
    color.used_fallback = true;
    if (texture_columns != nullptr && pixel_index < texture_columns->size()
        && texture_columns->at(pixel_index).has_color) {
        color = texture_columns->at(pixel_index);
    }
    return color;
}

void update_texture_report_for_color(const TextureColumnColor& color, TextureReportData* texture_report) {
    if (texture_report == nullptr) {
        return;
    }
    if (color.sampled_texture) {
        ++texture_report->sampled_pixels;
    }
    if (color.used_fallback) {
        ++texture_report->fallback_pixels;
    }
    if (color.uv_out_of_range) {
        ++texture_report->uv_out_of_range_pixels;
    }
}

bool is_top_material_layer(
    const std::vector<ColumnLayerRange>* column_ranges,
    const std::size_t pixel_index,
    const int layer_index,
    const int top_layers) {
    if (column_ranges == nullptr || pixel_index >= column_ranges->size()) {
        return false;
    }
    const ColumnLayerRange& range = column_ranges->at(pixel_index);
    if (!range.has_model || range.upper_layer < range.lower_layer) {
        return false;
    }
    const int first_top_layer = std::max(range.lower_layer, range.upper_layer - top_layers + 1);
    return layer_index >= first_top_layer && layer_index <= range.upper_layer;
}

bool ShouldApplyTextureToLayer(
    const SliceConfig& config,
    const std::vector<ColumnLayerRange>* columnRanges,
    const std::size_t pixelIndex,
    const int layerIndex)
{
    if (config.texture.apply_mode == "solid_volume_from_top_surface")
    {
        return true;
    }
    if (config.texture.apply_mode == "top_surface_only")
    {
        return is_top_material_layer(columnRanges, pixelIndex, layerIndex, 1);
    }
    if (config.texture.apply_mode == "top_surface_band")
    {
        return is_top_material_layer(
            columnRanges,
            pixelIndex,
            layerIndex,
            config.texture.top_surface_layers);
    }
    return true;
}

MaterialPixel compose_material_policy_pixel(
    const SliceConfig& config,
    const std::vector<TextureColumnColor>* texture_columns,
    const std::vector<ColumnLayerRange>* column_ranges,
    const std::size_t pixel_index,
    const int layer_index,
    TextureReportData* texture_report) {
    MaterialPixel pixel;
    if (config.material_policy.rgb.enabled) {
        if (config.material_policy.rgb.source == "texture_or_fallback" && config.texture.enabled
            && ShouldApplyTextureToLayer(config, column_ranges, pixel_index, layer_index)) {
            const TextureColumnColor color = resolve_texture_color(config, texture_columns, pixel_index);
            pixel.r = color.rgb.at(0);
            pixel.g = color.rgb.at(1);
            pixel.b = color.rgb.at(2);
            update_texture_report_for_color(color, texture_report);
        } else {
            const bool useNonSurfaceTextureRgb =
                config.material_policy.rgb.source == "texture_or_fallback" && config.texture.enabled;
            const std::array<std::uint8_t, 3> rgb =
                useNonSurfaceTextureRgb ? NonSurfaceRgb(config) : config.material.rgb;
            pixel.r = rgb.at(0);
            pixel.g = rgb.at(1);
            pixel.b = rgb.at(2);
        }
    }

    if (config.material_policy.white.enabled
        && (config.material_policy.white.mode == "underbase" || config.material_policy.white.mode == "all_model")) {
        pixel.w = config.material_policy.white.value;
    }

    if (config.material_policy.varnish.enabled) {
        if (config.material_policy.varnish.mode == "all_model") {
            pixel.v = config.material_policy.varnish.value;
        } else if (config.material_policy.varnish.mode == "top_n_layers"
                   && is_top_material_layer(
                       column_ranges,
                       pixel_index,
                       layer_index,
                       config.material_policy.varnish.top_layers)) {
            pixel.v = config.material_policy.varnish.value;
        }
    }
    return pixel;
}

void write_material_pixel(
    std::vector<std::uint8_t>& pixels,
    const std::size_t base,
    const MaterialPixel& pixel,
    MaterialPolicyReportData* material_policy_report) {
    pixels.at(base + 0U) = pixel.r;
    pixels.at(base + 1U) = pixel.g;
    pixels.at(base + 2U) = pixel.b;
    pixels.at(base + 3U) = pixel.w;
    pixels.at(base + 5U) = pixel.v;
    if (material_policy_report != nullptr) {
        if (pixel.r < 255U || pixel.g < 255U || pixel.b < 255U) {
            ++material_policy_report->rgb_print_pixels;
        }
        if (pixel.w < 255U) {
            ++material_policy_report->white_print_pixels;
        }
        if (pixel.v < 255U) {
            ++material_policy_report->varnish_print_pixels;
        }
    }
}

bool write_material_role_pixel(
    std::vector<std::uint8_t>& pixels,
    const std::size_t base,
    const MaterialRoleColumn& role_column) {
    if (!role_column.has_role) {
        pixels.at(base + 0U) = role_column.rgb.at(0);
        pixels.at(base + 1U) = role_column.rgb.at(1);
        pixels.at(base + 2U) = role_column.rgb.at(2);
        return true;
    }
    switch (role_column.role) {
        case MaterialRole::Rgb:
            pixels.at(base + 0U) = role_column.rgb.at(0);
            pixels.at(base + 1U) = role_column.rgb.at(1);
            pixels.at(base + 2U) = role_column.rgb.at(2);
            return true;
        case MaterialRole::White:
            pixels.at(base + 3U) = 0;
            return true;
        case MaterialRole::Varnish:
            pixels.at(base + 5U) = 0;
            return true;
        case MaterialRole::Support:
            pixels.at(base + 4U) = 0;
            return true;
        case MaterialRole::Ignore:
        case MaterialRole::SupportCandidate:
            return false;
    }
    return false;
}

std::vector<std::uint8_t> build_texture_preview_mask(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& model_mask,
    const std::vector<MaterialRoleColumn>* material_role_columns,
    const std::vector<ColumnLayerRange>* column_ranges,
    const int layer_index)
{
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(grid.width_px) * grid.height_px, 0);
    if (!config.texture.enabled)
    {
        return mask;
    }

    for (int y{0}; y < grid.height_px; ++y)
    {
        for (int x{0}; x < grid.width_px; ++x)
        {
            const std::size_t pixel_index = mask_index(grid, x, y);
            if (model_mask.at(pixel_index) == 0)
            {
                continue;
            }

            bool rgb_role{true};
            if (config.material_role_mapping.enabled && material_role_columns != nullptr
                && pixel_index < material_role_columns->size())
            {
                const MaterialRoleColumn& role_column = material_role_columns->at(pixel_index);
                rgb_role = !role_column.has_role || role_column.role == MaterialRole::Rgb;
            }
            if (rgb_role && ShouldApplyTextureToLayer(config, column_ranges, pixel_index, layer_index))
            {
                mask.at(pixel_index) = 1;
            }
        }
    }
    return mask;
}

MaterialClosureSemanticLayerInput InitializeMaterialClosureSemanticInput(
    const GridSpec& grid,
    const int layerIndex,
    const double zMm,
    const std::vector<std::uint8_t>& modelEnvelopeMask,
    const std::vector<std::uint8_t>& finalSupportMask,
    const std::vector<std::size_t>& clearedSupportIndices,
    const std::vector<std::uint8_t>& outerVarnishShellMask)
{
    const std::size_t pixelCount = static_cast<std::size_t>(grid.width_px)
        * static_cast<std::size_t>(grid.height_px);
    MaterialClosureSemanticLayerInput input;
    input.layerIndex = layerIndex;
    input.zMm = zMm;
    input.widthPx = grid.width_px;
    input.heightPx = grid.height_px;
    input.textureSurfaceMask.assign(pixelCount, 0U);
    input.modelFillMask.assign(pixelCount, 0U);
    input.modelMaterialMask.assign(pixelCount, 0U);
    input.supportFillMask.assign(pixelCount, 0U);
    input.internalVoidSupportMask.assign(pixelCount, 0U);
    input.surfaceVarnishMask.assign(pixelCount, 0U);
    input.outerVarnishShellMask = outerVarnishShellMask;
    input.modelEnvelopeMask = modelEnvelopeMask;
    input.supportRequiredMask = finalSupportMask;
    input.layerEmptyMask.assign(pixelCount, 0U);

    for (const std::size_t index : clearedSupportIndices)
    {
        input.supportRequiredMask.at(index) = 1U;
    }

    input.expectedOccupiedDomainMask.assign(pixelCount, 0U);
    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        input.expectedOccupiedDomainMask.at(index) =
            input.modelEnvelopeMask.at(index) != 0U
                || input.supportRequiredMask.at(index) != 0U
                || input.outerVarnishShellMask.at(index) != 0U
            ? 1U
            : 0U;
    }
    return input;
}

void PopulateMaterialClosureEmptyMask(
    const std::vector<std::uint8_t>& layer,
    MaterialClosureSemanticLayerInput& input)
{
    constexpr std::size_t channelCount{6U};
    const std::size_t pixelCount = static_cast<std::size_t>(input.widthPx)
        * static_cast<std::size_t>(input.heightPx);
    if (layer.size() != pixelCount * channelCount)
    {
        throw std::invalid_argument("material closure semantic RGBWSV layer size mismatch");
    }

    for (std::size_t index{0U}; index < pixelCount; ++index)
    {
        const std::size_t base = index * channelCount;
        bool empty{true};
        for (std::size_t channel{0U}; channel < channelCount; ++channel)
        {
            empty = empty && layer.at(base + channel) == 255U;
        }
        input.layerEmptyMask.at(index) = empty ? 1U : 0U;
    }
}

std::vector<std::uint8_t> compose_layer(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& model_mask,
    const std::vector<std::uint8_t>& outer_varnish_mask,
    const std::vector<std::uint8_t>& outer_surface_varnish_mask,
    const std::vector<std::uint8_t>& inner_surface_varnish_mask,
    const std::vector<std::uint8_t>& support_mask,
    const std::vector<SupportType>& support_type_map,
    const std::vector<TextureColumnColor>* texture_columns,
    const std::vector<MaterialRoleColumn>* material_role_columns,
    const std::vector<ColumnLayerRange>* column_ranges,
    const int layer_index,
    TextureReportData* texture_report,
    MaterialPolicyReportData* material_policy_report,
    MaterialClosureSemanticLayerInput* materialClosureInput,
    LayerSemanticStats& semantic_stats,
    int& model_pixels,
    int& support_pixels) {
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(grid.width_px) * grid.height_px * rgbwsv_channel_count,
        config.background.value);

    for (int y{0}; y < grid.height_px; ++y) {
        for (int x{0}; x < grid.width_px; ++x) {
            const std::size_t pixel_index = mask_index(grid, x, y);
            const std::size_t base =
                (static_cast<std::size_t>(y) * grid.width_px + x) * rgbwsv_channel_count;
            if (model_mask.at(pixel_index) != 0) {
                bool counted_model_pixel{false};
                bool texture_surface_pixel{false};
                bool model_fill_pixel{false};
                if (config.material_role_mapping.enabled && material_role_columns != nullptr
                    && pixel_index < material_role_columns->size()) {
                    const MaterialRoleColumn& role_column = material_role_columns->at(pixel_index);
                    bool wrote_model{false};
                    const bool apply_texture =
                        config.texture.enabled && ShouldApplyTextureToLayer(config, column_ranges, pixel_index, layer_index);
                    if (role_column.has_role && role_column.role == MaterialRole::Rgb && config.texture.enabled
                        && !apply_texture) {
                        if (ModelFillUsesExplicitPolicy(config)) {
                            wrote_model = WriteModelFillPixel(pixels, base, config, &role_column);
                            model_fill_pixel = wrote_model;
                        } else {
                            write_non_surface_texture_pixel(pixels, base, config);
                            wrote_model = true;
                        }
                    } else {
                        wrote_model = write_material_role_pixel(
                            pixels,
                            base,
                            role_column);
                    }
                    if (wrote_model) {
                        counted_model_pixel = true;
                        if (role_column.has_role && role_column.role == MaterialRole::Rgb && apply_texture) {
                            const TextureColumnColor color =
                                resolve_texture_color(config, texture_columns, pixel_index);
                            update_texture_report_for_color(color, texture_report);
                            texture_surface_pixel = true;
                        } else if (config.model_fill.enabled) {
                            model_fill_pixel = !role_column.has_role
                                || role_column.role == MaterialRole::Rgb
                                || (role_column.role == MaterialRole::White && config.model_fill.material == "white")
                                || (role_column.role == MaterialRole::Varnish && config.model_fill.material == "varnish");
                        }
                        if (texture_surface_pixel) {
                            const ModelFillMaterial fill_material = ResolveModelFillMaterial(config, &role_column);
                            if (ShouldApplyModelFill(config, texture_surface_pixel, fill_material)
                                && WriteModelFillPixel(pixels, base, config, &role_column)) {
                                model_fill_pixel = true;
                            }
                        }
                    }
                } else if (config.material_policy.enabled) {
                    texture_surface_pixel =
                        config.material_policy.rgb.enabled
                        && config.material_policy.rgb.source == "texture_or_fallback"
                        && config.texture.enabled
                        && ShouldApplyTextureToLayer(config, column_ranges, pixel_index, layer_index);
                    const ModelFillMaterial fill_material = ResolveModelFillMaterial(config, nullptr);
                    if (ShouldApplyModelFill(config, texture_surface_pixel, fill_material)
                        && !texture_surface_pixel) {
                        counted_model_pixel = WriteModelFillPixel(pixels, base, config, nullptr);
                        model_fill_pixel = counted_model_pixel;
                    } else {
                        const MaterialPixel pixel = compose_material_policy_pixel(
                            config,
                            texture_columns,
                            column_ranges,
                            pixel_index,
                            layer_index,
                            texture_report);
                        write_material_pixel(pixels, base, pixel, material_policy_report);
                        counted_model_pixel = true;
                        if (ShouldApplyModelFill(config, texture_surface_pixel, fill_material)
                            && WriteModelFillPixel(pixels, base, config, nullptr)) {
                            model_fill_pixel = true;
                        } else {
                            model_fill_pixel = config.model_fill.enabled && !texture_surface_pixel;
                        }
                    }
                } else if (config.texture.enabled
                           && ShouldApplyTextureToLayer(config, column_ranges, pixel_index, layer_index)) {
                    const TextureColumnColor color = resolve_texture_color(config, texture_columns, pixel_index);
                    pixels.at(base + 0U) = color.rgb.at(0);
                    pixels.at(base + 1U) = color.rgb.at(1);
                    pixels.at(base + 2U) = color.rgb.at(2);
                    update_texture_report_for_color(color, texture_report);
                    counted_model_pixel = true;
                    texture_surface_pixel = true;
                    const ModelFillMaterial fill_material = ResolveModelFillMaterial(config, nullptr);
                    if (ShouldApplyModelFill(config, texture_surface_pixel, fill_material)
                        && WriteModelFillPixel(pixels, base, config, nullptr)) {
                        model_fill_pixel = true;
                    }
                } else {
                    if (ModelFillUsesExplicitPolicy(config)) {
                        counted_model_pixel = WriteModelFillPixel(pixels, base, config, nullptr);
                        model_fill_pixel = counted_model_pixel;
                    } else {
                        write_non_surface_texture_pixel(pixels, base, config);
                        counted_model_pixel = true;
                        model_fill_pixel = config.model_fill.enabled;
                    }
                }
                if (counted_model_pixel) {
                    const std::uint8_t surfaceVarnishValue = ResolveSurfaceVarnishValue(config);
                    if (outer_surface_varnish_mask.at(pixel_index) != 0) {
                        pixels.at(base + 5U) = surfaceVarnishValue;
                        ++semantic_stats.outer_surface_varnish_pixels;
                        if (materialClosureInput != nullptr)
                        {
                            materialClosureInput->surfaceVarnishMask.at(pixel_index) = 1U;
                        }
                    }
                    if (inner_surface_varnish_mask.at(pixel_index) != 0) {
                        pixels.at(base + 5U) = surfaceVarnishValue;
                        ++semantic_stats.inner_surface_varnish_pixels;
                        if (materialClosureInput != nullptr)
                        {
                            materialClosureInput->surfaceVarnishMask.at(pixel_index) = 1U;
                        }
                    }
                    ++model_pixels;
                    if (materialClosureInput != nullptr)
                    {
                        materialClosureInput->modelMaterialMask.at(pixel_index) = 1U;
                    }
                    if (texture_surface_pixel) {
                        ++semantic_stats.texture_surface_pixels;
                        if (materialClosureInput != nullptr)
                        {
                            materialClosureInput->textureSurfaceMask.at(pixel_index) = 1U;
                        }
                    }
                    if (model_fill_pixel) {
                        ++semantic_stats.model_fill_pixels;
                        if (materialClosureInput != nullptr)
                        {
                            materialClosureInput->modelFillMask.at(pixel_index) = 1U;
                        }
                    }
                }
            } else if (outer_varnish_mask.at(pixel_index) != 0) {
                pixels.at(base + 5U) = config.outer_varnish.value;
                ++semantic_stats.outer_varnish_pixels;
            } else if (config.support.enabled && support_mask.at(pixel_index) != 0) {
                pixels.at(base + 4U) = config.support.value;
                ++support_pixels;
                ++semantic_stats.support_pixels;
                if (materialClosureInput != nullptr)
                {
                    materialClosureInput->supportFillMask.at(pixel_index) = 1U;
                }
                if (support_type_map.at(pixel_index) == SupportType::InternalVoid) {
                    ++semantic_stats.internal_void_support_pixels;
                    if (materialClosureInput != nullptr)
                    {
                        materialClosureInput->internalVoidSupportMask.at(pixel_index) = 1U;
                    }
                }
            }
        }
    }

    return pixels;
}

void update_layer_channel_stats(const std::vector<std::uint8_t>& layer, LayerDiagnostics& diagnostics) {
    const std::size_t pixel_count = layer.size() / rgbwsv_channel_count;
    for (std::size_t i{0}; i < pixel_count; ++i) {
        const std::size_t base{i * rgbwsv_channel_count};
        for (std::size_t channel{0}; channel < rgbwsv_channel_count; ++channel) {
            const int value = layer.at(base + channel);
            ChannelStats& stats = diagnostics.channel_stats.at(channel);
            stats.min_value = std::min(stats.min_value, value);
            stats.max_value = std::max(stats.max_value, value);
            if (value == 255) {
                ++stats.empty_pixels;
            } else {
                ++stats.print_pixels;
                if (value == 0) {
                    ++stats.full_print_pixels;
                } else {
                    ++stats.partial_print_pixels;
                }
            }
        }
        if (layer.at(base + 0U) < 255U || layer.at(base + 1U) < 255U || layer.at(base + 2U) < 255U) {
            ++diagnostics.rgb_non_zero_pixels;
        }
        if (layer.at(base + 3U) < 255U) {
            ++diagnostics.white_non_zero_pixels;
        }
        if (layer.at(base + 4U) < 255U) {
            ++diagnostics.support_non_zero_pixels;
        }
        if (layer.at(base + 5U) < 255U) {
            ++diagnostics.varnish_non_zero_pixels;
        }
    }
}

void merge_channel_stats(std::array<ChannelStats, rgbwsv_channel_count>& totals, const LayerDiagnostics& diagnostics) {
    for (std::size_t channel{0}; channel < rgbwsv_channel_count; ++channel) {
        ChannelStats& total = totals.at(channel);
        const ChannelStats& layer = diagnostics.channel_stats.at(channel);
        total.print_pixels += layer.print_pixels;
        total.full_print_pixels += layer.full_print_pixels;
        total.partial_print_pixels += layer.partial_print_pixels;
        total.empty_pixels += layer.empty_pixels;
        total.min_value = std::min(total.min_value, layer.min_value);
        total.max_value = std::max(total.max_value, layer.max_value);
    }
}

void merge_semantic_stats(LayerSemanticStats& totals, const LayerSemanticStats& layer) {
    totals.texture_surface_pixels += layer.texture_surface_pixels;
    totals.model_fill_pixels += layer.model_fill_pixels;
    totals.support_pixels += layer.support_pixels;
    totals.internal_void_support_pixels += layer.internal_void_support_pixels;
    totals.outer_varnish_pixels += layer.outer_varnish_pixels;
    totals.outer_surface_varnish_pixels += layer.outer_surface_varnish_pixels;
    totals.inner_surface_varnish_pixels += layer.inner_surface_varnish_pixels;
}

Json channel_stats_to_json(const ChannelStats& stats) {
    return Json::object({
        {"printPixels", stats.print_pixels},
        {"fullPrintPixels", stats.full_print_pixels},
        {"partialPrintPixels", stats.partial_print_pixels},
        {"emptyPixels", stats.empty_pixels},
        {"minValue", stats.min_value},
        {"maxValue", stats.max_value},
    });
}

Json channel_stats_array_to_json(const std::array<ChannelStats, rgbwsv_channel_count>& stats) {
    Json::Object object;
    for (std::size_t i{0}; i < stats.size(); ++i) {
        object.emplace(channel_names.at(i), channel_stats_to_json(stats.at(i)));
    }
    return Json{object};
}

Json channel_order_json() {
    return Json::array({"R", "G", "B", "W", "S", "V"});
}

Json support_connectivity_to_json(const SupportConnectivityDiagnostics& diagnostics) {
    constexpr int tiny_component_area_px{8};
    constexpr int small_component_area_px{512};
    Json::Array components;
    for (const SupportComponentSummary& component : diagnostics.components) {
        components.push_back(Json::object({
            {"areaPx", component.area_px},
            {"bbox",
             Json::object({
                 {"minX", component.min_x},
                 {"minY", component.min_y},
                 {"maxX", component.max_x},
                 {"maxY", component.max_y},
             })},
        }));
    }
    return Json::object({
        {"enabled", diagnostics.enabled},
        {"componentCount", diagnostics.component_count},
        {"largestComponentPixels", diagnostics.largest_component_pixels},
        {"smallComponentCount", diagnostics.small_component_count},
        {"tinyComponentCount", diagnostics.tiny_component_count},
        {"tinyComponentAreaPx", tiny_component_area_px},
        {"smallComponentAreaPx", small_component_area_px},
        {"components", Json{components}},
    });
}

Json support_connectivity_summary_to_json(const std::vector<LayerDiagnostics>& diagnostics) {
    int layers_with_support_components{0};
    int layers_with_fragmentation{0};
    int max_component_count{0};
    int layer_with_max_component_count{-1};
    int max_small_component_count{0};
    int max_tiny_component_count{0};
    bool enabled{false};
    for (const LayerDiagnostics& layer : diagnostics) {
        const SupportConnectivityDiagnostics& support = layer.support_connectivity;
        enabled = enabled || support.enabled;
        if (!support.enabled || support.component_count <= 0) {
            continue;
        }
        ++layers_with_support_components;
        if (support.component_count > 1) {
            ++layers_with_fragmentation;
        }
        if (support.component_count > max_component_count) {
            max_component_count = support.component_count;
            layer_with_max_component_count = layer.layer_index;
        }
        max_small_component_count = std::max(max_small_component_count, support.small_component_count);
        max_tiny_component_count = std::max(max_tiny_component_count, support.tiny_component_count);
    }
    return Json::object({
        {"enabled", enabled},
        {"layersWithSupportComponents", layers_with_support_components},
        {"layersWithFragmentation", layers_with_fragmentation},
        {"maxComponentCount", max_component_count},
        {"layerWithMaxComponentCount", layer_with_max_component_count},
        {"maxSmallComponentCount", max_small_component_count},
        {"maxTinyComponentCount", max_tiny_component_count},
    });
}

Json semantic_stats_to_json(const LayerSemanticStats& stats) {
    return Json::object({
        {"textureSurfacePixels", stats.texture_surface_pixels},
        {"modelFillPixels", stats.model_fill_pixels},
        {"supportPixels", stats.support_pixels},
        {"internalVoidSupportPixels", stats.internal_void_support_pixels},
        {"outerVarnishPixels", stats.outer_varnish_pixels},
        {"outerSurfaceVarnishPixels", stats.outer_surface_varnish_pixels},
        {"innerSurfaceVarnishPixels", stats.inner_surface_varnish_pixels},
    });
}

Json BuildCrossSectionStackEntry(
    const int order,
    const std::string& id,
    const std::string& label,
    const std::string& channel,
    const std::string& semantic,
    const std::string& reportField,
    const std::uint64_t printPixels,
    const bool enabled,
    const std::string& sourceMask,
    const std::string& note)
{
    return Json::object({
        {"order", order},
        {"id", id},
        {"label", label},
        {"channel", channel},
        {"semantic", semantic},
        {"reportField", reportField},
        {"printPixels", printPixels},
        {"present", printPixels > 0U},
        {"enabled", enabled},
        {"sourceMask", sourceMask},
        {"note", note},
    });
}

Json BuildCrossSectionMaterialStackReport(
    const SliceConfig& config,
    const LayerSemanticStats& semanticStats,
    const SupportGenerationResult& supportGeneration,
    const SupportPlacementPolicy& supportPlacementPolicy)
{
    const std::uint64_t textureSurfacePixels =
        static_cast<std::uint64_t>(std::max(0, semanticStats.texture_surface_pixels));
    const std::uint64_t modelFillPixels =
        static_cast<std::uint64_t>(std::max(0, semanticStats.model_fill_pixels));
    const std::uint64_t outerSurfaceVarnishPixels =
        static_cast<std::uint64_t>(std::max(0, semanticStats.outer_surface_varnish_pixels));
    const std::uint64_t innerSurfaceVarnishPixels =
        static_cast<std::uint64_t>(std::max(0, semanticStats.inner_surface_varnish_pixels));
    const std::uint64_t upperSurfaceSupportPixels =
        static_cast<std::uint64_t>(std::max(0, supportGeneration.upper_projection_support_pixels));
    const std::uint64_t lowerSurfaceSupportPixels =
        static_cast<std::uint64_t>(std::max(0, supportGeneration.bottom_projection_support_pixels));
    const bool surfaceColorEnabled =
        config.texture.enabled
        || (config.material_process_profile.enabled && config.material_process_profile.rgb.enabled)
        || (config.material_policy.enabled && config.material_policy.rgb.enabled);

    Json::Array stack;
    stack.push_back(BuildCrossSectionStackEntry(
        1,
        "upper_surface_support",
        "上表面支撑",
        "S",
        "UpperSurfaceSupportMask",
        "upperSurfaceSupportPixels",
        upperSurfaceSupportPixels,
        supportPlacementPolicy.upper_enabled,
        "support_type.upper_projection",
        "模型外部可剥离支撑；启用 outerVarnish 时应位于外侧光油边界之外"));
    stack.push_back(BuildCrossSectionStackEntry(
        2,
        "outer_surface_varnish",
        "表面层光油",
        "V",
        "OuterSurfaceVarnishMask",
        "outerSurfaceVarnishPixels",
        outerSurfaceVarnishPixels,
        config.surface_varnish.enabled && config.surface_varnish.outer_surface,
        "surfaceVarnish.outerSurface",
        "写在模型外表面像素上的 V 通道；不同于扩张模型 XY 的 outerVarnish shell"));
    stack.push_back(BuildCrossSectionStackEntry(
        3,
        "outer_surface_color",
        "模型表层色彩层",
        "RGB",
        "OuterSurfaceColorMask",
        "textureSurfacePixels",
        textureSurfacePixels,
        surfaceColorEnabled,
        "textureSurfacePixels",
        "当前 legacy 统计未拆分外/内表面 RGB，使用 textureSurfacePixels 解释表面色彩层"));
    stack.push_back(BuildCrossSectionStackEntry(
        4,
        "model_fill",
        "模型内部填充层",
        config.model_fill.material == "varnish" ? "V" : (config.model_fill.material == "rgb" ? "RGB" : "W"),
        "ModelFillMask",
        "modelFillPixels",
        modelFillPixels,
        config.model_fill.enabled,
        "modelFill",
        "生产 Profile 不允许内部填充为空；默认材料为 white"));
    stack.push_back(BuildCrossSectionStackEntry(
        5,
        "inner_surface_color",
        "模型内表层色彩层",
        "RGB",
        "InnerSurfaceColorMask",
        "textureSurfacePixels",
        textureSurfacePixels,
        surfaceColorEnabled,
        "textureSurfacePixels",
        "当前 legacy 统计未拆分外/内表面 RGB，使用同一 textureSurfacePixels 解释内表层色彩"));
    stack.push_back(BuildCrossSectionStackEntry(
        6,
        "inner_surface_varnish",
        "模型内表面光油层",
        "V",
        "InnerSurfaceVarnishMask",
        "innerSurfaceVarnishPixels",
        innerSurfaceVarnishPixels,
        config.surface_varnish.enabled && config.surface_varnish.inner_surface,
        "surfaceVarnish.innerSurface",
        "写在模型内表面像素上的 V 通道，主要用于解释真实 RIP 横截面内侧清漆带"));
    stack.push_back(BuildCrossSectionStackEntry(
        7,
        "lower_surface_support",
        "模型下表面支撑层",
        "S",
        "LowerSurfaceSupportMask",
        "supportTypeStats.bottom_projection",
        lowerSurfaceSupportPixels,
        supportPlacementPolicy.lower_enabled,
        "support_type.bottom_projection",
        "模型外部可剥离下表面支撑"));

    Json::Array missing;
    if (upperSurfaceSupportPixels == 0U)
    {
        missing.push_back("upper_surface_support");
    }
    if (outerSurfaceVarnishPixels == 0U)
    {
        missing.push_back("outer_surface_varnish");
    }
    if (textureSurfacePixels == 0U)
    {
        missing.push_back("surface_color");
    }
    if (modelFillPixels == 0U)
    {
        missing.push_back("model_fill");
    }
    if (innerSurfaceVarnishPixels == 0U)
    {
        missing.push_back("inner_surface_varnish");
    }
    if (lowerSurfaceSupportPixels == 0U)
    {
        missing.push_back("lower_surface_support");
    }

    const bool hasRequiredStack = missing.empty();
    return Json::object({
        {"schema", "p0.cross_section_material_stack.1"},
        {"reference",
         Json::object({
             {"diagram", "docs/slice/DOC/DIAGRAM_12A_指甲模型横截面材料示意图.png"},
             {"realRipLayer", "slice.446.png"},
             {"review", "docs/slice/DOC/DOC_REVIEW_12A_真实RIP横截面示意图对齐审查.md"},
             {"oldConceptDiagramGeometryAcceptance", "deprecated"},
         })},
        {"stackOrder",
         "UpperSurfaceSupport>SurfaceVarnish>SurfaceColor>ModelFill>SurfaceColor>InnerSurfaceVarnish>LowerSupport"},
        {"note", "该材料栈描述模型横截面材料关系，不表示画布坐标上下方向，也不实现 RIP 半色调。"},
        {"outerInnerColorSplit",
         Json::object({
             {"available", false},
             {"source", "textureSurfacePixels"},
             {"reason", "当前 legacy pipeline 只统计总 texture surface RGB，尚未拆分 outer/inner surface RGB mask"},
         })},
        {"outerVarnishShell",
         Json::object({
             {"enabled", config.outer_varnish.enabled},
             {"printPixels", static_cast<std::uint64_t>(std::max(0, semanticStats.outer_varnish_pixels))},
             {"note", "outerVarnishShell 是模型外侧扩张光油壳层，不等同于真实横截面中的表面/内表面光油带"},
         })},
        {"summary",
         Json::object({
             {"canExplainRealRipCrossSection", hasRequiredStack},
             {"missingElements", Json{missing}},
             {"modelFillMaterial", config.model_fill.material},
             {"supportPlacement", supportPlacementPolicy.effective_placement},
             {"semanticPriority", "Model>OuterVarnishShell>Support>Empty"},
         })},
        {"stack", Json{stack}},
    });
}

Json BuildSingleMaterialConsistencyHint(const SliceConfig& config)
{
    const std::string profileKind = config.texture.enabled ? "color_texture" : "single_material";
    return Json::object({
        {"schema", "p0.single_material_consistency_hint.1"},
        {"profileKind", profileKind},
        {"pairComparisonRequired", true},
        {"comparisonStatus", "not_evaluated_in_single_package"},
        {"modelSemanticComparable", true},
        {"singleMaterialConsistency", "requires_pair_comparison"},
        {"geometryComparableFields",
         Json::array({
             "grid.widthPx",
             "grid.heightPx",
             "grid.layerCount",
             "layers[].zMm",
             "layers[].modelPixels",
             "layers[].supportPixels",
             "layers[].supportTypeStats",
             "layers[].outerVarnishPixels",
             "layers[].outerSurfaceVarnishPixels",
             "layers[].innerSurfaceVarnishPixels",
         })},
        {"allowedMaterialDifferences",
         Json::array({
             "textureSurfacePixels",
             "modelFillPixels",
             "rgbPrintPixels",
             "whitePrintPixels",
             "varnishPrintPixels",
             "texture_report.*",
         })},
        {"note", "Use a paired color/single-material fixture to assert geometry, support and layer-order consistency."},
    });
}

Json layer_diagnostics_to_json(const LayerDiagnostics& diagnostics) {
    Json::Array fill_warnings;
    if (diagnostics.odd_intersection_rows > 0) {
        fill_warnings.push_back("odd_scanline_intersections");
    }
    return Json::object({
        {"layerIndex", diagnostics.layer_index},
        {"zMm", diagnostics.z_mm},
        {"segmentCount", diagnostics.segment_count},
        {"openSegmentWarnings", diagnostics.odd_intersection_rows},
        {"filledSpans", diagnostics.filled_spans},
        {"fillWarnings", Json{fill_warnings}},
        {"modelNonZeroPixels", diagnostics.model_pixels},
        {"supportNonZeroPixels", diagnostics.support_pixels},
        {"textureSurfacePixels", diagnostics.semantic.texture_surface_pixels},
        {"modelFillPixels", diagnostics.semantic.model_fill_pixels},
        {"supportPixels", diagnostics.semantic.support_pixels},
        {"internalVoidSupportPixels", diagnostics.semantic.internal_void_support_pixels},
        {"upperSurfaceSupportPixels", diagnostics.upper_projection_support_pixels},
        {"outerVarnishPixels", diagnostics.semantic.outer_varnish_pixels},
        {"rgbNonZeroPixels", diagnostics.rgb_non_zero_pixels},
        {"whiteNonZeroPixels", diagnostics.white_non_zero_pixels},
        {"modelPrintPixels", diagnostics.model_pixels},
        {"supportPrintPixels", diagnostics.support_non_zero_pixels},
        {"rgbPrintPixels", diagnostics.rgb_non_zero_pixels},
        {"whitePrintPixels", diagnostics.white_non_zero_pixels},
        {"varnishPrintPixels", diagnostics.varnish_non_zero_pixels},
        {"varnishNonZeroPixels", diagnostics.varnish_non_zero_pixels},
        {"islandCount", diagnostics.island_count},
        {"islandPixels", diagnostics.island_pixels},
        {"unsupportedPixels", diagnostics.unsupported_pixels},
        {"filteredIslandCount", diagnostics.filtered_island_count},
        {"filteredIslandPixels", diagnostics.filtered_island_pixels},
        {"supportTypeStats",
         Json::object({
             {"bottom_projection", diagnostics.bottom_projection_support_pixels},
             {"unsupported_island", diagnostics.unsupported_island_support_pixels},
             {"full_vertical_projection", diagnostics.full_vertical_projection_support_pixels},
             {"internal_void", diagnostics.internal_void_support_pixels},
             {"upper_projection", diagnostics.upper_projection_support_pixels},
        })},
        {"supportConnectivity", support_connectivity_to_json(diagnostics.support_connectivity)},
        {"semantic", semantic_stats_to_json(diagnostics.semantic)},
        {"channelStats", channel_stats_array_to_json(diagnostics.channel_stats)},
    });
}

Json texture_report_to_json(const TextureReportData& report) {
    return Json::object({
        {"enabled", report.enabled},
        {"applyMode", report.apply_mode},
        {"nonSurfaceRgbPolicy", report.non_surface_rgb_policy},
        {"topSurfaceLayers", report.top_surface_layers},
        {"source", report.source},
        {"materials", Json{report.materials}},
        {"textureFiles", Json{report.texture_files}},
        {"loadedTextures", report.loaded_textures},
        {"missingTextures", report.missing_textures},
        {"stats",
         Json::object({
             {"facesWithUv", report.faces_with_uv},
             {"facesWithoutUv", report.faces_without_uv},
             {"sampledPixels", report.sampled_pixels},
             {"fallbackPixels", report.fallback_pixels},
             {"uvOutOfRangePixels", report.uv_out_of_range_pixels},
         })},
        {"sampledPixels", report.sampled_pixels},
        {"fallbackPixels", report.fallback_pixels},
        {"uvOutOfRangePixels", report.uv_out_of_range_pixels},
        {"warnings", Json{report.warnings}},
    });
}

Json material_policy_report_to_json(const SliceConfig& config, const MaterialPolicyReportData& report) {
    return Json::object({
        {"enabled", config.material_policy.enabled},
        {"conflictPolicy", config.material_policy.conflict_policy},
        {"rgb",
         Json::object({
             {"enabled", config.material_policy.rgb.enabled},
             {"source", config.material_policy.rgb.source},
             {"printPixels", report.rgb_print_pixels},
         })},
        {"white",
         Json::object({
             {"enabled", config.material_policy.white.enabled},
             {"mode", config.material_policy.white.mode},
             {"layers", config.material_policy.white.layers},
             {"value", static_cast<int>(config.material_policy.white.value)},
             {"printPixels", report.white_print_pixels},
         })},
        {"varnish",
         Json::object({
             {"enabled", config.material_policy.varnish.enabled},
             {"mode", config.material_policy.varnish.mode},
             {"topLayers", config.material_policy.varnish.top_layers},
             {"value", static_cast<int>(config.material_policy.varnish.value)},
             {"printPixels", report.varnish_print_pixels},
         })},
        {"warnings", Json{report.warnings}},
    });
}

double coverage_ratio(const std::uint64_t print_pixels, const std::uint64_t denominator) {
    if (denominator == 0U) {
        return 0.0;
    }
    return static_cast<double>(print_pixels) / static_cast<double>(denominator);
}

Json material_process_report_to_json(
    const SliceConfig& config,
    const ModelReport& model_report,
    const GridSpec& grid,
    const std::vector<LayerDiagnostics>& diagnostics,
    const std::array<ChannelStats, rgbwsv_channel_count>& total_channel_stats) {
    const MaterialProcessProfileConfig& profile = config.material_process_profile;
    const std::uint64_t total_pixels =
        static_cast<std::uint64_t>(grid.width_px) * static_cast<std::uint64_t>(grid.height_px)
        * static_cast<std::uint64_t>(grid.layer_count);
    std::uint64_t rgb_print_pixels{0};
    const std::uint64_t white_print_pixels = total_channel_stats.at(3).print_pixels;
    const std::uint64_t support_print_pixels = total_channel_stats.at(4).print_pixels;
    const std::uint64_t varnish_print_pixels = total_channel_stats.at(5).print_pixels;

    Json::Array layers;
    Json::Array varnish_active_layer_indices;
    int first_varnish_layer{-1};
    int last_varnish_layer{-1};
    for (const LayerDiagnostics& layer : diagnostics) {
        const std::uint64_t layer_rgb = static_cast<std::uint64_t>(layer.rgb_non_zero_pixels);
        const std::uint64_t layer_white = layer.channel_stats.at(3).print_pixels;
        const std::uint64_t layer_support = layer.channel_stats.at(4).print_pixels;
        const std::uint64_t layer_varnish = layer.channel_stats.at(5).print_pixels;
        rgb_print_pixels += layer_rgb;
        if (layer_varnish > 0U) {
            varnish_active_layer_indices.push_back(layer.layer_index);
            if (first_varnish_layer < 0) {
                first_varnish_layer = layer.layer_index;
            }
            last_varnish_layer = layer.layer_index;
        }
        layers.push_back(Json::object({
            {"layerIndex", layer.layer_index},
            {"rgbPrintPixels", layer_rgb},
            {"whitePrintPixels", layer_white},
            {"varnishPrintPixels", layer_varnish},
            {"supportPrintPixels", layer_support},
        }));
    }

    const std::uint64_t missing_underbase_pixels =
        white_print_pixels < rgb_print_pixels ? rgb_print_pixels - white_print_pixels : 0U;
    constexpr std::uint64_t unexpected_overlap_pixels{0U};

    Json::Array validation_failures;
    Json::Array warnings;
    if (profile.enabled) {
        if (profile.validation.require_rgb_pixels && rgb_print_pixels == 0U) {
            validation_failures.push_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_RGB");
        }
        if (profile.validation.require_white_pixels && white_print_pixels == 0U) {
            validation_failures.push_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_WHITE");
        }
        if (profile.validation.require_varnish_pixels && varnish_print_pixels == 0U) {
            validation_failures.push_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_VARNISH");
        }
        if (profile.validation.require_support_pixels && support_print_pixels == 0U) {
            validation_failures.push_back("E_MATERIAL_PROCESS_PROFILE_EMPTY_SUPPORT");
        }
        if (unexpected_overlap_pixels
            > static_cast<std::uint64_t>(profile.validation.max_unexpected_overlap_pixels)) {
            validation_failures.push_back("E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_OVERLAP");
        }
        if (profile.white.enabled && profile.white.mode == "underbase" && missing_underbase_pixels > 0U) {
            validation_failures.push_back("E_MATERIAL_PROCESS_PROFILE_UNDERBASE_COVERAGE_LOW");
        }
        if (profile.varnish.enabled && profile.varnish.mode == "top_n_layers" && first_varnish_layer >= 0) {
            (void)last_varnish_layer;
            if (static_cast<int>(varnish_active_layer_indices.size()) > profile.varnish.top_layers) {
                validation_failures.push_back("E_MATERIAL_PROCESS_PROFILE_UNEXPECTED_VARNISH_LAYER");
            }
        }
        if (config.material_policy.enabled == false && config.material_role_mapping.enabled == false) {
            warnings.push_back("materialProcessProfile is report-only; no materialPolicy or materialRoleMapping is enabled");
        }
    }

    return Json::object({
        {"enabled", profile.enabled},
        {"profileName", profile.name},
        {"target", profile.target},
        {"inputFormat", model_report.format},
        {"sourceModel", model_report.model_path.generic_string()},
        {"grid",
         Json::object({
             {"widthPx", grid.width_px},
             {"heightPx", grid.height_px},
             {"layerCount", grid.layer_count},
             {"pixelSizeMm", Json::array({grid.pixel_size_x_mm, grid.pixel_size_y_mm})},
             {"layerThicknessMm", config.output.layer_thickness_mm},
         })},
        {"layerCount", grid.layer_count},
        {"rgb",
         Json::object({
             {"enabled", profile.rgb.enabled},
             {"source", profile.rgb.source},
             {"printPixels", rgb_print_pixels},
             {"coverageRatio", coverage_ratio(rgb_print_pixels, total_pixels)},
         })},
        {"white",
         Json::object({
             {"enabled", profile.white.enabled},
             {"mode", profile.white.mode},
             {"coverage", profile.white.coverage},
             {"value", static_cast<int>(profile.white.value)},
             {"expandPx", profile.white.expand_px},
             {"shrinkPx", profile.white.shrink_px},
             {"printPixels", white_print_pixels},
             {"coverageRatio", coverage_ratio(white_print_pixels, total_pixels)},
             {"missingUnderbasePixels", missing_underbase_pixels},
         })},
        {"varnish",
         Json::object({
             {"enabled", profile.varnish.enabled},
             {"mode", profile.varnish.mode},
             {"topLayers", profile.varnish.top_layers},
             {"value", static_cast<int>(profile.varnish.value)},
             {"coverage", profile.varnish.coverage},
             {"printPixels", varnish_print_pixels},
             {"coverageRatio", coverage_ratio(varnish_print_pixels, total_pixels)},
             {"activeLayerIndices", Json{varnish_active_layer_indices}},
         })},
        {"support",
         Json::object({
             {"expected", profile.support.expected},
             {"mode", profile.support.mode},
             {"printPixels", support_print_pixels},
             {"coverageRatio", coverage_ratio(support_print_pixels, total_pixels)},
         })},
        {"unexpectedOverlapPixels", unexpected_overlap_pixels},
        {"layers", Json{layers}},
        {"validation",
         Json::object({
             {"pass", validation_failures.empty()},
             {"failures", Json{validation_failures}},
         })},
        {"warnings", Json{warnings}},
    });
}

Json material_role_mapping_report_to_json(const MaterialRoleMappingReportData& report) {
    return Json::object({
        {"enabled", report.enabled},
        {"inputFormat", report.input_format},
        {"rules", Json{report.rules}},
        {"defaultRole", report.default_role},
        {"allowInputSupportMaterial", report.allow_input_support_material},
        {"materialCount", report.material_count},
        {"mappedRgb", report.mapped_rgb},
        {"mappedWhite", report.mapped_white},
        {"mappedVarnish", report.mapped_varnish},
        {"mappedIgnore", report.mapped_ignore},
        {"mappedSupportCandidate", report.mapped_support_candidate},
        {"mappedSupport", report.mapped_support},
        {"facesWithMappedMaterial", report.faces_with_mapped_material},
        {"facesWithoutMappedMaterial", report.faces_without_mapped_material},
        {"materials", Json{report.materials}},
        {"stats",
         Json::object({
             {"materialCount", report.material_count},
             {"mappedRgb", report.mapped_rgb},
             {"mappedWhite", report.mapped_white},
             {"mappedVarnish", report.mapped_varnish},
             {"mappedIgnore", report.mapped_ignore},
             {"mappedSupportCandidate", report.mapped_support_candidate},
             {"mappedSupport", report.mapped_support},
             {"facesWithMappedMaterial", report.faces_with_mapped_material},
             {"facesWithoutMappedMaterial", report.faces_without_mapped_material},
         })},
        {"warnings", Json{report.warnings}},
    });
}

Json obj_mtl_material_report_to_json(const ModelReport& model_report) {
    Json::Array materials;
    Json::Array textures;
    for (const MaterialInfo& material : model_report.material_infos) {
        materials.push_back(Json::object({
            {"name", material.name},
            {"hasDiffuse", material.has_diffuse},
            {"diffuseRgb", rgb_to_json(material.diffuse_rgb)},
            {"hasTexture", material.has_texture},
            {"texturePath", material.diffuse_texture_path.generic_string()},
            {"textureExists", material.texture_exists},
        }));
        if (material.has_texture) {
            textures.push_back(material.diffuse_texture_path.generic_string());
        }
    }
    int faces_with_material{0};
    int faces_without_material{0};
    for (const TriangleTextureInfo& triangle : model_report.triangle_textures) {
        if (triangle.material_name.empty()) {
            ++faces_without_material;
        } else {
            ++faces_with_material;
        }
    }
    return Json::object({
        {"inputFormat", model_report.format},
        {"materialCount", static_cast<int>(model_report.material_infos.size())},
        {"materials", Json{materials}},
        {"facesWithMaterial", faces_with_material},
        {"facesWithoutMaterial", faces_without_material},
        {"textures", Json{textures}},
    });
}

Json three_mf_report_to_json(const ModelReport& model_report) {
    Json::Array unsupported_resources;
    for (const std::string& resource : model_report.three_mf.unsupported_resources) {
        unsupported_resources.push_back(resource);
    }
    Json::Array unsupported_extensions;
    for (const std::string& extension : model_report.three_mf.unsupported_extensions) {
        unsupported_extensions.push_back(extension);
    }
    Json::Array warnings;
    for (const std::string& warning : model_report.three_mf.warnings) {
        warnings.push_back(warning);
    }
    Json::Array errors;
    for (const std::string& error : model_report.three_mf.errors) {
        errors.push_back(error);
    }
    return Json::object({
        {"enabled", model_report.three_mf.enabled},
        {"packagePath", model_report.three_mf.package_path.generic_string()},
        {"modelPartPath", model_report.three_mf.model_part_path},
        {"unit", model_report.three_mf.unit},
        {"unitScaleToMm", model_report.three_mf.unit_scale_to_mm},
        {"zipCompressionStats",
         Json::object({
             {"entryCount", model_report.three_mf.entry_count},
             {"storedEntryCount", model_report.three_mf.stored_entry_count},
             {"deflatedEntryCount", model_report.three_mf.deflated_entry_count},
             {"totalUncompressedBytes", static_cast<double>(model_report.three_mf.total_uncompressed_bytes)},
         })},
        {"entryCount", model_report.three_mf.entry_count},
        {"totalUncompressedBytes", static_cast<double>(model_report.three_mf.total_uncompressed_bytes)},
        {"zip",
         Json::object({
             {"entryCount", model_report.three_mf.entry_count},
             {"storedEntryCount", model_report.three_mf.stored_entry_count},
             {"deflatedEntryCount", model_report.three_mf.deflated_entry_count},
             {"totalUncompressedBytes", static_cast<double>(model_report.three_mf.total_uncompressed_bytes)},
         })},
        {"xml",
         Json::object({
             {"parser", model_report.three_mf.xml_parser},
             {"allowExternalEntities", false},
             {"parseWarnings", Json::array({})},
         })},
        {"xmlParser", model_report.three_mf.xml_parser},
        {"validation",
         Json::object({
             {"invalidReferenceCount", model_report.three_mf.invalid_reference_count},
             {"unknownMaterialCount", model_report.three_mf.unknown_material_count},
             {"ignoredResourceCount", model_report.three_mf.ignored_resource_count},
         })},
        {"colorGroups",
         Json::object({
             {"count", model_report.three_mf.color_group_count},
             {"colorCount", model_report.three_mf.color_count},
             {"resolvedTriangles", model_report.three_mf.color_group_resolved_triangles},
             {"interpolatedColorFallbackCount", model_report.three_mf.interpolated_color_fallback_count},
         })},
        {"textures",
         Json::object({
             {"texture2dCount", model_report.three_mf.texture2d_count},
             {"texture2dGroupCount", model_report.three_mf.texture2d_group_count},
             {"tex2CoordCount", model_report.three_mf.tex2coord_count},
             {"resourceCount", model_report.three_mf.texture_resource_count},
             {"loadedCount", model_report.three_mf.texture_loaded_count},
             {"missingCount", model_report.three_mf.texture_missing_count},
             {"sampledPixels", static_cast<double>(model_report.three_mf.texture_sampled_pixels)},
             {"resolvedTriangles", model_report.three_mf.texture_group_resolved_triangles},
         })},
        {"colorGroupCount", model_report.three_mf.color_group_count},
        {"colorCount", model_report.three_mf.color_count},
        {"texture2dCount", model_report.three_mf.texture2d_count},
        {"texture2dGroupCount", model_report.three_mf.texture2d_group_count},
        {"tex2CoordCount", model_report.three_mf.tex2coord_count},
        {"textureResourceCount", model_report.three_mf.texture_resource_count},
        {"textureLoadedCount", model_report.three_mf.texture_loaded_count},
        {"textureMissingCount", model_report.three_mf.texture_missing_count},
        {"textureSampledPixels", static_cast<double>(model_report.three_mf.texture_sampled_pixels)},
        {"colorGroupResolvedTriangles", model_report.three_mf.color_group_resolved_triangles},
        {"textureGroupResolvedTriangles", model_report.three_mf.texture_group_resolved_triangles},
        {"interpolatedColorFallbackCount", model_report.three_mf.interpolated_color_fallback_count},
        {"invalidReferenceCount", model_report.three_mf.invalid_reference_count},
        {"ignoredResourceCount", model_report.three_mf.ignored_resource_count},
        {"objectCount", model_report.three_mf.object_count},
        {"componentCount", model_report.three_mf.component_count},
        {"meshObjectCount", model_report.three_mf.mesh_object_count},
        {"triangleCount", model_report.three_mf.triangle_count},
        {"materialResourceCount", model_report.three_mf.material_resource_count},
        {"unsupportedResources", Json{unsupported_resources}},
        {"unsupportedExtensions", Json{unsupported_extensions}},
        {"warnings", Json{warnings}},
        {"errors", Json{errors}},
    });
}

Json relief_report_to_json(
    const SliceConfig& config,
    const ReliefReportData& relief_report,
    const int support_pixels,
    const int columns_with_support) {
    const double coverage_ratio = relief_report.total_columns > 0
        ? static_cast<double>(relief_report.hit_columns) / static_cast<double>(relief_report.total_columns)
        : 0.0;
    return Json::object({
        {"slicingMode", config.slicing_mode},
        {"fillMode", config.relief.fill_mode},
        {"baseZMm", config.relief.base_z_mm},
        {"support",
         Json::object({
             {"enabled", config.support.enabled},
             {"source", config.slicing_mode == "relief_heightfield" ? "relief_lower_surface" : "first_model_layer"},
             {"expectedSupport", config.slicing_mode == "relief_heightfield" && config.support.enabled},
             {"supportPixels", support_pixels},
             {"columnsWithSupport", columns_with_support},
         })},
        {"columns",
         Json::object({
             {"total", relief_report.total_columns},
             {"hit", relief_report.hit_columns},
             {"empty", relief_report.empty_columns},
             {"multiHit", relief_report.multi_hit_columns},
             {"coverageRatio", coverage_ratio},
         })},
        {"height",
         Json::object({
             {"zMinMm", relief_report.has_hits ? relief_report.z_min_mm : 0.0},
             {"zMaxMm", relief_report.has_hits ? relief_report.z_max_mm : 0.0},
             {"thicknessMinMm", relief_report.has_hits ? relief_report.thickness_min_mm : 0.0},
             {"thicknessMaxMm", relief_report.has_hits ? relief_report.thickness_max_mm : 0.0},
         })},
        {"zRangeMm",
         Json::object({
             {"min", relief_report.has_hits ? relief_report.z_min_mm : 0.0},
             {"max", relief_report.has_hits ? relief_report.z_max_mm : 0.0},
         })},
        {"warnings", Json{relief_report.warnings}},
    });
}

bool IsOpenVdbCandidateConfig(const SliceConfig& config)
{
    return config.texture.apply_mode == "surface_shell_from_sdf"
        || config.experimental.openvdb_pipeline.write_production_rgbwsv;
}

void EnsureLegacyPipelineAcceptsConfig(const SliceConfig& config)
{
    if (!IsOpenVdbCandidateConfig(config))
    {
        return;
    }

    throw std::runtime_error(
        "OpenVDB candidate slicing requires the explicit --openvdb-candidate-slice path; "
        "the legacy production path must not run surface_shell_from_sdf or writeProductionRgbwsv configs");
}

}  // namespace

SliceRunResult run_slicer(const std::filesystem::path& config_path) {
    return run_slicer(config_path, SliceRunOptions{});
}

SliceRunResult run_slicer(const std::filesystem::path& config_path, const SliceRunOptions& options) {
    SliceRunProfile profile;
    profile.available = true;
    profile.profile_level = "coarse";
    const auto run_start = SlicerClock::now();
    auto phase_start = run_start;

    NotifyProgress(options, run_start, "config_load", 0, 1, 0);
    const SliceConfig config = load_slice_config(config_path);
    profile.config_load_ms = ElapsedMsSince(phase_start);
    EnsureGlobalTextureFillPartitionBackendAvailable(config);
    NotifyProgress(options, run_start, "model_load", 0, 1, 3);
    phase_start = SlicerClock::now();

    EnsureLegacyPipelineAcceptsConfig(config);
    const std::filesystem::path config_dir =
        config_path.parent_path().empty() ? std::filesystem::current_path() : config_path.parent_path();
    ModelReport model_report = load_model_report(config, config_dir);
    profile.model_load_ms = ElapsedMsSince(phase_start);
    NotifyProgress(options, run_start, "grid_setup", 0, 1, 10);
    phase_start = SlicerClock::now();

    const GridSpec grid = make_grid_spec(config, model_report.bbox_mm);

    const std::filesystem::path package_dir = config.output.package_dir;
    if (options.write_tiff_layers || options.write_preview_files || options.write_reports) {
        std::filesystem::create_directories(package_dir);
    }
    if (options.write_tiff_layers) {
        std::filesystem::create_directories(package_dir / "layers");
    }
    if (options.write_reports) {
        std::filesystem::create_directories(package_dir / "reports");
    }
    if (config.preview.enabled && options.write_preview_files) {
        std::filesystem::create_directories(package_dir / "preview");
    }

    TiffImageSpec tiff_spec;
    tiff_spec.width = static_cast<std::uint32_t>(grid.width_px);
    tiff_spec.height = static_cast<std::uint32_t>(grid.height_px);
    tiff_spec.tile_width = static_cast<std::uint32_t>(config.output.tile_size.at(0));
    tiff_spec.tile_height = static_cast<std::uint32_t>(config.output.tile_size.at(1));
    tiff_spec.rows_per_strip = static_cast<std::uint32_t>(config.output.rows_per_strip);
    tiff_spec.storage_mode =
        config.output.storage_mode == "tiled" ? TiffStorageMode::Tiled : TiffStorageMode::Stripped;
    profile.grid_setup_ms = ElapsedMsSince(phase_start);
    NotifyProgress(options, run_start, "mask_sampling", 0, 1, 12);
    phase_start = SlicerClock::now();

    std::vector<LayerDiagnostics> layer_diagnostics;
    ReliefReportData relief_report;
    std::vector<ReliefColumnInfo> relief_columns;
    std::vector<std::vector<std::uint8_t>> model_masks;
    if (config.slicing_mode == "relief_heightfield") {
        ReliefSamplingResult relief_sampling = sample_relief_heightfield_masks(config, model_report, grid, layer_diagnostics);
        model_masks = std::move(relief_sampling.model_masks);
        relief_columns = std::move(relief_sampling.columns);
        relief_report = std::move(relief_sampling.report);
    } else {
        model_masks = sample_model_masks(model_report, grid, config.output.layer_thickness_mm, layer_diagnostics);
        relief_report.total_columns = grid.width_px * grid.height_px;
        relief_report.empty_columns = relief_report.total_columns;
    }
    const std::vector<int> support_source_layers = config.slicing_mode == "relief_heightfield"
        ? compute_relief_lower_layers(relief_columns)
        : compute_first_model_layers(model_masks, grid);
    profile.mask_sampling_ms = ElapsedMsSince(phase_start);
    NotifyProgress(options, run_start, "texture_prepare", 0, 1, 28);
    phase_start = SlicerClock::now();

    TextureRuntime texture_runtime = prepare_texture_runtime(config, model_report);
    if (model_report.format == "3mf") {
        int internal_loaded{0};
        int internal_missing{0};
        for (const auto& [unused_name, material] : texture_runtime.materials) {
            (void)unused_name;
            if (!material.material.has_texture || material.material.texture_source != "3mf_internal") {
                continue;
            }
            if (material.loaded) {
                ++internal_loaded;
            } else {
                ++internal_missing;
            }
        }
        if (internal_loaded + internal_missing > 0) {
            model_report.three_mf.texture_loaded_count = internal_loaded;
            model_report.three_mf.texture_missing_count = std::max(model_report.three_mf.texture_missing_count, internal_missing);
        }
    }
    std::vector<TextureColumnColor> texture_columns;
    if (config.texture.enabled && config.slicing_mode == "relief_heightfield") {
        texture_columns = build_relief_texture_columns(config, model_report, relief_columns, texture_runtime);
    }
    const MaterialRoleMappingReportData material_role_mapping_report =
        build_material_role_mapping_report(config, model_report);
    std::vector<MaterialRoleColumn> material_role_columns;
    if (config.material_role_mapping.enabled && config.slicing_mode == "relief_heightfield") {
        material_role_columns = build_material_role_columns(
            config,
            model_report,
            relief_columns,
            config.texture.enabled ? &texture_columns : nullptr);
    }
    profile.texture_prepare_ms = ElapsedMsSince(phase_start);
    NotifyProgress(options, run_start, "support_generation", 0, 1, 32);
    phase_start = SlicerClock::now();

    const std::vector<ColumnLayerRange> column_ranges = config.slicing_mode == "relief_heightfield"
        ? compute_relief_column_ranges(relief_columns)
        : compute_mask_column_ranges(model_masks, grid);
    const std::vector<std::vector<std::uint8_t>> outer_varnish_masks =
        BuildOuterVarnishMasks(config, grid, model_masks);
    const UpperSupportBoundaryInfo upper_support_boundary_info =
        ResolveUpperSupportBoundaryInfo(config);
    const std::vector<std::vector<std::uint8_t>> upper_support_boundary_masks =
        BuildUpperSupportBoundaryMasks(grid, model_masks, outer_varnish_masks, upper_support_boundary_info);
    const std::vector<ColumnLayerRange> upper_support_boundary_column_ranges =
        compute_mask_column_ranges(upper_support_boundary_masks, grid);
    const SurfaceVarnishMasks surface_varnish_masks =
        BuildSurfaceVarnishMasks(config, grid, model_masks);
    const int outerVarnishThicknessPx = ComputeOuterVarnishThicknessPx(config);
    const double outerVarnishEffectiveThicknessMm =
        ComputeOuterVarnishEffectiveThicknessMm(config, outerVarnishThicknessPx);
    const int columns_with_support = config.support.enabled
        ? static_cast<int>(std::count_if(support_source_layers.begin(), support_source_layers.end(), [](const int layer) {
              return layer > 0;
          }))
        : 0;
    SupportGenerationResult support_generation =
        generate_support_masks(
            config,
            grid,
            model_masks,
            upper_support_boundary_masks,
            support_source_layers,
            column_ranges,
            upper_support_boundary_column_ranges,
            layer_diagnostics);
    const SupportShapePolicy support_shape_policy = MakeSupportShapePolicy(config.support);
    SupportShapeOptimizationResult support_shape_result;
    if (support_shape_policy.enabled)
    {
        const std::vector<std::vector<std::uint8_t>> originalSupportMasks = support_generation.support_masks;
        support_shape_result = ApplySupportShapePolicy(
            support_shape_policy,
            model_masks,
            support_generation.support_masks,
            grid.width_px,
            grid.height_px,
            config.support.connectivity);
        if (support_shape_result.enabled)
        {
            SynchronizeSupportShapeTypeMaps(
                originalSupportMasks,
                support_generation.support_masks,
                support_generation.support_type_maps);
        }
    }
    const SupportPlacementPolicy support_placement_policy = ResolveSupportPlacementPolicy(config);
    const bool repairMaterialClosure = config.material_closure.enabled
        && config.material_closure.mode == "repair_then_report"
        && config.material_closure.repair.enabled;
    const bool collectMaterialClosureExact = config.material_closure.enabled
        && (options.write_reports || repairMaterialClosure);
    std::vector<std::vector<std::size_t>> clearedOuterVarnishSupportIndices;
    const int cleared_outer_varnish_support_pixels =
        ApplyOuterVarnishSupportPriority(
            outer_varnish_masks,
            support_generation,
            collectMaterialClosureExact ? &clearedOuterVarnishSupportIndices : nullptr);
    if (support_shape_result.enabled || cleared_outer_varnish_support_pixels > 0)
    {
        RecalculateSupportGenerationStats(support_generation, layer_diagnostics, grid, config);
    }
    profile.support_generation_ms = ElapsedMsSince(phase_start);
    NotifyProgress(options, run_start, "layer_processing", 0, grid.layer_count, 36);
    phase_start = SlicerClock::now();

    int total_model_pixels{0};
    int total_support_pixels{0};
    int total_rgb_non_zero_pixels{0};
    int total_white_non_zero_pixels{0};
    int total_support_non_zero_pixels{0};
    int total_varnish_non_zero_pixels{0};
    LayerSemanticStats total_semantic_stats;
    std::array<ChannelStats, rgbwsv_channel_count> total_channel_stats{};
    Json::Array layers;
    Json::Array slice_layers;
    Json::Array contour_layers;
    Json::Array preview_files;
    std::vector<MaterialClosureSemanticLayerResult> materialClosureExactLayers;
    if (collectMaterialClosureExact)
    {
        materialClosureExactLayers.reserve(static_cast<std::size_t>(grid.layer_count));
    }
    const bool collectMaterialClosureCandidate = config.material_closure.enabled
        && options.write_tiff_layers
        && !collectMaterialClosureExact;
    std::vector<MaterialClosureCandidateLayer> materialClosureCandidateLayers;
    if (collectMaterialClosureCandidate)
    {
        materialClosureCandidateLayers.reserve(static_cast<std::size_t>(grid.layer_count));
    }
    MaterialPolicyReportData material_policy_report;
    material_policy_report.enabled = config.material_policy.enabled;
    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index) {
        const auto layerComputeStart = SlicerClock::now();
        int layer_model_pixels{0};
        int layer_support_pixels{0};
        LayerDiagnostics& diagnostics = layer_diagnostics.at(layer_index);
        MaterialClosureSemanticLayerInput materialClosureInput;
        MaterialClosureSemanticLayerInput* materialClosureInputPointer{nullptr};
        if (collectMaterialClosureExact)
        {
            materialClosureInput = InitializeMaterialClosureSemanticInput(
                grid,
                layer_index,
                diagnostics.z_mm,
                model_masks.at(layer_index),
                support_generation.support_masks.at(layer_index),
                clearedOuterVarnishSupportIndices.at(layer_index),
                outer_varnish_masks.at(layer_index));
            materialClosureInputPointer = &materialClosureInput;
        }
        std::vector<std::uint8_t> layer = compose_layer(
            config,
            grid,
            model_masks.at(layer_index),
            outer_varnish_masks.at(layer_index),
            surface_varnish_masks.outer_surface_masks.at(layer_index),
            surface_varnish_masks.inner_surface_masks.at(layer_index),
            support_generation.support_masks.at(layer_index),
            support_generation.support_type_maps.at(layer_index),
            config.texture.enabled ? &texture_columns : nullptr,
            config.material_role_mapping.enabled ? &material_role_columns : nullptr,
            &column_ranges,
            layer_index,
            config.texture.enabled ? &texture_runtime.report : nullptr,
            config.material_policy.enabled ? &material_policy_report : nullptr,
            materialClosureInputPointer,
            diagnostics.semantic,
            layer_model_pixels,
            layer_support_pixels);
        diagnostics.model_pixels = layer_model_pixels;
        diagnostics.support_pixels = layer_support_pixels;
        if (collectMaterialClosureExact)
        {
            PopulateMaterialClosureEmptyMask(layer, materialClosureInput);
            const MaterialClosureSemanticLayerAnalysis analysis =
                AnalyzeMaterialClosureSemanticLayer(
                    materialClosureInput,
                    config.material_closure.connectivity,
                    config.material_closure.max_gap_px);
            MaterialClosureSemanticLayerResult result = analysis.summary;
            if (repairMaterialClosure)
            {
                const MaterialClosureRepairPlan repairPlan = BuildMaterialClosureRepairPlan(
                    materialClosureInput,
                    analysis,
                    config.material_closure.connectivity);
                const MaterialClosureRepairApplicationResult repairResult =
                    ApplyMaterialClosureRepair(
                        repairPlan,
                        ResolveMaterialClosureRepairValues(config),
                        layer,
                        materialClosureInput);
                const MaterialClosureSemanticLayerResult remaining =
                    DetectMaterialClosureSemanticLayer(
                        materialClosureInput,
                        config.material_closure.connectivity,
                        config.material_closure.max_gap_px);
                result.repairAttempted = true;
                result.repairedPixels = repairResult.repairedPixels;
                result.repairedColorFillPixels = repairResult.repairedColorFillPixels;
                result.repairedModelSupportPixels = repairResult.repairedModelSupportPixels;
                result.repairedInternalVoidPixels = repairResult.repairedInternalVoidPixels;
                result.repairedVarnishSupportPixels = repairResult.repairedVarnishSupportPixels;
                result.remainingGapPixels = remaining.gapPixels;
                result.remainingColorFillGapPixels = remaining.colorFillGapPixels;
                result.remainingModelSupportGapPixels = remaining.modelSupportGapPixels;
                result.remainingColorSupportGapPixels = remaining.colorSupportGapPixels;
                result.remainingInternalVoidGapPixels = remaining.internalVoidGapPixels;
                result.remainingVarnishSupportGapPixels = remaining.varnishSupportGapPixels;
                result.repairRejectedTooWidePixels = repairPlan.rejectedTooWidePixels;

                layer_model_pixels += repairResult.repairedModelFillPixels;
                layer_support_pixels += repairResult.repairedSupportPixels;
                diagnostics.model_pixels = layer_model_pixels;
                diagnostics.support_pixels = layer_support_pixels;
                diagnostics.semantic.model_fill_pixels += repairResult.repairedModelFillPixels;
                diagnostics.semantic.support_pixels += repairResult.repairedSupportPixels;
                diagnostics.semantic.internal_void_support_pixels +=
                    repairResult.repairedInternalVoidPixels;
            }
            materialClosureExactLayers.push_back(std::move(result));
        }
        update_layer_channel_stats(layer, diagnostics);
        total_model_pixels += layer_model_pixels;
        total_support_pixels += layer_support_pixels;
        total_rgb_non_zero_pixels += diagnostics.rgb_non_zero_pixels;
        total_white_non_zero_pixels += diagnostics.white_non_zero_pixels;
        total_support_non_zero_pixels += diagnostics.support_non_zero_pixels;
        total_varnish_non_zero_pixels += diagnostics.varnish_non_zero_pixels;
        merge_channel_stats(total_channel_stats, diagnostics);
        merge_semantic_stats(total_semantic_stats, diagnostics.semantic);
        profile.layer_compute_ms += ElapsedMsSince(layerComputeStart);
        const std::string relative_path = layer_file_name(layer_index);
        if (options.write_tiff_layers) {
            const auto tiffWriteStart = SlicerClock::now();
            write_rgbwsv_tiff(package_dir / relative_path, tiff_spec, layer);
            profile.tiff_write_ms += ElapsedMsSince(tiffWriteStart);
            if (collectMaterialClosureCandidate)
            {
                const auto candidateDetectionStart = SlicerClock::now();
                materialClosureCandidateLayers.push_back(DetectMaterialClosureCandidateLayer(
                    layer,
                    grid.width_px,
                    grid.height_px,
                    layer_index,
                    diagnostics.z_mm,
                    config.material_closure.connectivity,
                    config.material_closure.max_gap_px));
                profile.layer_compute_ms += ElapsedMsSince(candidateDetectionStart);
            }
        }
        if (options.write_preview_files && should_write_preview(config.preview, layer_index, grid.layer_count)) {
            const auto previewWriteStart = SlicerClock::now();
            const std::vector<std::uint8_t> texture_preview_mask = build_texture_preview_mask(
                config,
                grid,
                model_masks.at(layer_index),
                config.material_role_mapping.enabled ? &material_role_columns : nullptr,
                &column_ranges,
                layer_index);
            Json::Array written = write_layer_previews(
                config.preview,
                package_dir,
                grid,
                layer_index,
                layer,
                config.texture.enabled ? &texture_preview_mask : nullptr);
            preview_files.insert(preview_files.end(), written.begin(), written.end());
            profile.preview_write_ms += ElapsedMsSince(previewWriteStart);
        }
        const auto layerMetadataStart = SlicerClock::now();
        layers.push_back(Json::object({
            {"index", layer_index},
            {"zMm", diagnostics.z_mm},
            {"path", relative_path},
            {"widthPx", grid.width_px},
            {"heightPx", grid.height_px},
            {"modelPixels", layer_model_pixels},
            {"supportPixels", layer_support_pixels},
            {"textureSurfacePixels", diagnostics.semantic.texture_surface_pixels},
            {"modelFillPixels", diagnostics.semantic.model_fill_pixels},
            {"internalVoidSupportPixels", diagnostics.semantic.internal_void_support_pixels},
            {"upperSurfaceSupportPixels", diagnostics.upper_projection_support_pixels},
            {"outerVarnishPixels", diagnostics.semantic.outer_varnish_pixels},
            {"outerSurfaceVarnishPixels", diagnostics.semantic.outer_surface_varnish_pixels},
            {"innerSurfaceVarnishPixels", diagnostics.semantic.inner_surface_varnish_pixels},
            {"semantic", semantic_stats_to_json(diagnostics.semantic)},
        }));
        slice_layers.push_back(layer_diagnostics_to_json(diagnostics));
        contour_layers.push_back(layer_diagnostics_to_json(diagnostics));
        profile.layer_compute_ms += ElapsedMsSince(layerMetadataStart);
        const int completedLayers = layer_index + 1;
        if (ShouldNotifyLayerProgress(completedLayers, grid.layer_count))
        {
            const int percent = 36 + (completedLayers * 56 / std::max(1, grid.layer_count));
            NotifyProgress(
                options,
                run_start,
                "layer_processing",
                completedLayers,
                grid.layer_count,
                percent);
        }
    }
    profile.layer_compose_ms = ElapsedMsSince(phase_start);
    NotifyProgress(options, run_start, "report_build", 0, 1, 92);
    phase_start = SlicerClock::now();

    model_report.three_mf.texture_sampled_pixels = texture_runtime.report.sampled_pixels;

    const Json material_process_report =
        material_process_report_to_json(config, model_report, grid, layer_diagnostics, total_channel_stats);
    const Json cross_section_material_stack_report =
        BuildCrossSectionMaterialStackReport(
            config,
            total_semantic_stats,
            support_generation,
            support_placement_policy);
    Json material_closure_report;
    if (collectMaterialClosureExact
        && materialClosureExactLayers.size() == static_cast<std::size_t>(grid.layer_count))
    {
        material_closure_report = BuildMaterialClosureExactReport(
            config.material_closure,
            materialClosureExactLayers);
    }
    else if (collectMaterialClosureCandidate
             && materialClosureCandidateLayers.size() == static_cast<std::size_t>(grid.layer_count))
    {
        material_closure_report = BuildMaterialClosureCandidateReport(
            config.material_closure,
            materialClosureCandidateLayers);
    }
    else
    {
        material_closure_report = BuildMaterialClosureReportSkeleton(
            config.material_closure,
            grid.layer_count);
    }

    const Json slice_report = Json::object({
        {"slicingMode", config.slicing_mode},
        {"grid",
         Json::object({
             {"widthPx", grid.width_px},
             {"heightPx", grid.height_px},
             {"layerCount", grid.layer_count},
             {"pixelSizeMm", Json::array({grid.pixel_size_x_mm, grid.pixel_size_y_mm})},
             {"layerThicknessMm", config.output.layer_thickness_mm},
         })},
        {"totals",
         Json::object({
             {"modelPixels", total_model_pixels},
             {"supportPixels", total_support_pixels},
             {"rgbNonZeroPixels", total_rgb_non_zero_pixels},
             {"whiteNonZeroPixels", total_white_non_zero_pixels},
             {"supportNonZeroPixels", total_support_non_zero_pixels},
             {"varnishNonZeroPixels", total_varnish_non_zero_pixels},
             {"modelPrintPixels", total_model_pixels},
             {"supportPrintPixels", total_support_non_zero_pixels},
             {"textureSurfacePixels", total_semantic_stats.texture_surface_pixels},
             {"modelFillPixels", total_semantic_stats.model_fill_pixels},
             {"internalVoidSupportPixels", total_semantic_stats.internal_void_support_pixels},
             {"upperSurfaceSupportPixels", support_generation.upper_projection_support_pixels},
             {"outerVarnishSupportOverlapPixelsCleared", cleared_outer_varnish_support_pixels},
             {"outerVarnishPixels", total_semantic_stats.outer_varnish_pixels},
             {"outerSurfaceVarnishPixels", total_semantic_stats.outer_surface_varnish_pixels},
             {"innerSurfaceVarnishPixels", total_semantic_stats.inner_surface_varnish_pixels},
             {"rgbPrintPixels", total_rgb_non_zero_pixels},
             {"whitePrintPixels", total_white_non_zero_pixels},
             {"varnishPrintPixels", total_varnish_non_zero_pixels},
             {"islandCount", support_generation.island_count},
             {"islandPixels", support_generation.island_pixels},
             {"unsupportedPixels", support_generation.unsupported_pixels},
             {"filteredIslandCount", support_generation.filtered_island_count},
             {"filteredIslandPixels", support_generation.filtered_island_pixels},
             {"supportTypeStats",
              Json::object({
                  {"bottom_projection", support_generation.bottom_projection_support_pixels},
                  {"unsupported_island", support_generation.unsupported_island_support_pixels},
                  {"full_vertical_projection", support_generation.full_vertical_projection_support_pixels},
                  {"internal_void", support_generation.internal_void_support_pixels},
                  {"upper_projection", support_generation.upper_projection_support_pixels},
              })},
             {"supportConnectivity", support_connectivity_summary_to_json(layer_diagnostics)},
             {"channelStats", channel_stats_array_to_json(total_channel_stats)},
             {"texture",
              Json::object({
                  {"enabled", texture_runtime.report.enabled},
                  {"sampledPixels", texture_runtime.report.sampled_pixels},
                  {"fallbackPixels", texture_runtime.report.fallback_pixels},
                  {"uvOutOfRangePixels", texture_runtime.report.uv_out_of_range_pixels},
              })},
             {"materialPolicyApplied", config.material_policy.enabled},
             {"materialPolicy",
              Json::object({
                  {"enabled", config.material_policy.enabled},
                  {"rgbPrintPixels", material_policy_report.rgb_print_pixels},
                  {"whitePrintPixels", material_policy_report.white_print_pixels},
                  {"varnishPrintPixels", material_policy_report.varnish_print_pixels},
              })},
             {"semantic", semantic_stats_to_json(total_semantic_stats)},
             {"materialSemantics",
              Json::object({
                  {"modelFill",
                   Json::object({
                       {"enabled", config.model_fill.enabled},
                       {"material", config.model_fill.material},
                       {"scope", config.model_fill.scope},
                       {"value", static_cast<int>(config.model_fill.value)},
                       {"emptyAllowedInProduction", config.model_fill.empty_allowed_in_production},
                       {"legacyRgbFallback", config.model_fill.legacy_rgb_fallback},
                       {"resolvedProfileDefaultMaterial",
                        ModelFillMaterialToString(ResolveProfileDefaultModelFillMaterial(config))},
                   })},
                  {"supportPlacement", config.support.placement},
                  {"supportPlacementPolicy",
                   Json::object({
                       {"requested", support_placement_policy.requested_placement},
                       {"effective", support_placement_policy.effective_placement},
                       {"placementExplicit", support_placement_policy.placement_explicit},
                       {"lowerEnabled", support_placement_policy.lower_enabled},
                       {"upperEnabled", support_placement_policy.upper_enabled},
                       {"unsupportedOnlyEnabled", support_placement_policy.unsupported_only_enabled},
                       {"fullVerticalProjectionEnabled",
                        support_placement_policy.full_vertical_projection_enabled},
                       {"advancedDebug", support_placement_policy.advanced_debug},
                       {"upperBoundarySource", upper_support_boundary_info.source},
                       {"upperBoundaryIncludesOuterVarnishShell",
                        upper_support_boundary_info.includes_outer_varnish_shell},
                       {"outerVarnishSupportOverlapPixelsCleared", cleared_outer_varnish_support_pixels},
                   })},
                  {"internalVoidSupport",
                   Json::object({
                       {"enabled", config.support.internal_void.enabled},
                       {"fillRule", config.support.internal_void.fill_rule},
                       {"minAreaPx", config.support.internal_void.min_area_px},
                       {"printPixels", total_semantic_stats.internal_void_support_pixels},
                   })},
                  {"outerVarnish",
                   Json::object({
                       {"enabled", config.outer_varnish.enabled},
                       {"thicknessMm", config.outer_varnish.thickness_mm},
                       {"thicknessPx", outerVarnishThicknessPx},
                       {"effectiveThicknessMm", outerVarnishEffectiveThicknessMm},
                       {"pixelPitchUm", config.outer_varnish.pixel_pitch_um},
                       {"allowXYExpansion", config.outer_varnish.allow_xy_expansion},
                       {"conflictPolicy", config.outer_varnish.conflict_policy},
                       {"value", static_cast<int>(config.outer_varnish.value)},
                       {"printPixels", total_semantic_stats.outer_varnish_pixels},
                   })},
                  {"surfaceVarnish",
                   Json::object({
                       {"enabled", config.surface_varnish.enabled},
                       {"outerSurface", config.surface_varnish.outer_surface},
                       {"innerSurface", config.surface_varnish.inner_surface},
                       {"thicknessPx", config.surface_varnish.thickness_px},
                       {"source", config.surface_varnish.source},
                       {"value", static_cast<int>(ResolveSurfaceVarnishValue(config))},
                       {"outerSurfacePrintPixels", total_semantic_stats.outer_surface_varnish_pixels},
                       {"innerSurfacePrintPixels", total_semantic_stats.inner_surface_varnish_pixels},
                       {"note", "SurfaceVarnishLayer writes V on model pixels and is distinct from OuterVarnishShell"},
                   })},
                  {"semanticPriority", "Model>OuterVarnishShell>Support>Empty"},
              })},
             {"crossSectionMaterialStack", cross_section_material_stack_report},
             {"materialClosure", BuildMaterialClosureSliceSummary(material_closure_report)},
             {"singleMaterialConsistency", BuildSingleMaterialConsistencyHint(config)},
         })},
        {"layers", Json{slice_layers}},
    });

    const Json repair_report = Json::object({
        {"status", "not_required_p0_lite"},
        {"operations", Json::Array{}},
        {"note", "P0 performs lightweight load validation and bbox extraction only"},
    });

    const Json support_report = Json::object({
        {"enabled", config.support.enabled},
        {"mode", config.support.mode},
        {"supportMode", config.support.mode},
        {"placement", config.support.placement},
        {"placementPolicy",
         Json::object({
             {"requested", support_placement_policy.requested_placement},
             {"effective", support_placement_policy.effective_placement},
             {"placementExplicit", support_placement_policy.placement_explicit},
             {"lowerEnabled", support_placement_policy.lower_enabled},
             {"upperEnabled", support_placement_policy.upper_enabled},
             {"unsupportedOnlyEnabled", support_placement_policy.unsupported_only_enabled},
             {"fullVerticalProjectionEnabled", support_placement_policy.full_vertical_projection_enabled},
             {"advancedDebug", support_placement_policy.advanced_debug},
             {"upperBoundarySource", upper_support_boundary_info.source},
             {"upperBoundaryIncludesOuterVarnishShell",
              upper_support_boundary_info.includes_outer_varnish_shell},
             {"upperOutsideBoundary",
              config.outer_varnish.enabled ? "outer_varnish_shell" : config.support.upper.outside},
         })},
        {"supportReasons",
         Json::object({
             {"bottom_projection", "lower_surface_support"},
             {"unsupported_island", "unsupported_island_projection"},
             {"full_vertical_projection", "advanced_debug_full_vertical_projection"},
             {"internal_void", "internal_void"},
             {"upper_projection", "upper_detachable_surface_support"},
         })},
        {"value", config.support.value},
        {"minOverlapRatio", config.support.min_overlap_ratio},
        {"minIslandAreaPx", config.support.min_island_area_px},
        {"connectivity", config.support.connectivity},
        {"unsupportedProjection", config.support.unsupported_projection},
        {"xyDilationPx", config.support.xy_dilation_px},
        {"internalVoid",
         Json::object({
             {"enabled", config.support.internal_void.enabled},
             {"fillRule", config.support.internal_void.fill_rule},
             {"minAreaPx", config.support.internal_void.min_area_px},
             {"reason", "internal_void"},
             {"printPixels", support_generation.internal_void_support_pixels},
         })},
        {"shape",
         Json::object({
             {"enabled", support_shape_policy.enabled},
             {"minComponentAreaPx", support_shape_policy.min_component_area_px},
             {"xyDilationPx", support_shape_policy.xy_dilation_px},
             {"closingRadiusPx", support_shape_policy.closing_radius_px},
             {"bridgeGapPx", support_shape_policy.bridge_gap_px},
             {"preserveModelPriority", support_shape_policy.preserve_model_priority},
             {"maxAddedSupportRatio", support_shape_policy.max_added_support_ratio},
             {"addedSupportPixels", support_shape_result.added_support_pixels},
             {"removedSupportPixels", support_shape_result.removed_support_pixels},
         })},
        {"slicingMode", config.slicing_mode},
        {"supportSource",
         config.slicing_mode == "relief_heightfield" ? "relief_lower_surface" : "first_model_layer"},
        {"modelPriority", "Model > Support"},
        {"supportPixels", total_support_pixels},
        {"supportPrintPixels", total_support_non_zero_pixels},
        {"upperSurfaceSupportPixels", support_generation.upper_projection_support_pixels},
        {"outerVarnishSupportOverlapPixelsCleared", cleared_outer_varnish_support_pixels},
        {"columnsWithSupport", columns_with_support},
        {"islandCount", support_generation.island_count},
        {"islandPixels", support_generation.island_pixels},
        {"unsupportedPixels", support_generation.unsupported_pixels},
        {"filteredIslandCount", support_generation.filtered_island_count},
        {"filteredIslandPixels", support_generation.filtered_island_pixels},
        {"layersWithIslands", support_generation.layers_with_islands},
        {"layersWithSupport", support_generation.layers_with_support},
        {"supportConnectivity", support_connectivity_summary_to_json(layer_diagnostics)},
        {"totals",
         Json::object({
             {"supportPixels", total_support_pixels},
             {"supportPrintPixels", total_support_non_zero_pixels},
             {"islandCount", support_generation.island_count},
             {"islandPixels", support_generation.island_pixels},
             {"unsupportedPixels", support_generation.unsupported_pixels},
             {"filteredIslandCount", support_generation.filtered_island_count},
             {"filteredIslandPixels", support_generation.filtered_island_pixels},
             {"upperSurfaceSupportPixels", support_generation.upper_projection_support_pixels},
             {"upperProjectionPixels", support_generation.upper_projection_support_pixels},
         })},
        {"supportTypeStats",
         Json::object({
             {"bottom_projection", support_generation.bottom_projection_support_pixels},
             {"unsupported_island", support_generation.unsupported_island_support_pixels},
             {"full_vertical_projection", support_generation.full_vertical_projection_support_pixels},
             {"internal_void", support_generation.internal_void_support_pixels},
             {"upper_projection", support_generation.upper_projection_support_pixels},
         })},
        {"layers", Json{contour_layers}},
    });
    const Json support_shape_report = MakeSupportShapeReport(support_shape_policy, support_shape_result);

    Json::Array preview_channels;
    for (const std::string& channel : config.preview.channels) {
        preview_channels.push_back(canonical_preview_channel(channel));
    }
    const auto color_json = [](const std::array<std::uint8_t, 3>& color) {
        return Json::array({static_cast<int>(color.at(0)), static_cast<int>(color.at(1)), static_cast<int>(color.at(2))});
    };

    const Json preview_report = Json::object({
        {"schema", "p0.preview_report.1"},
        {"enabled", config.preview.enabled},
        {"format", config.preview.format},
        {"interval", config.preview.interval},
        {"channels", Json{preview_channels}},
        {"pseudoColors",
         Json::object({
             {"empty", color_json(config.preview.empty_color)},
             {"support", color_json(config.preview.support_color)},
             {"white", color_json(config.preview.white_color)},
             {"varnish", color_json(config.preview.varnish_color)},
         })},
        {"layerRange",
         config.preview.has_layer_range ? Json::array({config.preview.layer_range.at(0), config.preview.layer_range.at(1)})
                                        : Json{}},
        {"onlyNonEmptyLayers", config.preview.only_non_empty_layers},
        {"files", Json{preview_files}},
        {"generated", Json{preview_files}},
    });

    Json::Array material_libraries;
    for (const std::string& library : model_report.material_libraries) {
        material_libraries.push_back(library);
    }
    Json::Array materials;
    for (const MaterialStat& material : model_report.materials) {
        materials.push_back(Json::object({
            {"name", material.name},
            {"faceCount", static_cast<std::uint64_t>(material.face_count)},
            {"triangleCount", static_cast<std::uint64_t>(material.triangle_count)},
        }));
    }
    Json::Array material_infos;
    for (const MaterialInfo& material : model_report.material_infos) {
        material_infos.push_back(Json::object({
            {"name", material.name},
            {"hasDiffuse", material.has_diffuse},
            {"diffuseRgb", rgb_to_json(material.diffuse_rgb)},
            {"hasTexture", material.has_texture},
            {"texturePath", material.diffuse_texture_path.generic_string()},
            {"textureExists", material.texture_exists},
        }));
    }

    const Json model_json = Json::object({
        {"modelPath", model_report.model_path.generic_string()},
        {"format", model_report.format},
        {"stlEncoding", model_report.stl_encoding},
        {"vertexCount", static_cast<std::uint64_t>(model_report.vertex_count)},
        {"faceCount", static_cast<std::uint64_t>(model_report.face_count)},
        {"triangleCount", static_cast<std::uint64_t>(model_report.triangle_count)},
        {"degenerateTriangleCount", static_cast<std::uint64_t>(model_report.degenerate_triangle_count)},
        {"materialCount", static_cast<std::uint64_t>(model_report.materials.size())},
        {"texcoordCount", static_cast<std::uint64_t>(model_report.texcoord_count)},
        {"facesWithUv", static_cast<std::uint64_t>(model_report.faces_with_uv)},
        {"facesWithoutUv", static_cast<std::uint64_t>(model_report.faces_without_uv)},
        {"materialLibraries", Json{material_libraries}},
        {"materials", Json{materials}},
        {"materialInfos", Json{material_infos}},
        {"autoOrient",
         Json::object({
             {"enabled", model_report.auto_orient.enabled},
             {"applied", model_report.auto_orient.applied},
             {"maxHeightMm", model_report.auto_orient.max_height_mm},
             {"selectedOrientation", model_report.auto_orient.selected_orientation},
             {"originalBboxMm", bbox_to_json(model_report.auto_orient.original_bbox_mm)},
         })},
        {"bboxMm", bbox_to_json(model_report.bbox_mm)},
    });
    const Json package_report = MakeReportBase(
        "p0.report.package.1",
        Json::object({
            {"component", "slicer_core"},
            {"packageDir", package_dir.generic_string()},
        }),
        Json::object({
            {"configPath", config_path.generic_string()},
            {"modelPath", model_report.model_path.generic_string()},
            {"schema", "p0.rgbwsv.2"},
        }));

    Json::Object tiff_json{
        {"channelOrder", channel_order_json()},
        {"channelCount", rgbwsv_channel_count},
        {"bitDepth", 8},
        {"sampleFormat", "uint"},
        {"planarConfig", "contiguous"},
        {"tiled", config.output.storage_mode == "tiled"},
        {"storage", config.output.storage_mode},
        {"storageMode", config.output.storage_mode},
        {"polarity", "black_is_print"},
        {"printValue", 0},
        {"emptyValue", 255},
        {"writeTiffLayers", options.write_tiff_layers},
        {"layers", Json{layers}},
    };
    if (config.output.storage_mode == "tiled") {
        tiff_json["tileSize"] = Json::array({config.output.tile_size.at(0), config.output.tile_size.at(1)});
    } else {
        tiff_json["rowsPerStrip"] = config.output.rows_per_strip;
    }

    const Json manifest = Json::object({
        {"schema", "p0.rgbwsv.2"},
        {"schemaVersion", "p0.rgbwsv.2"},
        {"source",
         Json::object({
             {"configPath", config_path.generic_string()},
             {"modelPath", model_report.model_path.generic_string()},
             {"format", model_report.format},
         })},
        {"grid",
         Json::object({
             {"widthPx", grid.width_px},
             {"heightPx", grid.height_px},
             {"layerCount", grid.layer_count},
             {"dpiX", config.output.dpi_x},
             {"dpiY", config.output.dpi_y},
             {"dpi", Json::array({config.output.dpi_x, config.output.dpi_y})},
             {"pixelSizeXmm", grid.pixel_size_x_mm},
             {"pixelSizeYmm", grid.pixel_size_y_mm},
             {"pixelSizeMm", Json::array({grid.pixel_size_x_mm, grid.pixel_size_y_mm})},
             {"layerThicknessMm", config.output.layer_thickness_mm},
             {"originMm", Json::array({grid.origin_x_mm, grid.origin_y_mm, 0.0})},
         })},
        {"slicing",
         Json::object({
             {"mode", config.slicing_mode},
             {"reliefFillMode", config.relief.fill_mode},
         })},
        {"tiff", Json{tiff_json}},
        {"layers", Json{layers}},
        {"reports",
         Json::object({
             {"package", "reports/package_report.json"},
             {"model", "reports/model_report.json"},
             {"slice", "reports/slice_report.json"},
             {"repair", "reports/repair_report.json"},
             {"support", "reports/support_report.json"},
             {"supportShape", "reports/support_shape_report.json"},
             {"preview", "reports/preview_report.json"},
             {"texture", "reports/texture_report.json"},
             {"materialPolicy", "reports/material_policy_report.json"},
             {"materialProcess", "reports/material_process_report.json"},
             {"crossSectionMaterialStack", "reports/cross_section_material_stack_report.json"},
             {"materialClosure", "reports/material_closure_report.json"},
             {"materialRoleMapping", "reports/material_role_mapping_report.json"},
             {"objMtlMaterial", "reports/obj_mtl_material_report.json"},
             {"threeMf", "reports/three_mf_report.json"},
             {"contour", "reports/contour_report.json"},
             {"relief", "reports/relief_report.json"},
         })},
        {"preview", Json::object({{"format", config.preview.format}, {"files", Json{preview_files}}})},
    });
    profile.report_build_ms = ElapsedMsSince(phase_start);
    NotifyProgress(options, run_start, "report_write", 0, 1, 95);
    phase_start = SlicerClock::now();

    if (options.write_reports) {
        write_json_file(package_dir / "reports/model_report.json", model_json);
        write_json_file(package_dir / "reports/package_report.json", package_report);
        write_json_file(package_dir / "reports/slice_report.json", slice_report);
        write_json_file(package_dir / "reports/repair_report.json", repair_report);
        write_json_file(package_dir / "reports/support_report.json", support_report);
        write_json_file(package_dir / "reports/support_shape_report.json", support_shape_report);
        write_json_file(package_dir / "reports/preview_report.json", preview_report);
        write_json_file(package_dir / "reports/texture_report.json", texture_report_to_json(texture_runtime.report));
        write_json_file(
            package_dir / "reports/material_policy_report.json",
            material_policy_report_to_json(config, material_policy_report));
        write_json_file(package_dir / "reports/material_process_report.json", material_process_report);
        write_json_file(package_dir / "reports/cross_section_material_stack_report.json", cross_section_material_stack_report);
        write_json_file(package_dir / "reports/material_closure_report.json", material_closure_report);
        write_json_file(
            package_dir / "reports/material_role_mapping_report.json",
            material_role_mapping_report_to_json(material_role_mapping_report));
        write_json_file(
            package_dir / "reports/obj_mtl_material_report.json",
            obj_mtl_material_report_to_json(model_report));
        write_json_file(
            package_dir / "reports/three_mf_report.json",
            three_mf_report_to_json(model_report));
        write_json_file(package_dir / "reports/contour_report.json", Json::object({{"layers", Json{contour_layers}}}));
        write_json_file(
            package_dir / "reports/relief_report.json",
            relief_report_to_json(config, relief_report, total_support_pixels, columns_with_support));
        write_json_file(package_dir / "manifest.json", manifest);
    }
    profile.report_write_ms = ElapsedMsSince(phase_start);
    profile.slice_processing_ms =
        profile.grid_setup_ms
        + profile.mask_sampling_ms
        + profile.texture_prepare_ms
        + profile.support_generation_ms
        + profile.layer_compute_ms;
    profile.output_write_ms =
        profile.tiff_write_ms
        + profile.preview_write_ms
        + profile.report_write_ms
        + profile.package_publish_ms;
    profile.total_ms = ElapsedMsSince(run_start);
    NotifyProgress(options, run_start, "completed", 1, 1, 100);

    return {
        package_dir,
        grid.width_px,
        grid.height_px,
        grid.layer_count,
        total_model_pixels,
        total_support_pixels,
        profile};
}

}  // namespace slicer_core
