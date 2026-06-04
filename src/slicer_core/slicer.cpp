#include "slicer_core/slicer.h"

#include "slicer_core/json_value.h"
#include "slicer_core/model.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace slicer_core {
namespace {

constexpr double mm_per_inch{25.4};

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

std::string layer_file_name(const int layer_index) {
    std::ostringstream stream;
    stream << "layers/layer_" << std::setw(6) << std::setfill('0') << layer_index << ".tiff";
    return stream.str();
}

std::string preview_file_name(const std::string& prefix, const int layer_index) {
    std::ostringstream stream;
    stream << "preview/" << prefix << "_" << std::setw(6) << std::setfill('0') << layer_index << ".ppm";
    return stream.str();
}

GridSpec make_grid_spec(const SliceConfig& config, const BoundingBox& bbox) {
    GridSpec grid;
    grid.pixel_size_x_mm = mm_per_inch / static_cast<double>(config.output.dpi_x);
    grid.pixel_size_y_mm = mm_per_inch / static_cast<double>(config.output.dpi_y);
    grid.origin_x_mm = bbox.min.x;
    grid.origin_y_mm = bbox.min.y;
    const double width_mm{std::max(0.001, bbox.max.x - bbox.min.x)};
    const double height_mm{std::max(0.001, bbox.max.y - bbox.min.y)};
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

std::uint8_t to_u8(const std::uint16_t value) {
    return static_cast<std::uint8_t>(value / 257U);
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

Json::Array write_layer_previews(
    const std::filesystem::path& package_dir,
    const GridSpec& grid,
    const int layer_index,
    const std::vector<std::uint16_t>& layer) {
    std::vector<std::array<std::uint8_t, 3>> model_preview(static_cast<std::size_t>(grid.width_px) * grid.height_px);
    std::vector<std::array<std::uint8_t, 3>> support_preview(model_preview.size());
    std::vector<std::array<std::uint8_t, 3>> varnish_preview(model_preview.size());

    for (std::size_t i{0}; i < model_preview.size(); ++i) {
        const std::size_t base{i * rgbwsv_channel_count};
        model_preview.at(i) = {to_u8(layer.at(base + 0U)), to_u8(layer.at(base + 1U)), to_u8(layer.at(base + 2U))};
        support_preview.at(i) = {0, to_u8(layer.at(base + 4U)), 0};
        const std::uint8_t varnish{to_u8(layer.at(base + 5U))};
        varnish_preview.at(i) = {varnish, 0, varnish};
    }

    const std::string model_path = preview_file_name("model_rgb", layer_index);
    const std::string support_path = preview_file_name("support_s", layer_index);
    const std::string varnish_path = preview_file_name("varnish_v", layer_index);
    write_ppm(package_dir / model_path, grid, model_preview);
    write_ppm(package_dir / support_path, grid, support_preview);
    write_ppm(package_dir / varnish_path, grid, varnish_preview);

    return Json::Array{
        Json::object({{"type", "model_rgb"}, {"path", model_path}}),
        Json::object({{"type", "support_s"}, {"path", support_path}}),
        Json::object({{"type", "varnish_v"}, {"path", varnish_path}}),
    };
}

bool should_write_preview(const PreviewConfig& preview, const int layer_index, const int layer_count) {
    if (!preview.enabled) {
        return false;
    }
    const int interval{std::max(1, preview.interval)};
    return layer_index == 0 || layer_index + 1 == layer_count || (layer_index % interval) == 0;
}

std::size_t mask_index(const GridSpec& grid, const int x, const int y) {
    return static_cast<std::size_t>(y) * grid.width_px + x;
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

std::vector<std::uint8_t> rasterize_segments(const GridSpec& grid, const std::vector<Segment2>& segments) {
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(grid.width_px) * grid.height_px, 0);
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
        for (std::size_t i{0}; i + 1U < intersections.size(); i += 2U) {
            fill_span(mask, grid, intersections.at(i), intersections.at(i + 1U), y);
        }
    }
    return mask;
}

std::vector<std::vector<std::uint8_t>> sample_model_masks(
    const ModelReport& model_report,
    const GridSpec& grid,
    const double layer_thickness_mm) {
    std::vector<std::vector<std::uint8_t>> masks;
    masks.reserve(grid.layer_count);
    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index) {
        const double z_mm{(static_cast<double>(layer_index) + 0.5) * layer_thickness_mm};
        if (z_mm < model_report.bbox_mm.min.z || z_mm > model_report.bbox_mm.max.z) {
            masks.emplace_back(static_cast<std::size_t>(grid.width_px) * grid.height_px, 0);
            continue;
        }
        masks.push_back(rasterize_segments(grid, slice_triangles_to_segments(model_report.triangles, z_mm)));
    }
    return masks;
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

std::vector<std::uint16_t> compose_layer(
    const SliceConfig& config,
    const GridSpec& grid,
    const int layer_index,
    const std::vector<std::uint8_t>& model_mask,
    const std::vector<int>& first_model_layers,
    int& model_pixels,
    int& support_pixels) {
    std::vector<std::uint16_t> pixels(
        static_cast<std::size_t>(grid.width_px) * grid.height_px * rgbwsv_channel_count,
        0);

    for (int y{0}; y < grid.height_px; ++y) {
        for (int x{0}; x < grid.width_px; ++x) {
            const std::size_t pixel_index = mask_index(grid, x, y);
            const std::size_t base =
                (static_cast<std::size_t>(y) * grid.width_px + x) * rgbwsv_channel_count;
            if (model_mask.at(pixel_index) != 0) {
                pixels.at(base + 0U) = config.material.rgb.at(0);
                pixels.at(base + 1U) = config.material.rgb.at(1);
                pixels.at(base + 2U) = config.material.rgb.at(2);
                pixels.at(base + 3U) = config.material.white_strength;
                pixels.at(base + 4U) = 0;
                pixels.at(base + 5U) = config.material.varnish_strength;
                ++model_pixels;
            } else if (config.support.enabled && first_model_layers.at(pixel_index) > layer_index) {
                pixels.at(base + 4U) = config.support.strength;
                ++support_pixels;
            }
        }
    }

    return pixels;
}

Json channel_order_json() {
    return Json::array({"R", "G", "B", "W", "S", "V"});
}

}  // namespace

SliceRunResult run_slicer(const std::filesystem::path& config_path) {
    return run_slicer(config_path, SliceRunOptions{});
}

SliceRunResult run_slicer(const std::filesystem::path& config_path, const SliceRunOptions& options) {
    const SliceConfig config = load_slice_config(config_path);
    const std::filesystem::path config_dir =
        config_path.parent_path().empty() ? std::filesystem::current_path() : config_path.parent_path();
    const ModelReport model_report = load_model_report(config, config_dir);
    const GridSpec grid = make_grid_spec(config, model_report.bbox_mm);

    const std::filesystem::path package_dir = config.output.package_dir;
    std::filesystem::create_directories(package_dir / "layers");
    std::filesystem::create_directories(package_dir / "reports");
    if (config.preview.enabled) {
        std::filesystem::create_directories(package_dir / "preview");
    }

    TiffImageSpec tiff_spec;
    tiff_spec.width = static_cast<std::uint32_t>(grid.width_px);
    tiff_spec.height = static_cast<std::uint32_t>(grid.height_px);
    tiff_spec.tile_width = static_cast<std::uint32_t>(config.output.tile_size.at(0));
    tiff_spec.tile_height = static_cast<std::uint32_t>(config.output.tile_size.at(1));

    const std::vector<std::vector<std::uint8_t>> model_masks =
        sample_model_masks(model_report, grid, config.output.layer_thickness_mm);
    const std::vector<int> first_model_layers = compute_first_model_layers(model_masks, grid);

    int total_model_pixels{0};
    int total_support_pixels{0};
    Json::Array layers;
    Json::Array preview_files;
    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index) {
        int layer_model_pixels{0};
        int layer_support_pixels{0};
        const std::vector<std::uint16_t> layer = compose_layer(
            config,
            grid,
            layer_index,
            model_masks.at(layer_index),
            first_model_layers,
            layer_model_pixels,
            layer_support_pixels);
        total_model_pixels += layer_model_pixels;
        total_support_pixels += layer_support_pixels;
        const std::string relative_path = layer_file_name(layer_index);
        if (options.write_tiff_layers) {
            write_rgbwsv_tiled_tiff(package_dir / relative_path, tiff_spec, layer);
        }
        if (should_write_preview(config.preview, layer_index, grid.layer_count)) {
            Json::Array written = write_layer_previews(package_dir, grid, layer_index, layer);
            preview_files.insert(preview_files.end(), written.begin(), written.end());
        }
        layers.push_back(Json::object({
            {"index", layer_index},
            {"zMm", static_cast<double>(layer_index) * config.output.layer_thickness_mm},
            {"path", relative_path},
            {"modelPixels", layer_model_pixels},
            {"supportPixels", layer_support_pixels},
        }));
    }

    const Json slice_report = Json::object({
        {"grid",
         Json::object({
             {"widthPx", grid.width_px},
             {"heightPx", grid.height_px},
             {"layerCount", grid.layer_count},
             {"pixelSizeMm", Json::array({grid.pixel_size_x_mm, grid.pixel_size_y_mm})},
             {"layerThicknessMm", config.output.layer_thickness_mm},
         })},
        {"totals", Json::object({{"modelPixels", total_model_pixels}, {"supportPixels", total_support_pixels}})},
    });

    const Json repair_report = Json::object({
        {"status", "not_required_p0_lite"},
        {"operations", Json::Array{}},
        {"note", "P0 performs lightweight load validation and bbox extraction only"},
    });

    const Json support_report = Json::object({
        {"enabled", config.support.enabled},
        {"mode", config.support.mode},
        {"strength", config.support.strength},
        {"modelPriority", "Model > Support"},
        {"supportPixels", total_support_pixels},
    });

    const Json preview_report = Json::object({
        {"enabled", config.preview.enabled},
        {"format", "PPM P6"},
        {"interval", config.preview.interval},
        {"files", Json{preview_files}},
    });

    const Json model_json = Json::object({
        {"modelPath", model_report.model_path.generic_string()},
        {"format", model_report.format},
        {"vertexCount", static_cast<std::uint64_t>(model_report.vertex_count)},
        {"triangleCount", static_cast<std::uint64_t>(model_report.triangle_count)},
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

    write_json_file(package_dir / "reports/model_report.json", model_json);
    write_json_file(package_dir / "reports/slice_report.json", slice_report);
    write_json_file(package_dir / "reports/repair_report.json", repair_report);
    write_json_file(package_dir / "reports/support_report.json", support_report);
    write_json_file(package_dir / "reports/preview_report.json", preview_report);

    const Json manifest = Json::object({
        {"schemaVersion", "p0.rgbwsv.1"},
        {"source",
         Json::object({
             {"configPath", config_path.generic_string()},
             {"modelPath", model_report.model_path.generic_string()},
         })},
        {"grid",
         Json::object({
             {"widthPx", grid.width_px},
             {"heightPx", grid.height_px},
             {"layerCount", grid.layer_count},
             {"dpi", Json::array({config.output.dpi_x, config.output.dpi_y})},
             {"pixelSizeMm", Json::array({grid.pixel_size_x_mm, grid.pixel_size_y_mm})},
             {"layerThicknessMm", config.output.layer_thickness_mm},
             {"originMm", Json::array({grid.origin_x_mm, grid.origin_y_mm, 0.0})},
         })},
        {"tiff",
         Json::object({
             {"channelOrder", channel_order_json()},
             {"channelCount", rgbwsv_channel_count},
             {"bitDepth", 16},
             {"sampleFormat", "uint"},
             {"planarConfig", "contiguous"},
             {"storage", "tiled"},
             {"writeTiffLayers", options.write_tiff_layers},
             {"tileSize", Json::array({config.output.tile_size.at(0), config.output.tile_size.at(1)})},
             {"layers", Json{layers}},
         })},
        {"reports",
         Json::object({
             {"model", "reports/model_report.json"},
             {"slice", "reports/slice_report.json"},
             {"repair", "reports/repair_report.json"},
             {"support", "reports/support_report.json"},
             {"preview", "reports/preview_report.json"},
         })},
        {"preview", Json::object({{"format", "PPM P6"}, {"files", Json{preview_files}}})},
    });
    write_json_file(package_dir / "manifest.json", manifest);

    return {
        package_dir,
        grid.width_px,
        grid.height_px,
        grid.layer_count,
        total_model_pixels,
        total_support_pixels};
}

}  // namespace slicer_core
