#include "slicer_core/config.h"

#include "slicer_core/json_value.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace slicer_core {
namespace {

constexpr std::array<const char*, 6> expected_channel_order{"R", "G", "B", "W", "S", "V"};

std::uint8_t read_u8(const Json& object, const char* key, const std::uint8_t fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    const int value{object.at(key).as_int()};
    if (value < 0 || value > 255) {
        throw std::runtime_error(std::string{"uint8 config field out of range: "} + key);
    }
    return static_cast<std::uint8_t>(value);
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

bool has_int2(const Json& object, const char* key) {
    if (!object.contains(key)) {
        return false;
    }
    const auto& value = object.at(key);
    if (!value.is_array() || value.size() != 2) {
        throw std::runtime_error(std::string{"expected 2-integer array for field: "} + key);
    }
    return true;
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

std::array<std::uint8_t, 3> read_rgb(const Json& object, const std::array<std::uint8_t, 3>& fallback) {
    if (!object.contains("rgb")) {
        return fallback;
    }
    const auto& value = object.at("rgb");
    if (!value.is_array() || value.size() != 3) {
        throw std::runtime_error("expected 3-value uint8 array for modelMaterial.rgb");
    }
    std::array<std::uint8_t, 3> result{};
    for (std::size_t i{0}; i < result.size(); ++i) {
        const int channel{value.at(i).as_int()};
        if (channel < 0 || channel > 255) {
            throw std::runtime_error("modelMaterial.rgb value out of uint8 range");
        }
        result.at(i) = static_cast<std::uint8_t>(channel);
    }
    return result;
}

std::array<std::uint8_t, 3> read_rgb_field(
    const Json& object,
    const char* key,
    const std::array<std::uint8_t, 3>& fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    const auto& value = object.at(key);
    if (!value.is_array() || value.size() != 3) {
        throw std::runtime_error(std::string{"expected 3-value uint8 array for field: "} + key);
    }
    std::array<std::uint8_t, 3> result{};
    for (std::size_t i{0}; i < result.size(); ++i) {
        const int channel{value.at(i).as_int()};
        if (channel < 0 || channel > 255) {
            throw std::runtime_error(std::string{"RGB config field value out of uint8 range: "} + key);
        }
        result.at(i) = static_cast<std::uint8_t>(channel);
    }
    return result;
}

std::uint8_t read_legacy_u16_as_u8(const Json& object, const char* key, const std::uint8_t fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    const int value{object.at(key).as_int()};
    if (value < 0 || value > 65535) {
        throw std::runtime_error(std::string{"legacy uint16 config field out of range: "} + key);
    }
    if (value > 255) {
        return static_cast<std::uint8_t>(value / 257);
    }
    return static_cast<std::uint8_t>(value);
}

}  // namespace

SliceConfig load_slice_config(const std::filesystem::path& config_path) {
    std::ifstream input{config_path};
    if (!input) {
        throw std::runtime_error("failed to open config: " + config_path.string());
    }

    const Json root = Json::parse(input);
    SliceConfig config;
    config.slicing_mode = root.value("slicingMode", config.slicing_mode);

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
        if (output.contains("storageMode")) {
            config.output.storage_mode = output.at("storageMode").as_string();
            config.output.tiled = config.output.storage_mode == "tiled";
        } else if (output.contains("tiled")) {
            config.output.tiled = output.at("tiled").as_bool();
            config.output.storage_mode = config.output.tiled ? "tiled" : "stripped";
        }
        config.output.tile_size = read_int2(output, "tileSize", config.output.tile_size);
        config.output.rows_per_strip = output.value("rowsPerStrip", config.output.rows_per_strip);
    }

    if (root.contains("modelTransform")) {
        const auto& transform = root.at("modelTransform");
        config.transform.unit = transform.value("unit", config.transform.unit);
        config.transform.scale = read_double3(transform, "scale", config.transform.scale);
        config.transform.rotation_deg = read_double3(transform, "rotationDeg", config.transform.rotation_deg);
        config.transform.translation_mm = read_double3(transform, "translationMm", config.transform.translation_mm);
    }

    if (root.contains("autoOrient")) {
        const auto& auto_orient = root.at("autoOrient");
        config.auto_orient.enabled = auto_orient.value("enabled", config.auto_orient.enabled);
        config.auto_orient.max_height_mm = auto_orient.value("maxHeightMm", config.auto_orient.max_height_mm);
        config.auto_orient.strategy = auto_orient.value("strategy", config.auto_orient.strategy);
    }

    if (root.contains("background")) {
        const auto& background = root.at("background");
        config.background.value = read_u8(background, "value", config.background.value);
    }

    if (root.contains("modelMaterial")) {
        const auto& material = root.at("modelMaterial");
        config.material.material_channel = material.value("materialChannel", config.material.material_channel);
        config.material.apply_mode = material.value("applyMode", config.material.apply_mode);
        config.material.rgb = read_rgb(material, config.material.rgb);
        config.material.white_value = read_u8(material, "whiteValue", config.material.white_value);
        config.material.varnish_value = read_u8(material, "varnishValue", config.material.varnish_value);
        config.material.white_value = read_legacy_u16_as_u8(material, "whiteStrength", config.material.white_value);
        config.material.varnish_value =
            read_legacy_u16_as_u8(material, "varnishStrength", config.material.varnish_value);
    }

    if (root.contains("texture")) {
        const auto& texture = root.at("texture");
        config.texture.enabled = texture.value("enabled", config.texture.enabled);
        config.texture.apply_mode = texture.value("applyMode", config.texture.apply_mode);
        config.texture.sampler = texture.value("sampler", config.texture.sampler);
        config.texture.uv_address_mode = texture.value("uvAddressMode", config.texture.uv_address_mode);
        config.texture.flip_v = texture.value("flipV", config.texture.flip_v);
        config.texture.fallback_rgb = read_rgb_field(texture, "fallbackRgb", config.texture.fallback_rgb);
        config.texture.missing_texture_policy =
            texture.value("missingTexturePolicy", config.texture.missing_texture_policy);
    }

    if (root.contains("materialPolicy")) {
        const auto& policy = root.at("materialPolicy");
        config.material_policy.enabled = policy.value("enabled", config.material_policy.enabled);
        config.material_policy.conflict_policy =
            policy.value("conflictPolicy", config.material_policy.conflict_policy);
        if (policy.contains("rgb")) {
            const auto& rgb = policy.at("rgb");
            config.material_policy.rgb.enabled = rgb.value("enabled", config.material_policy.rgb.enabled);
            config.material_policy.rgb.source = rgb.value("source", config.material_policy.rgb.source);
        }
        if (policy.contains("white")) {
            const auto& white = policy.at("white");
            config.material_policy.white.enabled = white.value("enabled", config.material_policy.white.enabled);
            config.material_policy.white.mode = white.value("mode", config.material_policy.white.mode);
            config.material_policy.white.value = read_u8(white, "value", config.material_policy.white.value);
            config.material_policy.white.layers = white.value("layers", config.material_policy.white.layers);
        }
        if (policy.contains("varnish")) {
            const auto& varnish = policy.at("varnish");
            config.material_policy.varnish.enabled = varnish.value("enabled", config.material_policy.varnish.enabled);
            config.material_policy.varnish.mode = varnish.value("mode", config.material_policy.varnish.mode);
            config.material_policy.varnish.value =
                read_u8(varnish, "value", config.material_policy.varnish.value);
            config.material_policy.varnish.top_layers =
                varnish.value("topLayers", config.material_policy.varnish.top_layers);
        }
    }

    if (root.contains("materialRoleMapping")) {
        const auto& mapping = root.at("materialRoleMapping");
        config.material_role_mapping.enabled = mapping.value("enabled", config.material_role_mapping.enabled);
        config.material_role_mapping.mode = mapping.value("mode", config.material_role_mapping.mode);
        config.material_role_mapping.default_role =
            mapping.value("defaultRole", config.material_role_mapping.default_role);
        config.material_role_mapping.allow_input_support_material =
            mapping.value("allowInputSupportMaterial", config.material_role_mapping.allow_input_support_material);
        if (mapping.contains("rules")) {
            const auto& rules = mapping.at("rules");
            if (!rules.is_array()) {
                throw std::runtime_error("materialRoleMapping.rules must be an array");
            }
            config.material_role_mapping.rules.clear();
            for (const auto& rule_json : rules.as_array()) {
                MaterialRoleRuleConfig rule;
                rule.match_name_contains = rule_json.value("matchNameContains", rule.match_name_contains);
                rule.role = rule_json.value("role", rule.role);
                config.material_role_mapping.rules.push_back(rule);
            }
        }
    }

    if (root.contains("support")) {
        const auto& support = root.at("support");
        config.support.enabled = support.value("enabled", config.support.enabled);
        config.support.mode = support.value("mode", config.support.mode);
        config.support.value = read_u8(support, "value", config.support.value);
        config.support.value = read_legacy_u16_as_u8(support, "strength", config.support.value);
        config.support.offset_mm = support.value("offsetMm", config.support.offset_mm);
        config.support.min_area_px = support.value("minAreaPx", config.support.min_area_px);
        config.support.min_overlap_ratio = support.value("minOverlapRatio", config.support.min_overlap_ratio);
        config.support.min_island_area_px = support.value("minIslandAreaPx", config.support.min_island_area_px);
        config.support.connectivity = support.value("connectivity", config.support.connectivity);
        config.support.unsupported_projection =
            support.value("unsupportedProjection", config.support.unsupported_projection);
        config.support.xy_dilation_px = support.value("xyDilationPx", config.support.xy_dilation_px);
        config.support.write_support_type_debug =
            support.value("writeSupportTypeDebug", config.support.write_support_type_debug);
    }

    if (root.contains("preview")) {
        const auto& preview = root.at("preview");
        config.preview.enabled = preview.value("enabled", config.preview.enabled);
        config.preview.format = preview.value("format", config.preview.format);
        config.preview.interval = preview.value("interval", config.preview.interval);
        if (has_int2(preview, "layerRange")) {
            config.preview.has_layer_range = true;
            config.preview.layer_range = read_int2(preview, "layerRange", config.preview.layer_range);
        }
        config.preview.channels = read_string_array(preview, "channels", config.preview.channels);
        config.preview.only_non_empty_layers =
            preview.value("onlyNonEmptyLayers", config.preview.only_non_empty_layers);
    }

    if (root.contains("relief")) {
        const auto& relief = root.at("relief");
        config.relief.fill_mode = relief.value("fillMode", config.relief.fill_mode);
        config.relief.base_z_mm = relief.value("baseZMm", config.relief.base_z_mm);
    }

    validate_slice_config(config);
    return config;
}

void validate_slice_config(const SliceConfig& config) {
    if (config.input.model_path.empty()) {
        throw std::runtime_error("input.modelPath must not be empty");
    }
    if (config.slicing_mode != "closed_mesh_scanline" && config.slicing_mode != "relief_heightfield") {
        throw std::runtime_error("slicingMode must be closed_mesh_scanline or relief_heightfield");
    }
    if (config.output.dpi_x != 600 || config.output.dpi_y != 600) {
        throw std::runtime_error("P0 requires dpiX == dpiY == 600");
    }
    if (config.output.layer_thickness_mm <= 0.0) {
        throw std::runtime_error("output.layerThicknessMm must be positive");
    }
    if (config.auto_orient.max_height_mm <= 0.0) {
        throw std::runtime_error("autoOrient.maxHeightMm must be positive");
    }
    if (config.auto_orient.strategy != "minimize_height_by_right_angle_rotation") {
        throw std::runtime_error("P0 only supports autoOrient.strategy == minimize_height_by_right_angle_rotation");
    }
    if (config.background.value != 255) {
        throw std::runtime_error("P0 00B requires background.value == 255");
    }
    if (config.output.bit_depth != 8) {
        throw std::runtime_error("P0 00B requires output.bitDepth == 8");
    }
    if (config.output.planar_config != "contiguous") {
        throw std::runtime_error("P0 requires output.planarConfig == contiguous");
    }
    if (config.output.storage_mode != "stripped" && config.output.storage_mode != "tiled") {
        throw std::runtime_error("output.storageMode must be stripped or tiled");
    }
    if (config.output.storage_mode == "tiled"
        && (config.output.tile_size.at(0) <= 0 || config.output.tile_size.at(1) <= 0)) {
        throw std::runtime_error("output.tileSize values must be positive");
    }
    if (config.output.storage_mode == "stripped" && config.output.rows_per_strip <= 0) {
        throw std::runtime_error("output.rowsPerStrip must be positive for stripped TIFF output");
    }
    if (config.support.enabled && config.support.mode != "bottom_projection"
        && config.support.mode != "unsupported_only"
        && config.support.mode != "bottom_projection_plus_unsupported"
        && config.support.mode != "full_vertical_projection") {
        throw std::runtime_error(
            "support.mode must be bottom_projection, unsupported_only, bottom_projection_plus_unsupported, or full_vertical_projection");
    }
    if (config.support.min_overlap_ratio < 0.0 || config.support.min_overlap_ratio > 1.0) {
        throw std::runtime_error("support.minOverlapRatio must be between 0 and 1");
    }
    if (config.support.min_island_area_px < 0) {
        throw std::runtime_error("support.minIslandAreaPx must be non-negative");
    }
    if (config.support.connectivity != 4 && config.support.connectivity != 8) {
        throw std::runtime_error("support.connectivity must be 4 or 8");
    }
    if (config.support.unsupported_projection != "project_to_build_plate"
        && config.support.unsupported_projection != "project_to_nearest_supported_layer") {
        throw std::runtime_error(
            "support.unsupportedProjection must be project_to_build_plate or project_to_nearest_supported_layer");
    }
    if (config.support.unsupported_projection == "project_to_nearest_supported_layer") {
        throw std::runtime_error("support.unsupportedProjection project_to_nearest_supported_layer is not implemented in 02");
    }
    if (config.support.xy_dilation_px < 0) {
        throw std::runtime_error("support.xyDilationPx must be non-negative");
    }
    if (config.material.material_channel != "auto" && config.material.material_channel != "RGB"
        && config.material.material_channel != "W" && config.material.material_channel != "V") {
        throw std::runtime_error("modelMaterial.materialChannel must be auto, RGB, W, or V");
    }
    if (config.material.apply_mode != "solid_volume") {
        throw std::runtime_error("00C only supports modelMaterial.applyMode == solid_volume");
    }
    if (config.texture.enabled) {
        if (config.texture.apply_mode != "solid_volume_from_top_surface") {
            throw std::runtime_error("04 only supports texture.applyMode == solid_volume_from_top_surface");
        }
        if (config.texture.sampler != "nearest" && config.texture.sampler != "bilinear") {
            throw std::runtime_error("texture.sampler must be nearest or bilinear");
        }
        if (config.texture.uv_address_mode != "clamp" && config.texture.uv_address_mode != "repeat") {
            throw std::runtime_error("texture.uvAddressMode must be clamp or repeat");
        }
        if (config.texture.missing_texture_policy != "warn_and_fallback"
            && config.texture.missing_texture_policy != "fail_fast") {
            throw std::runtime_error("texture.missingTexturePolicy must be warn_and_fallback or fail_fast");
        }
        if (config.slicing_mode != "relief_heightfield") {
            throw std::runtime_error("04 texture.enabled currently requires relief_heightfield");
        }
    }
    if (config.material_policy.enabled) {
        if (config.material_policy.rgb.source != "texture_or_fallback"
            && config.material_policy.rgb.source != "modelMaterial") {
            throw std::runtime_error("materialPolicy.rgb.source must be texture_or_fallback or modelMaterial");
        }
        if (config.material_policy.white.mode != "disabled"
            && config.material_policy.white.mode != "underbase"
            && config.material_policy.white.mode != "all_model") {
            throw std::runtime_error("materialPolicy.white.mode must be disabled, underbase, or all_model");
        }
        if (config.material_policy.white.layers != "all_model") {
            throw std::runtime_error("materialPolicy.white.layers currently supports all_model");
        }
        if (config.material_policy.varnish.mode != "disabled"
            && config.material_policy.varnish.mode != "all_model"
            && config.material_policy.varnish.mode != "top_n_layers") {
            throw std::runtime_error("materialPolicy.varnish.mode must be disabled, all_model, or top_n_layers");
        }
        if (config.material_policy.varnish.top_layers <= 0) {
            throw std::runtime_error("materialPolicy.varnish.topLayers must be positive");
        }
        if (config.material_policy.conflict_policy != "model_material_over_support") {
            throw std::runtime_error("materialPolicy.conflictPolicy must be model_material_over_support");
        }
    }
    if (config.material_role_mapping.enabled) {
        if (config.material_role_mapping.mode != "rules_then_default") {
            throw std::runtime_error("materialRoleMapping.mode must be rules_then_default");
        }
        const auto valid_role = [](const std::string& role) {
            return role == "rgb" || role == "white" || role == "varnish" || role == "ignore"
                || role == "support_candidate" || role == "support";
        };
        if (!valid_role(config.material_role_mapping.default_role)) {
            throw std::runtime_error("materialRoleMapping.defaultRole is invalid");
        }
        for (const MaterialRoleRuleConfig& rule : config.material_role_mapping.rules) {
            if (rule.match_name_contains.empty()) {
                throw std::runtime_error("materialRoleMapping.rules[].matchNameContains must not be empty");
            }
            if (!valid_role(rule.role)) {
                throw std::runtime_error("materialRoleMapping.rules[].role is invalid");
            }
            if (rule.role == "support" && !config.material_role_mapping.allow_input_support_material) {
                throw std::runtime_error(
                    "materialRoleMapping role=support requires allowInputSupportMaterial=true");
            }
        }
    }
    if (config.relief.fill_mode != "surface_to_base" && config.relief.fill_mode != "intersection_range") {
        throw std::runtime_error("relief.fillMode must be surface_to_base or intersection_range");
    }
    if (config.relief.base_z_mm < 0.0) {
        throw std::runtime_error("relief.baseZMm must be non-negative");
    }
    if (config.preview.format != "ppm" && config.preview.format != "png") {
        throw std::runtime_error("preview.format must be ppm or png");
    }
    if (config.preview.interval <= 0) {
        throw std::runtime_error("preview.interval must be positive");
    }
    if (config.preview.has_layer_range) {
        if (config.preview.layer_range.at(0) < 0 || config.preview.layer_range.at(1) < config.preview.layer_range.at(0)) {
            throw std::runtime_error("preview.layerRange must be [start, end] with start >= 0 and end >= start");
        }
    }
    for (const std::string& channel : config.preview.channels) {
        if (channel != "rgb" && channel != "model_rgb" && channel != "support" && channel != "s"
            && channel != "white" && channel != "w" && channel != "varnish" && channel != "v"
            && channel != "texture_rgb" && channel != "model_rgb_true_color" && channel != "true_rgb") {
            throw std::runtime_error("preview.channels supports rgb, texture_rgb, support, white, varnish");
        }
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
