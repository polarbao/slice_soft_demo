// 贴图取色与材质角色映射的实现（F-09 第 5 步从 slicer.cpp 搬出）。
//
// 只移位、不改行为：函数体逐字节搬过来，判据是字节级基线逐产物全等。
// 25 个私有助手留在匿名命名空间；公开函数定义顺序不受约束——
// 头文件在最前面已声明全部 10 个。

#include "slicer_core/materials/SliceMaterialTexture.h"

#include "slicer_core/material/RetainedMaterialLayerComposer.h"
#include "slicer_core/materials/texture_application/TextureWhiteCarrierPolicy.h"
#include "slicer_core/materials/volume/MaterialLayerRgbComposer.h"
#include "slicer_core/materials/volume/MaterialVolumePlan.h"
#include "slicer_core/materials/volume/MaterialVolumeWhiteCarrier.h"
#include "slicer_core/system/Utf8Path.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace slicer_core::materials {

namespace {

const RuntimeMaterialTexture* find_runtime_material(
    const TextureRuntime& runtime,
    const std::string& material_name) {
    const auto found = runtime.materials.find(material_name);
    if (found == runtime.materials.end()) {
        return nullptr;
    }
    return &found->second;
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
    if (texture_columns != nullptr && pixel_index < texture_columns->size() && texture_columns->at(pixel_index).hasColor) {
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

std::array<std::uint8_t, 3> fallback_texture_rgb(
    const SliceConfig& config,
    const RuntimeMaterialTexture* material) {
    if (material != nullptr && material->material.has_diffuse) {
        return material->material.diffuse_rgb;
    }
    return config.texture.fallback_rgb;
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
        if (roleColumn == nullptr || !roleColumn->hasRole)
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
            if (roleColumn != nullptr && roleColumn->hasRole && roleColumn->role == MaterialRole::Rgb
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

TextureColumnColor resolve_texture_color(
    const SliceConfig& config,
    const std::vector<TextureColumnColor>* texture_columns,
    const std::size_t pixel_index) {
    TextureColumnColor color;
    color.hasColor = true;
    color.rgb = config.texture.fallback_rgb;
    color.usedFallback = true;
    if (texture_columns != nullptr && pixel_index < texture_columns->size()
        && texture_columns->at(pixel_index).hasColor) {
        color = texture_columns->at(pixel_index);
    }
    return color;
}

void update_texture_report_for_color(const TextureColumnColor& color, TextureReportData* texture_report) {
    if (texture_report == nullptr) {
        return;
    }
    if (color.sampledTexture) {
        ++texture_report->sampled_pixels;
    }
    if (color.usedFallback) {
        ++texture_report->fallback_pixels;
    }
    if (color.uvOutOfRange) {
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
    if (!range.hasModel || range.upperLayer < range.lowerLayer) {
        return false;
    }
    const int first_top_layer = std::max(range.lowerLayer, range.upperLayer - top_layers + 1);
    return layer_index >= first_top_layer && layer_index <= range.upperLayer;
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
    if (!role_column.hasRole) {
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

/**
 * @brief 判断本像素的材质所有者是否就是该 XY 列的顶面材质。
 *
 * MATOPQ-RGB M1：逐列顶面贴图（build_relief_texture_columns）对「被顶面遮住的
 * 下层材质」是错误的颜色来源——它会把上层材质的色写到下层体积上（tm2-5 的
 * nail-L2 段因此整段变成 trans 的 Kd 250）。owner 与顶面一致时该来源是对的，
 * 不一致时才需让位给 MATVOL 的逐材质颜色。
 *
 * 任一侧信息缺失一律返回 true，即退回既有行为：本卡是取色修正，不是校验加严，
 * 不得新增 fail 路径、不得改变任何资产的可切性。单材质与顶面材质像素由此
 * 结构性零漂移——它们走的是与修订前完全相同的代码路径。
 */
[[nodiscard]] bool TextureColumnMatchesOwner(
    const std::vector<std::uint32_t>* topMaterialIndex,
    const std::vector<std::uint32_t>* owner,
    const std::size_t pixelIndex) {
    if (topMaterialIndex == nullptr || owner == nullptr
        || pixelIndex >= topMaterialIndex->size()
        || pixelIndex >= owner->size()) {
        return true;
    }
    const std::uint32_t ownerIndex = owner->at(pixelIndex);
    const std::uint32_t topIndex = topMaterialIndex->at(pixelIndex);
    if (ownerIndex == kNoMaterialOwner || topIndex == kNoMaterialOwner) {
        return true;
    }
    return ownerIndex == topIndex;
}

}  // namespace

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
            {"diffuseRgb", reports::rgb_to_json(material.diffuse_rgb)},
            {"hasTexture", material.has_texture},
            {"texturePath", slicer_core::PathToUtf8(material.diffuse_texture_path)},
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
        role_column.hasRole = true;
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
            runtime.report.texture_files.push_back(slicer_core::PathToUtf8(material.diffuse_texture_path));
            if (material.texture_source == "3mf_internal") {
                runtime.report.source = "3mf_internal";
            }
            if (!material.texture_exists) {
                ++runtime.report.missing_textures;
                runtime.report.warnings.push_back("missing texture: " + slicer_core::PathToUtf8(material.diffuse_texture_path));
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
                    runtime.report.warnings.push_back("texture decode failed: " + slicer_core::PathToUtf8(material.diffuse_texture_path));
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
            {"diffuseRgb", reports::rgb_to_json(material.diffuse_rgb)},
            {"hasTexture", material.has_texture},
            {"texturePath", slicer_core::PathToUtf8(material.diffuse_texture_path)},
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
        color.hasColor = true;

        if (texture_info.has_uv && material != nullptr && material->loaded) {
            const double u = column.top_barycentric.at(0) * texture_info.uv.at(0).u
                + column.top_barycentric.at(1) * texture_info.uv.at(1).u
                + column.top_barycentric.at(2) * texture_info.uv.at(2).u;
            const double v = column.top_barycentric.at(0) * texture_info.uv.at(0).v
                + column.top_barycentric.at(1) * texture_info.uv.at(1).v
                + column.top_barycentric.at(2) * texture_info.uv.at(2).v;
            bool uv_out_of_range{false};
            color.rgb = sample_texture_rgb(material->image, u, v, sample_options, uv_out_of_range);
            color.sampledTexture = true;
            color.uvOutOfRange = uv_out_of_range;
        } else {
            color.rgb = fallback_texture_rgb(config, material);
            color.usedFallback = true;
        }
    }
    return result;
}

/**
 * @brief 按 (材质, 列) 采样各材质自己的贴图（MATOPQ-RGB M2）。
 *
 * 与 build_relief_texture_columns 的区别只在顶面来源：后者用全列最高面，
 * 故被遮住的下层材质取不到自己的 UV；本函数用该材质自己的最高面。
 *
 * 无贴图 / 无 UV 的 (材质, 列) 保持 has_color=false，由调用方回退到该材质的 Kd
 * （即 M1 行为），再回退到 MV-05 既有 fallbackPolicy。此处不静默填色，
 * 以免把「该材质没有贴图」和「采样得到某个颜色」混为一谈。
 */
std::vector<TextureColumnColor> build_per_material_texture_columns(
    const SliceConfig& config,
    const ModelReport& model_report,
    const ReliefPerMaterialTopSurface& perMaterialTop,
    TextureRuntime& runtime) {
    std::vector<TextureColumnColor> result;
    if (!config.texture.enabled || perMaterialTop.Empty()) {
        return result;
    }
    const TextureSampleOptions sample_options{
        config.texture.sampler,
        config.texture.uv_address_mode,
        config.texture.flip_v};
    result.assign(perMaterialTop.topTriangle.size(), TextureColumnColor{});
    for (std::size_t slot{0}; slot < perMaterialTop.topTriangle.size(); ++slot) {
        const int triangleIndex = perMaterialTop.topTriangle.at(slot);
        if (triangleIndex < 0
            || triangleIndex >= static_cast<int>(model_report.triangle_textures.size())) {
            continue;
        }
        const TriangleTextureInfo& texture_info =
            model_report.triangle_textures.at(static_cast<std::size_t>(triangleIndex));
        const RuntimeMaterialTexture* material =
            find_runtime_material(runtime, texture_info.material_name);
        if (!texture_info.has_uv || material == nullptr || !material->loaded) {
            continue;
        }
        const std::array<double, 3>& bary = perMaterialTop.topBarycentric.at(slot);
        const double u = bary.at(0) * texture_info.uv.at(0).u
            + bary.at(1) * texture_info.uv.at(1).u
            + bary.at(2) * texture_info.uv.at(2).u;
        const double v = bary.at(0) * texture_info.uv.at(0).v
            + bary.at(1) * texture_info.uv.at(1).v
            + bary.at(2) * texture_info.uv.at(2).v;
        bool uv_out_of_range{false};
        TextureColumnColor& color = result.at(slot);
        color.rgb = sample_texture_rgb(
            material->image, u, v, sample_options, uv_out_of_range);
        color.hasColor = true;
        color.sampledTexture = true;
        color.uvOutOfRange = uv_out_of_range;
    }
    return result;
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
                rgb_role = !role_column.hasRole || role_column.role == MaterialRole::Rgb;
            }
            if (rgb_role && ShouldApplyTextureToLayer(config, column_ranges, pixel_index, layer_index))
            {
                mask.at(pixel_index) = 1;
            }
        }
    }
    return mask;
}

/**
 * @brief 合成单层六通道输出。
 *
 * MF-03X4：`pixels` 由调用方持有并跨层复用。原实现每层新建 w*h*6 字节
 * （10um 大幅面场景 44.2 MB），每层都要重新触碰新页 —— 这是按幅面计的固定开销，
 * 与模型占多少列无关。改为 assign 后容量足够即不重新分配，只做填充。
 */
void compose_layer(
    std::vector<std::uint8_t>& pixels,
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
    const std::vector<std::uint8_t>* material_volume_rgb,
    const std::vector<std::uint8_t>* material_volume_varnish_mask,
    // M1：逐列顶面材质下标与本层材质所有者，仅用于判断逐列顶面贴图对本像素
    // 是否为正确来源；两者任一为空即退回既有取色路径。
    const std::vector<std::uint32_t>* top_material_index,
    const std::vector<std::uint32_t>* material_volume_owner,
    // M2：按 (材质, 列) 预采好的各材质自身贴图色；为空则 MATVOL 分支沿用 Kd（M1 行为）。
    const std::vector<TextureColumnColor>* per_material_texture_columns,
    // MF-03X2b 稀疏遍历：非空时只遍历这些列，其余列保持预填的 background.value。
    // 这是【精确等价】而非近似：下方 else 链没有末尾 else，故 model / outer_varnish /
    // support 三者皆零的列在原实现里也不写任何字节。调用方须保证该列表覆盖三者的并集。
    const std::vector<std::uint32_t>* active_columns,
    const int layer_index,
    TextureReportData* texture_report,
    MaterialPolicyReportData* material_policy_report,
    MaterialClosureSemanticLayerInput* materialClosureInput,
    LayerSemanticStats& semantic_stats,
    int& model_pixels,
    int& support_pixels) {
    const std::size_t composeByteCount =
        static_cast<std::size_t>(grid.width_px) * grid.height_px * rgbwsv_channel_count;
    if (active_columns == nullptr || pixels.size() != composeByteCount)
    {
        pixels.assign(composeByteCount, config.background.value);
    }
    else
    {
        // MF-03X4：稀疏遍历下只有活动列会被写，表外的列自上次填充后恒为背景值，
        // 故每层只需重置活动列 —— 省掉每层一次 w*h*6（10um 场景 44.2 MB）的整幅面写。
        for (const std::uint32_t column : *active_columns)
        {
            std::fill_n(
                pixels.begin()
                    + static_cast<std::ptrdiff_t>(
                        static_cast<std::size_t>(column) * rgbwsv_channel_count),
                rgbwsv_channel_count,
                config.background.value);
        }
    }
    const bool whiteCarrierEnabled =
        config.texture.unprintable_white_policy == "white_underbase";

    const std::size_t composeColumnCount =
        static_cast<std::size_t>(grid.width_px) * grid.height_px;
    const std::size_t composeIterationCount =
        active_columns != nullptr ? active_columns->size() : composeColumnCount;
    for (std::size_t composeIndex{0}; composeIndex < composeIterationCount; ++composeIndex) {
        {
            const std::size_t pixel_index = active_columns != nullptr
                ? static_cast<std::size_t>(active_columns->at(composeIndex))
                : composeIndex;
            const std::size_t base = pixel_index * rgbwsv_channel_count;
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
                    if (role_column.hasRole && role_column.role == MaterialRole::Rgb && config.texture.enabled
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
                        if (role_column.hasRole && role_column.role == MaterialRole::Rgb && apply_texture) {
                            const TextureColumnColor color =
                                resolve_texture_color(config, texture_columns, pixel_index);
                            update_texture_report_for_color(color, texture_report);
                            texture_surface_pixel = true;
                        } else if (config.model_fill.enabled) {
                            model_fill_pixel = !role_column.hasRole
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
                           && TextureColumnMatchesOwner(
                               top_material_index, material_volume_owner, pixel_index)
                           && ShouldApplyTextureToLayer(config, column_ranges, pixel_index, layer_index)) {
                    const TextureColumnColor color = resolve_texture_color(config, texture_columns, pixel_index);
                    pixels.at(base + 0U) = color.rgb.at(0);
                    pixels.at(base + 1U) = color.rgb.at(1);
                    pixels.at(base + 2U) = color.rgb.at(2);
                    update_texture_report_for_color(color, texture_report);
                    if (whiteCarrierEnabled
                        && ApplyUnprintableWhiteCarrier(
                            config.texture.unprintable_white_ink_threshold,
                            config.texture.unprintable_white_value,
                            color.rgb,
                            pixels.at(base + 3U)))
                    {
                        ++semantic_stats.unprintable_white_carrier_pixels;
                    }
                    counted_model_pixel = true;
                    texture_surface_pixel = true;
                    const ModelFillMaterial fill_material = ResolveModelFillMaterial(config, nullptr);
                    if (ShouldApplyModelFill(config, texture_surface_pixel, fill_material)
                        && WriteModelFillPixel(pixels, base, config, nullptr)) {
                        model_fill_pixel = true;
                    }
                } else if (config.material_volume_policy.enabled
                           && material_volume_rgb != nullptr
                           && pixel_index * 3U + 2U < material_volume_rgb->size()) {
                    // MATVOL：逐层材质所有权已在层循环内解算为紧凑 RGB，此处按列取用。
                    // 与旧路径的本质区别是【同一 XY 列在不同层可以属于不同材质】，
                    // 而 relief_heightfield 每列只有一个 top_triangle_index，结构上做不到。
                    //
                    // M2：先取该 owner 材质自己的贴图色（用它自己的顶面 UV 采样，
                    // 故被上层遮住的下层材质也能采到）；该材质无贴图或无 UV 时
                    // 回退到 Kd 表，即 M1 行为。
                    bool wrotePerMaterialTexture{false};
                    if (per_material_texture_columns != nullptr
                        && material_volume_owner != nullptr
                        && pixel_index < material_volume_owner->size()) {
                        const std::uint32_t owner =
                            material_volume_owner->at(pixel_index);
                        const std::size_t columnCount{
                            static_cast<std::size_t>(grid.width_px)
                            * static_cast<std::size_t>(grid.height_px)};
                        if (owner != kNoMaterialOwner && columnCount > 0U) {
                            const std::size_t slot =
                                static_cast<std::size_t>(owner) * columnCount
                                + pixel_index;
                            if (slot < per_material_texture_columns->size()
                                && per_material_texture_columns->at(slot).hasColor) {
                                const TextureColumnColor& color =
                                    per_material_texture_columns->at(slot);
                                pixels.at(base + 0U) = color.rgb.at(0);
                                pixels.at(base + 1U) = color.rgb.at(1);
                                pixels.at(base + 2U) = color.rgb.at(2);
                                update_texture_report_for_color(color, texture_report);
                                wrotePerMaterialTexture = true;
                            }
                        }
                    }
                    if (!wrotePerMaterialTexture) {
                        pixels.at(base + 0U) = material_volume_rgb->at(pixel_index * 3U + 0U);
                        pixels.at(base + 1U) = material_volume_rgb->at(pixel_index * 3U + 1U);
                        pixels.at(base + 2U) = material_volume_rgb->at(pixel_index * 3U + 2U);
                    }
                    counted_model_pixel = true;
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

    // MATVOL 按需补白（MV-08C）。必须在最终 RGB 之后：补白判据逐像素读 RGB，
    // 若在 RGB 定稿前施加，判的是中间值。此处是 compose_layer 内最后一个
    // 仍会改动模型像素 RGB 的位置之后，故为正确插入点。
    //
    // 布局差异是本段的要害：补白函数要求【紧凑】单通道 W（步长 1），
    // 而 pixels 是六通道交错（W 在 base+3、步长 6），不能直接取 span，
    // 必须用暂存缓冲并按列散射回写。暂存缓冲以调用方现值播种，
    // 保证未命中的像素 W 保持原值不变。
    if (config.material_volume_policy.enabled && material_volume_rgb != nullptr
        && whiteCarrierEnabled)
    {
        const std::size_t columnCount = model_mask.size();
        if (material_volume_rgb->size() == columnCount * 3U)
        {
            std::vector<std::uint8_t> whiteScratch(columnCount, 0U);
            for (std::size_t column{0}; column < columnCount; ++column)
            {
                whiteScratch[column] = pixels.at(column * 6U + 3U);
            }
            MaterialVolumeWhiteCarrierRequest carrier;
            carrier.whiteUnderbaseEnabled = true;
            carrier.inkThreshold = config.texture.unprintable_white_ink_threshold;
            carrier.whiteValue = config.texture.unprintable_white_value;
            MaterialVolumeWhiteCarrierStats carrierStats;
            // 补白必须观察【本层实际写入的】RGB，而不是 material_volume_rgb 这张
            // 逐材质 Kd 表。M2 的逐材质贴图采样会把贴图色写进 pixels，其纯白区为
            // (255,255,255)，而对应材质的 Kd 未必是全 255（如 nail 为 250,250,255）：
            // 按 Kd 表判定就不会补 W，该像素遂 ownership 非空却六通道全 255，
            // 触发 package 契约 layers.ownership 不闭合（PM-SLICER-CONTRACT-0060）。
            // 从 pixels 反读即让本策略回到其注释所声明的「观察最终 RGB」语义。
            std::vector<std::uint8_t> effectiveRgb(columnCount * 3U, 0U);
            for (std::size_t column{0}; column < columnCount; ++column)
            {
                effectiveRgb[column * 3U + 0U] = pixels.at(column * 6U + 0U);
                effectiveRgb[column * 3U + 1U] = pixels.at(column * 6U + 1U);
                effectiveRgb[column * 3U + 2U] = pixels.at(column * 6U + 2U);
            }
            ApplyMaterialVolumeWhiteCarrierLayer(
                carrier, effectiveRgb, model_mask, whiteScratch, carrierStats);
            for (std::size_t column{0}; column < columnCount; ++column)
            {
                pixels.at(column * 6U + 3U) = whiteScratch[column];
            }
            semantic_stats.unprintable_white_carrier_pixels +=
                carrierStats.unprintableWhiteCarrierPixels;
        }
    }

    // MO-04：不透明度判为光油的材质，其体积改写 V 通道而非 RGB。
    //
    // 放在白墨载体【之后】是必须的：补白策略要观察最终 RGB 才能决定是否补 W，
    // 若先把光油区 RGB 清空，补白会把它误看成空区。
    //
    // 光油区【不需要白墨底】（用户 2026-09-01 回签），故本段同时清 W：
    // 补白载体先按最终 RGB 判过一轮，光油区那部分 W 属于误加，必须在此撤除。
    if (config.material_volume_policy.opacity_varnish.enabled
        && material_volume_varnish_mask != nullptr
        && material_volume_varnish_mask->size() == model_mask.size())
    {
        for (std::size_t column{0}; column < model_mask.size(); ++column)
        {
            if (model_mask[column] == 0U
                || material_volume_varnish_mask->at(column) == 0U)
            {
                continue;
            }
            const std::size_t base = column * rgbwsv_channel_count;
            pixels.at(base + 0U) = config.background.value;
            pixels.at(base + 1U) = config.background.value;
            pixels.at(base + 2U) = config.background.value;
            pixels.at(base + 3U) = config.background.value;
// black_is_print：V 必须写 0（满墨）才是"打印光油"。
// 不可用 config.material.varnish_value——它默认 255，即 emptyValue，
// 写进去等于"此处不打光油"。与既有 MaterialRole::Varnish 落盘口径保持一致。
            pixels.at(base + 5U) = 0U;
            ++semantic_stats.opacity_varnish_pixels;
        }
    }

}

}  // namespace slicer_core::materials
