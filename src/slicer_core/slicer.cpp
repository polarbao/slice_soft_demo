#include "slicer_core/slicer.h"

#include "slicer_core/json_value.h"
#include "slicer_core/model.h"
#include "slicer_core/tiff_io.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

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

std::string layer_file_name(const int layer_index) {
    std::ostringstream stream;
    stream << "layers/layer_" << std::setw(6) << std::setfill('0') << layer_index << ".tiff";
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

bool is_model_layer(const double z_mm, const BoundingBox& bbox) {
    return z_mm >= bbox.min.z && z_mm < bbox.max.z;
}

bool is_support_layer(const double z_mm, const SliceConfig& config, const BoundingBox& bbox) {
    return config.support.enabled && z_mm < bbox.min.z;
}

std::vector<std::uint16_t> compose_layer(
    const SliceConfig& config,
    const GridSpec& grid,
    const BoundingBox& bbox,
    const int layer_index,
    int& model_pixels,
    int& support_pixels) {
    std::vector<std::uint16_t> pixels(
        static_cast<std::size_t>(grid.width_px) * grid.height_px * rgbwsv_channel_count,
        0);

    const double z_mm{static_cast<double>(layer_index) * config.output.layer_thickness_mm};
    const bool model_layer{is_model_layer(z_mm, bbox)};
    const bool support_layer{is_support_layer(z_mm, config, bbox)};
    if (!model_layer && !support_layer) {
        return pixels;
    }

    for (int y{0}; y < grid.height_px; ++y) {
        for (int x{0}; x < grid.width_px; ++x) {
            const std::size_t base =
                (static_cast<std::size_t>(y) * grid.width_px + x) * rgbwsv_channel_count;
            if (model_layer) {
                pixels.at(base + 0U) = config.material.rgb.at(0);
                pixels.at(base + 1U) = config.material.rgb.at(1);
                pixels.at(base + 2U) = config.material.rgb.at(2);
                pixels.at(base + 3U) = config.material.white_strength;
                pixels.at(base + 4U) = 0;
                pixels.at(base + 5U) = config.material.varnish_strength;
                ++model_pixels;
            } else if (support_layer) {
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
    const SliceConfig config = load_slice_config(config_path);
    const std::filesystem::path config_dir =
        config_path.parent_path().empty() ? std::filesystem::current_path() : config_path.parent_path();
    const ModelReport model_report = load_model_report(config, config_dir);
    const GridSpec grid = make_grid_spec(config, model_report.bbox_mm);

    const std::filesystem::path package_dir = config.output.package_dir;
    std::filesystem::create_directories(package_dir / "layers");
    std::filesystem::create_directories(package_dir / "reports");

    TiffImageSpec tiff_spec;
    tiff_spec.width = static_cast<std::uint32_t>(grid.width_px);
    tiff_spec.height = static_cast<std::uint32_t>(grid.height_px);
    tiff_spec.tile_width = static_cast<std::uint32_t>(config.output.tile_size.at(0));
    tiff_spec.tile_height = static_cast<std::uint32_t>(config.output.tile_size.at(1));

    int total_model_pixels{0};
    int total_support_pixels{0};
    Json::Array layers;
    for (int layer_index{0}; layer_index < grid.layer_count; ++layer_index) {
        int layer_model_pixels{0};
        int layer_support_pixels{0};
        const std::vector<std::uint16_t> layer = compose_layer(
            config,
            grid,
            model_report.bbox_mm,
            layer_index,
            layer_model_pixels,
            layer_support_pixels);
        total_model_pixels += layer_model_pixels;
        total_support_pixels += layer_support_pixels;
        const std::string relative_path = layer_file_name(layer_index);
        write_rgbwsv_tiled_tiff(package_dir / relative_path, tiff_spec, layer);
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

    const Json model_json = Json::object({
        {"modelPath", model_report.model_path.generic_string()},
        {"format", model_report.format},
        {"vertexCount", static_cast<std::uint64_t>(model_report.vertex_count)},
        {"triangleCount", static_cast<std::uint64_t>(model_report.triangle_count)},
        {"bboxMm", bbox_to_json(model_report.bbox_mm)},
    });

    write_json_file(package_dir / "reports/model_report.json", model_json);
    write_json_file(package_dir / "reports/slice_report.json", slice_report);
    write_json_file(package_dir / "reports/repair_report.json", repair_report);
    write_json_file(package_dir / "reports/support_report.json", support_report);

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
             {"tileSize", Json::array({config.output.tile_size.at(0), config.output.tile_size.at(1)})},
             {"layers", Json{layers}},
         })},
        {"reports",
         Json::object({
             {"model", "reports/model_report.json"},
             {"slice", "reports/slice_report.json"},
             {"repair", "reports/repair_report.json"},
             {"support", "reports/support_report.json"},
         })},
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
