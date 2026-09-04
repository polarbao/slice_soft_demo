#include "slicer_core/model/MtlMaterialParser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace slicer_core::model_detail {

namespace {

/// @brief 两个不透明度读数仍视为一致的最大差值。
constexpr double kOpacityAgreementEpsilon{1.0e-3};

std::string TrimWhitespace(const std::string& input)
{
    const auto first = std::find_if_not(input.begin(), input.end(), [](const unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(input.rbegin(), input.rend(), [](const unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (first >= last)
    {
        return {};
    }
    return std::string(first, last);
}

std::uint8_t KdComponentToU8(const double value)
{
    const double clamped = std::clamp(value, 0.0, 1.0);
    return static_cast<std::uint8_t>(std::round(clamped * 255.0));
}

std::filesystem::path ResolveTexturePath(
    const std::filesystem::path& raw_path,
    const std::filesystem::path& mtl_dir,
    const std::filesystem::path& obj_dir)
{
    if (raw_path.is_absolute())
    {
        return raw_path.lexically_normal();
    }
    const std::filesystem::path from_mtl = (mtl_dir / raw_path).lexically_normal();
    if (std::filesystem::exists(from_mtl))
    {
        return from_mtl;
    }
    return (obj_dir / raw_path).lexically_normal();
}

MtlMaterialLineResult ApplyDiffuseColor(const std::string& arguments, MaterialInfo* material)
{
    std::istringstream stream{arguments};
    double r{0.0};
    double g{0.0};
    double b{0.0};
    if (!(stream >> r >> g >> b))
    {
        return {};
    }
    material->diffuse_rgb = {KdComponentToU8(r), KdComponentToU8(g), KdComponentToU8(b)};
    material->has_diffuse = true;
    return MtlMaterialLineResult{true, false};
}

MtlMaterialLineResult ApplyDiffuseTexture(
    const std::string& arguments,
    const MtlMaterialContext& context,
    MaterialInfo* material)
{
    const std::string texture_name = TrimWhitespace(arguments);
    if (texture_name.empty())
    {
        return {};
    }
    material->diffuse_texture_path =
        ResolveTexturePath(texture_name, context.mtl_dir, context.obj_dir);
    material->has_texture = true;
    material->texture_exists = std::filesystem::exists(material->diffuse_texture_path);
    return MtlMaterialLineResult{true, false};
}

MtlMaterialLineResult ApplyOpacity(
    const std::string& arguments,
    const bool isTransmission,
    MaterialInfo* material)
{
    std::istringstream stream{arguments};
    double value{0.0};
    if (!(stream >> value) || !std::isfinite(value))
    {
        return {};
    }
    const double opacity = std::clamp(isTransmission ? 1.0 - value : value, 0.0, 1.0);
    MtlMaterialLineResult result{true, false};
    if (material->has_opacity
        && std::abs(opacity - material->opacity) > kOpacityAgreementEpsilon)
    {
        result.opacity_conflict = true;
    }
    material->opacity = opacity;
    material->has_opacity = true;
    return result;
}

}  // namespace

std::string TrimMaterialName(const std::string& arguments)
{
    return TrimWhitespace(arguments);
}

MtlMaterialLineResult ApplyMtlMaterialLine(
    const std::string& token,
    const std::string& arguments,
    const MtlMaterialContext& context,
    MaterialInfo* const material)
{
    if (material == nullptr)
    {
        return {};
    }
    if (token == "Kd")
    {
        return ApplyDiffuseColor(arguments, material);
    }
    if (token == "map_Kd")
    {
        return ApplyDiffuseTexture(arguments, context, material);
    }
    if (token == "d" || token == "Tr")
    {
        return ApplyOpacity(arguments, token == "Tr", material);
    }
    return {};
}

}  // namespace slicer_core::model_detail
