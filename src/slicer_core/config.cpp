#include "slicer_core/config.h"

#include "slicer_core/config/ConfigMigration.h"
#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/json_value.h"
#include "slicer_core/materials/texture_application/TextureFillPartitionTypes.h"

#include <algorithm>
#include <cmath>
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

bool IsSupportedOutputDpi(const int dpi) noexcept
{
    return dpi >= kMinimumOutputDpi && dpi <= kMaximumOutputDpi;
}

bool IsOutputPixelSizeConsistent(
    const int dpi,
    const double pixelSizeMm) noexcept
{
    if (!IsSupportedOutputDpi(dpi)
        || !std::isfinite(pixelSizeMm)
        || pixelSizeMm <= 0.0)
    {
        return false;
    }
    const double expectedPixelSizeMm =
        kMillimetersPerInch / static_cast<double>(dpi);
    return std::abs(pixelSizeMm - expectedPixelSizeMm)
        <= kOutputPixelSizeToleranceMm;
}

SliceConfig load_slice_config(const std::filesystem::path& config_path) {
    std::ifstream input{config_path};
    if (!input) {
        throw std::runtime_error("failed to open config: " + config_path.string());
    }

    const Json root = NormalizeConfigJson(Json::parse(input));
    SliceConfig config;
    config.slicing_mode = root.value("slicingMode", config.slicing_mode);

    if (root.contains("slicePipeline"))
    {
        const Json& slicePipelineJson = root.at("slicePipeline");
        if (!slicePipelineJson.is_object()
            || !slicePipelineJson.contains("mode")
            || !slicePipelineJson.at("mode").is_string())
        {
            throw SlicePipelineError(
                SlicePipelineErrorCode::ModeUnsupported,
                "slicePipeline must be an object containing a string mode");
        }
        config.slice_pipeline.explicitly_configured = true;
        config.slice_pipeline.mode =
            ParseSlicePipelineMode(slicePipelineJson.at("mode").as_string());
    }

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
        if (output.contains("tiffCompression"))
        {
            const auto& compression = output.at("tiffCompression");
            if (!compression.is_object()
                || !compression.contains("algorithm")
                || !compression.at("algorithm").is_string())
            {
                throw std::runtime_error(
                    "output.tiffCompression must be an object containing a string algorithm");
            }
            config.output.tiff_compression =
                compression.at("algorithm").as_string();
        }
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
        config.texture.top_surface_layers = texture.value("topSurfaceLayers", config.texture.top_surface_layers);
        config.texture.sampler = texture.value("sampler", config.texture.sampler);
        config.texture.uv_address_mode = texture.value("uvAddressMode", config.texture.uv_address_mode);
        config.texture.flip_v = texture.value("flipV", config.texture.flip_v);
        config.texture.fallback_rgb = read_rgb_field(texture, "fallbackRgb", config.texture.fallback_rgb);
        config.texture.missing_texture_policy =
            texture.value("missingTexturePolicy", config.texture.missing_texture_policy);
        config.texture.non_surface_rgb_policy =
            texture.value("nonSurfaceRgbPolicy", config.texture.non_surface_rgb_policy);
        if (texture.contains("surfaceShell")) {
            const auto& surfaceShellJson = texture.at("surfaceShell");
            config.texture.surface_shell.geometry_mode =
                surfaceShellJson.value("geometryMode", config.texture.surface_shell.geometry_mode);
            config.texture.surface_shell.mode =
                surfaceShellJson.value("mode", config.texture.surface_shell.mode);
            config.texture.surface_shell.width_mm =
                surfaceShellJson.value("widthMm", config.texture.surface_shell.width_mm);
            config.texture.surface_shell.width_step_mm =
                surfaceShellJson.value("widthStepMm", config.texture.surface_shell.width_step_mm);
            config.texture.surface_shell.minimum_width_policy = surfaceShellJson.value(
                "minimumWidthPolicy",
                config.texture.surface_shell.minimum_width_policy);
            config.texture.surface_shell.surface_scope =
                surfaceShellJson.value("surfaceScope", config.texture.surface_shell.surface_scope);
            config.texture.surface_shell.full_texture_at_model_limit = surfaceShellJson.value(
                "fullTextureAtModelLimit",
                config.texture.surface_shell.full_texture_at_model_limit);
        }
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

    if (root.contains("modelFill")) {
        const auto& model_fill = root.at("modelFill");
        config.model_fill.enabled = model_fill.value("enabled", config.model_fill.enabled);
        config.model_fill.material = model_fill.value("material", config.model_fill.material);
        config.model_fill.scope = model_fill.value("scope", config.model_fill.scope);
        config.model_fill.value = read_u8(model_fill, "value", config.model_fill.value);
        config.model_fill.empty_allowed_in_production =
            model_fill.value("emptyAllowedInProduction", config.model_fill.empty_allowed_in_production);
        config.model_fill.legacy_rgb_fallback =
            model_fill.value("legacyRgbFallback", config.model_fill.legacy_rgb_fallback);
    }

    if (root.contains("materialProcessProfile")) {
        const auto& profile = root.at("materialProcessProfile");
        config.material_process_profile.enabled = profile.value("enabled", config.material_process_profile.enabled);
        config.material_process_profile.name = profile.value("name", config.material_process_profile.name);
        config.material_process_profile.target = profile.value("target", config.material_process_profile.target);
        if (profile.contains("rgb")) {
            const auto& rgb = profile.at("rgb");
            config.material_process_profile.rgb.enabled =
                rgb.value("enabled", config.material_process_profile.rgb.enabled);
            config.material_process_profile.rgb.source =
                rgb.value("source", config.material_process_profile.rgb.source);
        }
        if (profile.contains("white")) {
            const auto& white = profile.at("white");
            config.material_process_profile.white.enabled =
                white.value("enabled", config.material_process_profile.white.enabled);
            config.material_process_profile.white.mode =
                white.value("mode", config.material_process_profile.white.mode);
            config.material_process_profile.white.coverage =
                white.value("coverage", config.material_process_profile.white.coverage);
            config.material_process_profile.white.value =
                read_u8(white, "value", config.material_process_profile.white.value);
            config.material_process_profile.white.expand_px =
                white.value("expandPx", config.material_process_profile.white.expand_px);
            config.material_process_profile.white.shrink_px =
                white.value("shrinkPx", config.material_process_profile.white.shrink_px);
        }
        if (profile.contains("varnish")) {
            const auto& varnish = profile.at("varnish");
            config.material_process_profile.varnish.enabled =
                varnish.value("enabled", config.material_process_profile.varnish.enabled);
            config.material_process_profile.varnish.mode =
                varnish.value("mode", config.material_process_profile.varnish.mode);
            config.material_process_profile.varnish.top_layers =
                varnish.value("topLayers", config.material_process_profile.varnish.top_layers);
            config.material_process_profile.varnish.value =
                read_u8(varnish, "value", config.material_process_profile.varnish.value);
            config.material_process_profile.varnish.coverage =
                varnish.value("coverage", config.material_process_profile.varnish.coverage);
        }
        if (profile.contains("support")) {
            const auto& support = profile.at("support");
            config.material_process_profile.support.expected =
                support.value("expected", config.material_process_profile.support.expected);
            config.material_process_profile.support.mode =
                support.value("mode", config.material_process_profile.support.mode);
        }
        if (profile.contains("validation")) {
            const auto& validation = profile.at("validation");
            config.material_process_profile.validation.require_rgb_pixels =
                validation.value("requireRgbPixels", config.material_process_profile.validation.require_rgb_pixels);
            config.material_process_profile.validation.require_white_pixels =
                validation.value("requireWhitePixels", config.material_process_profile.validation.require_white_pixels);
            config.material_process_profile.validation.require_varnish_pixels =
                validation.value(
                    "requireVarnishPixels",
                    config.material_process_profile.validation.require_varnish_pixels);
            config.material_process_profile.validation.require_support_pixels =
                validation.value(
                    "requireSupportPixels",
                    config.material_process_profile.validation.require_support_pixels);
            config.material_process_profile.validation.max_unexpected_overlap_pixels =
                validation.value(
                    "maxUnexpectedOverlapPixels",
                    config.material_process_profile.validation.max_unexpected_overlap_pixels);
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
        if (support.contains("placement")) {
            config.support.placement = support.value("placement", config.support.placement);
            config.support.placement_explicit = true;
        }
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
        if (support.contains("shape"))
        {
            const auto& shape = support.at("shape");
            config.support.shape_enabled = shape.value("enabled", config.support.shape_enabled);
            config.support.shape_min_component_area_px =
                shape.value("minComponentAreaPx", config.support.shape_min_component_area_px);
            config.support.shape_xy_dilation_px = shape.value("xyDilationPx", config.support.shape_xy_dilation_px);
            config.support.shape_closing_radius_px =
                shape.value("closingRadiusPx", config.support.shape_closing_radius_px);
            config.support.shape_bridge_gap_px = shape.value("bridgeGapPx", config.support.shape_bridge_gap_px);
            config.support.shape_preserve_model_priority =
                shape.value("preserveModelPriority", config.support.shape_preserve_model_priority);
            config.support.shape_max_added_support_ratio =
                shape.value("maxAddedSupportRatio", config.support.shape_max_added_support_ratio);
        }
        if (support.contains("internalVoid"))
        {
            const auto& internal_void = support.at("internalVoid");
            config.support.internal_void.enabled =
                internal_void.value("enabled", config.support.internal_void.enabled);
            config.support.internal_void.min_area_px =
                internal_void.value("minAreaPx", config.support.internal_void.min_area_px);
            config.support.internal_void.fill_rule =
                internal_void.value("fillRule", config.support.internal_void.fill_rule);
        }
        if (support.contains("upper"))
        {
            const auto& upper = support.at("upper");
            config.support.upper.enabled = upper.value("enabled", config.support.upper.enabled);
            config.support.upper.outside = upper.value("outside", config.support.upper.outside);
            config.support.upper.reason = upper.value("reason", config.support.upper.reason);
        }
        if (support.contains("baseProjection"))
        {
            const auto& baseProjection =
                support.at("baseProjection");
            config.support.base_projection.enabled =
                baseProjection.value(
                    "enabled",
                    config.support.base_projection.enabled);
            config.support.base_projection.layer_count =
                baseProjection.value(
                    "layerCount",
                    config.support.base_projection.layer_count);
            config.support.base_projection.layer_placement =
                baseProjection.value(
                    "layerPlacement",
                    config.support.base_projection.layer_placement);
            config.support.base_projection.source =
                baseProjection.value(
                    "source",
                    config.support.base_projection.source);
        }
    }

    if (root.contains("outerVarnish")) {
        const auto& outer_varnish = root.at("outerVarnish");
        config.outer_varnish.enabled = outer_varnish.value("enabled", config.outer_varnish.enabled);
        config.outer_varnish.thickness_mm =
            outer_varnish.value("thicknessMm", config.outer_varnish.thickness_mm);
        config.outer_varnish.thickness_step_mm =
            outer_varnish.value("thicknessStepMm", config.outer_varnish.thickness_step_mm);
        config.outer_varnish.pixel_pitch_um =
            outer_varnish.value("pixelPitchUm", config.outer_varnish.pixel_pitch_um);
        config.outer_varnish.allow_xy_expansion =
            outer_varnish.value("allowXYExpansion", config.outer_varnish.allow_xy_expansion);
        config.outer_varnish.conflict_policy =
            outer_varnish.value("conflictPolicy", config.outer_varnish.conflict_policy);
        config.outer_varnish.value = read_u8(outer_varnish, "value", config.outer_varnish.value);
    }

    if (root.contains("surfaceVarnish"))
    {
        const auto& surface_varnish = root.at("surfaceVarnish");
        config.surface_varnish.enabled = surface_varnish.value("enabled", config.surface_varnish.enabled);
        config.surface_varnish.outer_surface =
            surface_varnish.value("outerSurface", config.surface_varnish.outer_surface);
        config.surface_varnish.inner_surface =
            surface_varnish.value("innerSurface", config.surface_varnish.inner_surface);
        config.surface_varnish.thickness_px =
            surface_varnish.value("thicknessPx", config.surface_varnish.thickness_px);
        config.surface_varnish.value = read_u8(surface_varnish, "value", config.surface_varnish.value);
        config.surface_varnish.source = surface_varnish.value("source", config.surface_varnish.source);
    }

    if (root.contains("materialClosure"))
    {
        const auto& materialClosure = root.at("materialClosure");
        config.material_closure.enabled =
            materialClosure.value("enabled", config.material_closure.enabled);
        config.material_closure.mode = materialClosure.value("mode", config.material_closure.mode);
        config.material_closure.connectivity =
            materialClosure.value("connectivity", config.material_closure.connectivity);
        config.material_closure.max_gap_px =
            materialClosure.value("maxGapPx", config.material_closure.max_gap_px);
        config.material_closure.fail_on_gap =
            materialClosure.value("failOnGap", config.material_closure.fail_on_gap);
        config.material_closure.write_gap_preview =
            materialClosure.value("writeGapPreview", config.material_closure.write_gap_preview);
        if (materialClosure.contains("repair"))
        {
            const auto& repair = materialClosure.at("repair");
            config.material_closure.repair.enabled =
                repair.value("enabled", config.material_closure.repair.enabled);
            config.material_closure.repair.color_fill_gap =
                repair.value("colorFillGap", config.material_closure.repair.color_fill_gap);
            config.material_closure.repair.model_support_gap =
                repair.value("modelSupportGap", config.material_closure.repair.model_support_gap);
            config.material_closure.repair.internal_void_gap =
                repair.value("internalVoidGap", config.material_closure.repair.internal_void_gap);
            config.material_closure.repair.varnish_support_gap =
                repair.value("varnishSupportGap", config.material_closure.repair.varnish_support_gap);
        }
    }

    if (root.contains("preview")) {
        const auto& preview = root.at("preview");
        if (preview.contains("outputPolicy"))
        {
            config.preview.output_policy =
                preview.value("outputPolicy", config.preview.output_policy);
            config.preview.enabled =
                config.preview.output_policy == "tiff_native_with_diagnostics";
        }
        else
        {
            config.preview.enabled =
                preview.value("enabled", config.preview.enabled);
            config.preview.output_policy =
                config.preview.enabled
                    ? "tiff_native_with_diagnostics"
                    : "tiff_native";
        }
        config.preview.format = preview.value("format", config.preview.format);
        config.preview.interval = preview.value("interval", config.preview.interval);
        if (has_int2(preview, "layerRange")) {
            config.preview.has_layer_range = true;
            config.preview.layer_range = read_int2(preview, "layerRange", config.preview.layer_range);
        }
        config.preview.channels = read_string_array(preview, "channels", config.preview.channels);
        config.preview.only_non_empty_layers =
            preview.value("onlyNonEmptyLayers", config.preview.only_non_empty_layers);
        if (preview.contains("pseudoColors")) {
            const auto& colors = preview.at("pseudoColors");
            config.preview.empty_color = read_rgb_field(colors, "empty", config.preview.empty_color);
            config.preview.support_color = read_rgb_field(colors, "support", config.preview.support_color);
            config.preview.white_color = read_rgb_field(colors, "white", config.preview.white_color);
            config.preview.varnish_color = read_rgb_field(colors, "varnish", config.preview.varnish_color);
        }
    }

    if (root.contains("relief")) {
        const auto& relief = root.at("relief");
        config.relief.fill_mode = relief.value("fillMode", config.relief.fill_mode);
        config.relief.base_z_mm = relief.value("baseZMm", config.relief.base_z_mm);
    }

    if (root.contains("experimental"))
    {
        const auto& experimental = root.at("experimental");
        if (experimental.contains("openvdbPipeline"))
        {
            const auto& openvdb = experimental.at("openvdbPipeline");
            config.experimental.openvdb_pipeline.enabled =
                openvdb.value("enabled", config.experimental.openvdb_pipeline.enabled);
            config.experimental.openvdb_pipeline.engine =
                openvdb.value("engine", config.experimental.openvdb_pipeline.engine);
            config.experimental.openvdb_pipeline.admission_mode =
                openvdb.value("admissionMode", config.experimental.openvdb_pipeline.admission_mode);
            config.experimental.openvdb_pipeline.failure_policy =
                openvdb.value("failurePolicy", config.experimental.openvdb_pipeline.failure_policy);
            config.experimental.openvdb_pipeline.allow_non_production_output =
                openvdb.value(
                    "allowNonProductionOutput",
                    config.experimental.openvdb_pipeline.allow_non_production_output);
            config.experimental.openvdb_pipeline.write_production_rgbwsv =
                openvdb.value(
                    "writeProductionRgbwsv",
                    config.experimental.openvdb_pipeline.write_production_rgbwsv);
        }
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
    const bool globalTextureConfigured =
        config.texture.enabled
        && config.texture.apply_mode == "global_surface_shell";
    const bool globalFillConfigured =
        config.model_fill.scope == "complement_of_global_texture_shell";
    if (config.slice_pipeline.explicitly_configured
        && config.slice_pipeline.mode == SlicePipelineMode::Legacy
        && (globalTextureConfigured || globalFillConfigured))
    {
        throw SlicePipelineError(
            SlicePipelineErrorCode::ConfigMismatch,
            "explicit legacy mode cannot use global_surface_shell-only material semantics");
    }
    if (config.slice_pipeline.mode == SlicePipelineMode::GlobalSurfaceShell
        && (!globalTextureConfigured
            || !config.model_fill.enabled
            || !globalFillConfigured))
    {
        throw SlicePipelineError(
            SlicePipelineErrorCode::ConfigMismatch,
            "global_surface_shell mode requires matching texture and modelFill configuration");
    }
    if (!IsSupportedOutputDpi(config.output.dpi_x))
    {
        throw std::runtime_error(
            "output.dpiX must be between "
            + std::to_string(kMinimumOutputDpi)
            + " and "
            + std::to_string(kMaximumOutputDpi));
    }
    if (!IsSupportedOutputDpi(config.output.dpi_y))
    {
        throw std::runtime_error(
            "output.dpiY must be between "
            + std::to_string(kMinimumOutputDpi)
            + " and "
            + std::to_string(kMaximumOutputDpi));
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
    if (config.output.tiff_compression != "none"
        && config.output.tiff_compression != "packbits")
    {
        throw std::runtime_error(
            "output.tiffCompression.algorithm must be none or packbits");
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
    if (config.support.placement != "lower"
        && config.support.placement != "upper"
        && config.support.placement != "both"
        && config.support.placement != "unsupported_only"
        && config.support.placement != "full_vertical_projection") {
        throw std::runtime_error(
            "support.placement must be lower, upper, both, unsupported_only, or full_vertical_projection");
    }
    if (config.support.internal_void.min_area_px < 0) {
        throw std::runtime_error("support.internalVoid.minAreaPx must be non-negative");
    }
    if (config.support.internal_void.fill_rule != "all_internal_voids") {
        throw std::runtime_error("support.internalVoid.fillRule must be all_internal_voids");
    }
    if (config.support.upper.outside != "outer_varnish_shell"
        && config.support.upper.outside != "model_envelope") {
        throw std::runtime_error("support.upper.outside must be outer_varnish_shell or model_envelope");
    }
    if (config.support.base_projection.layer_count < 0)
    {
        throw std::runtime_error(
            "support.baseProjection.layerCount must be non-negative");
    }
    if (config.support.base_projection.layer_count > 1000)
    {
        throw std::runtime_error(
            "support.baseProjection.layerCount must not exceed 1000");
    }
    if (config.support.base_projection.source
        != "max_support_footprint")
    {
        throw std::runtime_error(
            "support.baseProjection.source must be max_support_footprint");
    }
    if (config.support.base_projection.layer_placement
            != "overlay_existing"
        && config.support.base_projection.layer_placement
            != "prepend_below_model")
    {
        throw std::runtime_error(
            "support.baseProjection.layerPlacement must be overlay_existing or prepend_below_model");
    }
    if (config.material.material_channel != "auto" && config.material.material_channel != "RGB"
        && config.material.material_channel != "W" && config.material.material_channel != "V") {
        throw std::runtime_error("modelMaterial.materialChannel must be auto, RGB, W, or V");
    }
    if (config.material.apply_mode != "solid_volume") {
        throw std::runtime_error("00C only supports modelMaterial.applyMode == solid_volume");
    }
    if (config.model_fill.material != "white"
        && config.model_fill.material != "varnish"
        && config.model_fill.material != "rgb"
        && config.model_fill.material != "profile_default"
        && config.model_fill.material != "material_role") {
        throw std::runtime_error("modelFill.material must be white, varnish, rgb, profile_default, or material_role");
    }
    if (config.model_fill.scope != "solid_volume"
        && config.model_fill.scope != "below_texture_surface"
        && config.model_fill.scope != "all_model"
        && config.model_fill.scope != "complement_of_global_texture_shell") {
        if (config.texture.enabled && config.texture.apply_mode == "global_surface_shell")
        {
            throw TextureFillPartitionError(
                TextureFillPartitionErrorCode::TextureFillScopeMismatch,
                "global_surface_shell requires modelFill.scope=complement_of_global_texture_shell");
        }
        throw std::runtime_error(
            "modelFill.scope must be solid_volume, below_texture_surface, all_model, or complement_of_global_texture_shell");
    }
    if (config.model_fill.enabled
        && !config.model_fill.empty_allowed_in_production
        && !config.model_fill.legacy_rgb_fallback
        && config.model_fill.material == "rgb"
        && config.texture.enabled
        && config.texture.non_surface_rgb_policy == "empty") {
        throw std::runtime_error(
            "modelFill production profile cannot use rgb fill with texture.nonSurfaceRgbPolicy=empty");
    }
    if (config.outer_varnish.thickness_mm < 0.0) {
        throw std::runtime_error("outerVarnish.thicknessMm must be non-negative");
    }
    if (config.outer_varnish.thickness_step_mm <= 0.0) {
        throw std::runtime_error("outerVarnish.thicknessStepMm must be positive");
    }
    if (config.outer_varnish.pixel_pitch_um <= 0.0) {
        throw std::runtime_error("outerVarnish.pixelPitchUm must be positive");
    }
    if (config.outer_varnish.conflict_policy != "varnish_shell_wins") {
        throw std::runtime_error("outerVarnish.conflictPolicy must be varnish_shell_wins");
    }
    if (config.surface_varnish.thickness_px < 0) {
        throw std::runtime_error("surfaceVarnish.thicknessPx must be non-negative");
    }
    if (config.surface_varnish.enabled && config.surface_varnish.thickness_px <= 0) {
        throw std::runtime_error("surfaceVarnish.thicknessPx must be positive when enabled");
    }
    if (config.surface_varnish.source != "explicit" && config.surface_varnish.source != "material_policy") {
        throw std::runtime_error("surfaceVarnish.source must be explicit or material_policy");
    }
    if (config.material_closure.mode != "diagnostic"
        && config.material_closure.mode != "repair_then_report")
    {
        throw std::runtime_error("materialClosure.mode must be diagnostic or repair_then_report");
    }
    if (config.material_closure.connectivity != 4 && config.material_closure.connectivity != 8)
    {
        throw std::runtime_error("materialClosure.connectivity must be 4 or 8");
    }
    if (config.material_closure.max_gap_px <= 0)
    {
        throw std::runtime_error("materialClosure.maxGapPx must be positive");
    }
    if (config.material_closure.repair.color_fill_gap != "model_fill")
    {
        throw std::runtime_error("materialClosure.repair.colorFillGap must be model_fill");
    }
    if (config.material_closure.repair.model_support_gap != "contextual")
    {
        throw std::runtime_error("materialClosure.repair.modelSupportGap must be contextual");
    }
    if (config.material_closure.repair.internal_void_gap != "support")
    {
        throw std::runtime_error("materialClosure.repair.internalVoidGap must be support");
    }
    if (config.material_closure.repair.varnish_support_gap != "support")
    {
        throw std::runtime_error("materialClosure.repair.varnishSupportGap must be support");
    }
    const bool repairMode = config.material_closure.mode == "repair_then_report";
    if (repairMode != config.material_closure.repair.enabled)
    {
        throw std::runtime_error(
            "materialClosure repair mode and enabled flag must be configured together");
    }
    if (repairMode && config.material_closure.max_gap_px != 1)
    {
        throw std::runtime_error("materialClosure repair supports maxGapPx=1 only");
    }
    if (repairMode && !config.model_fill.enabled)
    {
        throw std::runtime_error("materialClosure repair requires modelFill.enabled=true");
    }
    if (repairMode && !config.support.enabled)
    {
        throw std::runtime_error("materialClosure repair requires support.enabled=true");
    }
    if (config.texture.enabled)
    {
        const bool surfaceShellFromSdf = config.texture.apply_mode == "surface_shell_from_sdf";
        const bool globalSurfaceShell = config.texture.apply_mode == "global_surface_shell";
        if (config.texture.apply_mode != "solid_volume_from_top_surface"
            && config.texture.apply_mode != "top_surface_only"
            && config.texture.apply_mode != "top_surface_band"
            && !surfaceShellFromSdf
            && !globalSurfaceShell)
        {
            throw std::runtime_error(
                "texture.applyMode must be solid_volume_from_top_surface, top_surface_only, top_surface_band, surface_shell_from_sdf, or global_surface_shell");
        }
        if (surfaceShellFromSdf
            && (!config.experimental.openvdb_pipeline.enabled
                || config.experimental.openvdb_pipeline.engine != "openvdb"))
        {
            throw std::runtime_error(
                "texture.applyMode surface_shell_from_sdf requires experimental.openvdbPipeline enabled with engine=openvdb");
        }
        if (config.texture.top_surface_layers <= 0)
        {
            throw std::runtime_error("texture.topSurfaceLayers must be positive");
        }
        if (config.texture.sampler != "nearest" && config.texture.sampler != "bilinear")
        {
            throw std::runtime_error("texture.sampler must be nearest or bilinear");
        }
        if (config.texture.uv_address_mode != "clamp" && config.texture.uv_address_mode != "repeat")
        {
            throw std::runtime_error("texture.uvAddressMode must be clamp or repeat");
        }
        if (config.texture.missing_texture_policy != "warn_and_fallback"
            && config.texture.missing_texture_policy != "fail_fast")
        {
            throw std::runtime_error("texture.missingTexturePolicy must be warn_and_fallback or fail_fast");
        }
        if (config.texture.non_surface_rgb_policy != "model_material"
            && config.texture.non_surface_rgb_policy != "empty"
            && config.texture.non_surface_rgb_policy != "fallback_rgb"
            && config.texture.non_surface_rgb_policy != "material_policy")
        {
            throw std::runtime_error(
                "texture.nonSurfaceRgbPolicy must be model_material, empty, fallback_rgb, or material_policy");
        }
        if (!surfaceShellFromSdf && !globalSurfaceShell && config.slicing_mode != "relief_heightfield")
        {
            throw std::runtime_error("04 texture.enabled currently requires relief_heightfield");
        }
    }

    const bool globalSurfaceShell = config.texture.enabled
        && config.texture.apply_mode == "global_surface_shell";
    const bool complementFill =
        config.model_fill.scope == "complement_of_global_texture_shell";
    if (globalSurfaceShell != complementFill)
    {
        throw TextureFillPartitionError(
            TextureFillPartitionErrorCode::TextureFillScopeMismatch,
            "global_surface_shell and complement_of_global_texture_shell must be configured together");
    }
    if (globalSurfaceShell && !config.model_fill.enabled)
    {
        throw TextureFillPartitionError(
            TextureFillPartitionErrorCode::ModelFillRequired,
            "global_surface_shell requires modelFill.enabled=true");
    }
    if (globalSurfaceShell)
    {
        const TextureSurfaceShellConfig& surfaceShell = config.texture.surface_shell;
        if (surfaceShell.mode != "partial_shell"
            && surfaceShell.mode != "all_texture")
        {
            throw TextureFillPartitionError(
                TextureFillPartitionErrorCode::SurfaceShellPartitionModeUnsupported,
                "texture.surfaceShell.mode must be partial_shell or all_texture");
        }
        if (!std::isfinite(surfaceShell.width_mm) || surfaceShell.width_mm <= 0.0)
        {
            throw TextureFillPartitionError(
                TextureFillPartitionErrorCode::SurfaceShellWidthInvalid,
                "texture.surfaceShell.widthMm must be finite and positive");
        }
        if (!std::isfinite(surfaceShell.width_step_mm)
            || std::abs(surfaceShell.width_step_mm - 0.01) > 1.0e-9)
        {
            throw TextureFillPartitionError(
                TextureFillPartitionErrorCode::SurfaceShellStepUnsupported,
                "texture.surfaceShell.widthStepMm must be exactly 0.01 mm");
        }
        if (surfaceShell.geometry_mode != "global_3d_distance")
        {
            throw TextureFillPartitionError(
                TextureFillPartitionErrorCode::SurfaceShellGeometryModeUnsupported,
                "texture.surfaceShell.geometryMode must be global_3d_distance");
        }
        if (surfaceShell.minimum_width_policy != "two_cells_floor_0_10_mm")
        {
            throw TextureFillPartitionError(
                TextureFillPartitionErrorCode::SurfaceShellMinimumPolicyUnsupported,
                "texture.surfaceShell.minimumWidthPolicy must be two_cells_floor_0_10_mm");
        }
        if (surfaceShell.surface_scope != "all_closed_surfaces")
        {
            throw TextureFillPartitionError(
                TextureFillPartitionErrorCode::SurfaceScopeUnsupported,
                "texture.surfaceShell.surfaceScope must be all_closed_surfaces");
        }
        if (!surfaceShell.full_texture_at_model_limit)
        {
            throw TextureFillPartitionError(
                TextureFillPartitionErrorCode::FullTextureAtModelLimitRequired,
                "texture.surfaceShell.fullTextureAtModelLimit must be true");
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
    if (config.material_process_profile.enabled) {
        if (config.material_process_profile.name.empty()) {
            throw std::runtime_error("materialProcessProfile.name must not be empty when enabled");
        }
        if (config.material_process_profile.target.empty()) {
            throw std::runtime_error("materialProcessProfile.target must not be empty when enabled");
        }
        if (config.material_process_profile.rgb.source != "texture_or_color"
            && config.material_process_profile.rgb.source != "modelMaterial") {
            throw std::runtime_error("materialProcessProfile.rgb.source must be texture_or_color or modelMaterial");
        }
        if (config.material_process_profile.white.mode != "disabled"
            && config.material_process_profile.white.mode != "underbase"
            && config.material_process_profile.white.mode != "all_model") {
            throw std::runtime_error("materialProcessProfile.white.mode must be disabled, underbase, or all_model");
        }
        if (config.material_process_profile.white.coverage != "all_model"
            && config.material_process_profile.white.coverage != "model_surface") {
            throw std::runtime_error("materialProcessProfile.white.coverage must be all_model or model_surface");
        }
        if (config.material_process_profile.white.expand_px < 0 || config.material_process_profile.white.shrink_px < 0) {
            throw std::runtime_error("materialProcessProfile white expandPx/shrinkPx must be non-negative");
        }
        if (config.material_process_profile.varnish.mode != "disabled"
            && config.material_process_profile.varnish.mode != "all_model"
            && config.material_process_profile.varnish.mode != "top_n_layers") {
            throw std::runtime_error("materialProcessProfile.varnish.mode must be disabled, all_model, or top_n_layers");
        }
        if (config.material_process_profile.varnish.top_layers <= 0) {
            throw std::runtime_error("materialProcessProfile.varnish.topLayers must be positive");
        }
        if (config.material_process_profile.varnish.coverage != "all_model"
            && config.material_process_profile.varnish.coverage != "model_surface") {
            throw std::runtime_error("materialProcessProfile.varnish.coverage must be all_model or model_surface");
        }
        if (config.material_process_profile.support.mode != "existing_support_pipeline") {
            throw std::runtime_error("materialProcessProfile.support.mode must be existing_support_pipeline");
        }
        if (config.material_process_profile.validation.max_unexpected_overlap_pixels < 0) {
            throw std::runtime_error("materialProcessProfile.validation.maxUnexpectedOverlapPixels must be non-negative");
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
    if (config.support.shape_min_component_area_px < 0 || config.support.shape_xy_dilation_px < 0
        || config.support.shape_closing_radius_px < 0 || config.support.shape_bridge_gap_px < 0)
    {
        throw std::runtime_error("support.shape pixel fields must be non-negative");
    }
    if (config.support.shape_max_added_support_ratio < 0.0)
    {
        throw std::runtime_error("support.shape.maxAddedSupportRatio must be non-negative");
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
    if (config.preview.output_policy != "tiff_native"
        && config.preview.output_policy != "tiff_native_with_diagnostics")
    {
        throw std::runtime_error(
            "preview.outputPolicy must be tiff_native or tiff_native_with_diagnostics");
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
    if (config.experimental.openvdb_pipeline.engine != "legacy"
        && config.experimental.openvdb_pipeline.engine != "openvdb")
    {
        throw std::runtime_error("experimental.openvdbPipeline.engine must be legacy or openvdb");
    }
    if (config.experimental.openvdb_pipeline.admission_mode != "strict_closed"
        && config.experimental.openvdb_pipeline.admission_mode != "warn_and_attempt"
        && config.experimental.openvdb_pipeline.admission_mode != "diagnostic_only"
        && config.experimental.openvdb_pipeline.admission_mode != "repair_then_strict")
    {
        throw std::runtime_error(
            "experimental.openvdbPipeline.admissionMode must be strict_closed, warn_and_attempt, diagnostic_only, or repair_then_strict");
    }
    if (config.experimental.openvdb_pipeline.failure_policy != "fail_fast"
        && config.experimental.openvdb_pipeline.failure_policy != "diagnostic_only"
        && config.experimental.openvdb_pipeline.failure_policy != "non_production_only")
    {
        throw std::runtime_error(
            "experimental.openvdbPipeline.failurePolicy must be fail_fast, diagnostic_only, or non_production_only");
    }
    if (config.experimental.openvdb_pipeline.write_production_rgbwsv
        && (!config.experimental.openvdb_pipeline.enabled
            || config.experimental.openvdb_pipeline.engine != "openvdb"))
    {
        throw std::runtime_error(
            "experimental.openvdbPipeline.writeProductionRgbwsv requires enabled=true and engine=openvdb");
    }
}

std::vector<ValidationIssue> BuildExperimentalOpenVdbPipelineDiagnostics(const SliceConfig& config)
{
    std::vector<ValidationIssue> issues;
    const ExperimentalOpenVdbPipelineConfig& openvdb = config.experimental.openvdb_pipeline;
    if (!openvdb.enabled)
    {
        return issues;
    }

    const OpenVdbStatus status = GetOpenVdbStatus();
    if (openvdb.engine == "openvdb" && (!status.compiled_with_openvdb || !status.runtime_available))
    {
        issues.push_back(MakeValidationIssue(
            "OPENVDB_UNAVAILABLE",
            ValidationSeverity::Error,
            "experimental OpenVDB pipeline is enabled but OpenVDB is unavailable"));
    }
    if (openvdb.write_production_rgbwsv)
    {
        issues.push_back(MakeValidationIssue(
            "EXPERIMENTAL_RGBWSV_REQUIRES_ADMISSION",
            ValidationSeverity::Warning,
            "writeProductionRgbwsv is requested and must remain gated by production admission policy"));
        if (openvdb.admission_mode != "strict_closed")
        {
            issues.push_back(MakeValidationIssue(
                "EXPERIMENTAL_RGBWSV_REQUIRES_STRICT_ADMISSION",
                ValidationSeverity::Error,
                "writeProductionRgbwsv requires admissionMode=strict_closed and must not run under diagnostic_only, warn_and_attempt, or repair_then_strict"));
        }
    }
    return issues;
}

}  // namespace slicer_core
