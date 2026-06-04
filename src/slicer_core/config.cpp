#include "slicer_core/config.h"

#include "slicer_core/json_value.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace slicer_core {
namespace {

constexpr std::array<const char*, 6> expected_channel_order{"R", "G", "B", "W", "S", "V"};

std::uint16_t read_u16(const Json& object, const char* key, const std::uint16_t fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    const int value{object.at(key).as_int()};
    if (value < 0 || value > 65535) {
        throw std::runtime_error(std::string{"uint16 config field out of range: "} + key);
    }
    return static_cast<std::uint16_t>(value);
}

std::array<double, 3> read_double3(const Json& object, const char* key, const std::array<double, 3>& fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    const auto& value = object.at(key);
    if (!value.is_array() || value.size() != 3) {
        throw std::runtime_error(std::string{"expected 3-number array for field: "} + key);
    }
    return {value.at(0).as_double(), value.at(1).as_double(), value.at(2).as_double()};
}

std::array<int, 2> read_int2(const Json& object, const char* key, const std::array<int, 2>& fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    const auto& value = object.at(key);
    if (!value.is_array() || value.size() != 2) {
        throw std::runtime_error(std::string{"expected 2-integer array for field: "} + key);
    }
    return {value.at(0).as_int(), value.at(1).as_int()};
}

std::vector<std::string> read_string_array(
    const Json& object,
    const char* key,
    const std::vector<std::string>& fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    const auto& value = object.at(key);
    if (!value.is_array()) {
        throw std::runtime_error(std::string{"expected string array for field: "} + key);
    }
    std::vector<std::string> result;
    result.reserve(value.size());
    for (const auto& item : value.as_array()) {
        result.push_back(item.as_string());
    }
    return result;
}

std::array<std::uint16_t, 3> read_rgb(const Json& object, const std::array<std::uint16_t, 3>& fallback) {
    if (!object.contains("rgb")) {
        return fallback;
    }
    const auto& value = object.at("rgb");
    if (!value.is_array() || value.size() != 3) {
        throw std::runtime_error("expected 3-value uint16 array for modelMaterial.rgb");
    }
    std::array<std::uint16_t, 3> result{};
    for (std::size_t i{0}; i < result.size(); ++i) {
        const int channel{value.at(i).as_int()};
        if (channel < 0 || channel > 65535) {
            throw std::runtime_error("modelMaterial.rgb value out of uint16 range");
        }
        result.at(i) = static_cast<std::uint16_t>(channel);
    }
    return result;
}

}  // namespace

SliceConfig load_slice_config(const std::filesystem::path& config_path) {
    std::ifstream input{config_path};
    if (!input) {
        throw std::runtime_error("failed to open config: " + config_path.string());
    }

    const Json root = Json::parse(input);
    SliceConfig config;

    if (!root.contains("input") || !root.at("input").contains("modelPath")) {
        throw std::runtime_error("missing required field: input.modelPath");
    }
    const auto& input_json = root.at("input");
    config.input.model_path = input_json.at("modelPath").as_string();
    config.input.format = input_json.value("format", config.input.format);

    if (root.contains("output")) {
        const auto& output = root.at("output");
        config.output.package_dir = output.value("packageDir", config.output.package_dir.string());
        config.output.dpi_x = output.value("dpiX", config.output.dpi_x);
        config.output.dpi_y = output.value("dpiY", config.output.dpi_y);
        config.output.layer_thickness_mm = output.value("layerThicknessMm", config.output.layer_thickness_mm);
        config.output.channel_order = read_string_array(output, "channelOrder", config.output.channel_order);
        config.output.bit_depth = output.value("bitDepth", config.output.bit_depth);
        config.output.planar_config = output.value("planarConfig", config.output.planar_config);
        config.output.tiled = output.value("tiled", config.output.tiled);
        config.output.tile_size = read_int2(output, "tileSize", config.output.tile_size);
    }

    if (root.contains("modelTransform")) {
        const auto& transform = root.at("modelTransform");
        config.transform.unit = transform.value("unit", config.transform.unit);
        config.transform.scale = read_double3(transform, "scale", config.transform.scale);
        config.transform.rotation_deg = read_double3(transform, "rotationDeg", config.transform.rotation_deg);
        config.transform.translation_mm = read_double3(transform, "translationMm", config.transform.translation_mm);
    }

    if (root.contains("modelMaterial")) {
        const auto& material = root.at("modelMaterial");
        config.material.rgb = read_rgb(material, config.material.rgb);
        config.material.white_strength = read_u16(material, "whiteStrength", config.material.white_strength);
        config.material.varnish_strength = read_u16(material, "varnishStrength", config.material.varnish_strength);
    }

    if (root.contains("support")) {
        const auto& support = root.at("support");
        config.support.enabled = support.value("enabled", config.support.enabled);
        config.support.mode = support.value("mode", config.support.mode);
        config.support.strength = read_u16(support, "strength", config.support.strength);
        config.support.offset_mm = support.value("offsetMm", config.support.offset_mm);
        config.support.min_area_px = support.value("minAreaPx", config.support.min_area_px);
    }

    if (root.contains("preview")) {
        const auto& preview = root.at("preview");
        config.preview.enabled = preview.value("enabled", config.preview.enabled);
        config.preview.interval = preview.value("interval", config.preview.interval);
        config.preview.channels = read_string_array(preview, "channels", config.preview.channels);
    }

    validate_slice_config(config);
    return config;
}

void validate_slice_config(const SliceConfig& config) {
    if (config.input.model_path.empty()) {
        throw std::runtime_error("input.modelPath must not be empty");
    }
    if (config.output.dpi_x != 600 || config.output.dpi_y != 600) {
        throw std::runtime_error("P0 requires dpiX == dpiY == 600");
    }
    if (config.output.layer_thickness_mm <= 0.0) {
        throw std::runtime_error("output.layerThicknessMm must be positive");
    }
    if (config.output.bit_depth != 16) {
        throw std::runtime_error("P0 requires output.bitDepth == 16");
    }
    if (config.output.planar_config != "contiguous") {
        throw std::runtime_error("P0 requires output.planarConfig == contiguous");
    }
    if (!config.output.tiled) {
        throw std::runtime_error("P0 requires tiled TIFF output");
    }
    if (config.output.tile_size.at(0) <= 0 || config.output.tile_size.at(1) <= 0) {
        throw std::runtime_error("output.tileSize values must be positive");
    }
    if (config.support.enabled && config.support.mode != "bottom_projection") {
        throw std::runtime_error("P0 only supports support.mode == bottom_projection");
    }
    if (config.output.channel_order.size() != expected_channel_order.size()) {
        throw std::runtime_error("P0 channelOrder must contain exactly six channels");
    }
    for (std::size_t i{0}; i < expected_channel_order.size(); ++i) {
        if (config.output.channel_order.at(i) != expected_channel_order.at(i)) {
            throw std::runtime_error("P0 channelOrder must be exactly R G B W S V");
        }
    }
}

}  // namespace slicer_core
