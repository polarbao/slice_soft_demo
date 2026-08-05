#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"

#include "slicer_core/api/viewdata/SceneViewIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <map>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace slicer_core::api::viewdata_detail
{
namespace
{

template <class T>
ApiResult<T> Failure(
    const std::string_view code,
    const std::string_view message,
    const std::string& detail)
{
    return ApiResult<T>::Failure(
        {std::string(code), std::string(message), detail});
}

bool IsFinite(const TexCoord& uv)
{
    return std::isfinite(uv.u) && std::isfinite(uv.v);
}

std::string SourceMaterialName(
    const SceneModel& model,
    const std::size_t triangleIndex)
{
    if (model.triangle_textures.empty())
    {
        return {};
    }
    return model.triangle_textures.at(triangleIndex).material_name;
}

ApiResult<ViewTexture> ResolveTexture(
    const MaterialInfo& source,
    const ISceneViewTextureSource& textureSource)
{
    if (source.diffuse_texture_path.empty() || !source.texture_exists)
    {
        return Failure<ViewTexture>(
            "PM-SLICER-INPUT-0001",
            "used ViewData material declares a missing texture",
            source.name);
    }
    ApiResult<TextureImage> loaded = textureSource.Load(
        source.diffuse_texture_path);
    if (!loaded.IsOk())
    {
        return Failure<ViewTexture>(
            loaded.Error()->code,
            loaded.Error()->message,
            source.name + ": " + loaded.Error()->detail);
    }

    ViewTexture texture;
    texture.width_px = loaded.Value()->width;
    texture.height_px = loaded.Value()->height;
    texture.rgba8 = loaded.Value()->rgba;
    const std::size_t expectedBytes =
        static_cast<std::size_t>(texture.width_px)
        * static_cast<std::size_t>(texture.height_px) * 4U;
    if (texture.width_px <= 0 || texture.height_px <= 0
        || texture.rgba8.size() != expectedBytes)
    {
        return Failure<ViewTexture>(
            "PM-SLICER-INPUT-0002",
            "used ViewData texture has invalid decoded RGBA8 data",
            source.name);
    }
    texture.texture_identity = ComputeTextureIdentity(texture);
    texture.texture_id = texture.texture_identity;
    return ApiResult<ViewTexture>::Success(std::move(texture));
}

}  // namespace

ApiResult<ResolvedViewAppearance> ResolveViewAppearance(
    const SceneModel& model,
    const ISceneViewTextureSource& textureSource,
    const ICancelToken& cancelToken) noexcept
{
    try
    {
        if (model.triangles.empty())
        {
            return Failure<ResolvedViewAppearance>(
                "PM-SLICER-INPUT-0002",
                "ViewData model geometry is empty",
                model.model_path.generic_string());
        }
        if (!model.triangle_textures.empty()
            && model.triangle_textures.size() != model.triangles.size())
        {
            return Failure<ResolvedViewAppearance>(
                "PM-SLICER-INPUT-0002",
                "triangle texture bindings do not match model geometry",
                model.model_path.generic_string());
        }

        std::map<std::string, const MaterialInfo*> sourceMaterials;
        for (const MaterialInfo& material : model.material_infos)
        {
            if (material.name.empty()
                || !sourceMaterials.emplace(material.name, &material).second)
            {
                return Failure<ResolvedViewAppearance>(
                    "PM-SLICER-INPUT-0002",
                    "ViewData material table contains an invalid identity",
                    material.name);
            }
        }

        std::vector<std::string> usedMaterialNames;
        std::set<std::string> seenMaterials;
        for (std::size_t index{0U}; index < model.triangles.size(); ++index)
        {
            if (cancelToken.IsCancelRequested())
            {
                return Failure<ResolvedViewAppearance>(
                    "PM-SLICER-CANCELLED-0070",
                    "ViewData appearance resolution was cancelled",
                    model.model_path.generic_string());
            }
            const std::string name = SourceMaterialName(model, index);
            if (seenMaterials.emplace(name).second)
            {
                usedMaterialNames.push_back(name);
            }
        }

        ResolvedViewAppearance resolved;
        resolved.materials.reserve(usedMaterialNames.size());
        std::map<std::string, std::size_t> textureIndices;
        for (const std::string& sourceName : usedMaterialNames)
        {
            ResolvedViewMaterial resolvedMaterial;
            resolvedMaterial.source_name = sourceName;
            resolvedMaterial.material.material_id = "material-"
                + ComputeSha256(
                    sourceName.empty() ? "__default__" : sourceName);

            const MaterialInfo* source{nullptr};
            if (!sourceName.empty())
            {
                const auto found = sourceMaterials.find(sourceName);
                if (found == sourceMaterials.end())
                {
                    return Failure<ResolvedViewAppearance>(
                        "PM-SLICER-INPUT-0002",
                        "triangle references an unknown ViewData material",
                        sourceName);
                }
                source = found->second;
                // Some production OBJ exporters write black Kd beside map_Kd.
                const bool hasNeutralizedTexturedBlack =
                    source->has_texture
                    && source->diffuse_rgb.at(0U) == 0U
                    && source->diffuse_rgb.at(1U) == 0U
                    && source->diffuse_rgb.at(2U) == 0U;
                if (source->has_diffuse && !hasNeutralizedTexturedBlack)
                {
                    for (std::size_t channel{0U}; channel < 3U; ++channel)
                    {
                        resolvedMaterial.material.base_color.at(channel) =
                            static_cast<float>(
                                source->diffuse_rgb.at(channel)) / 255.0F;
                    }
                }
            }

            if (source != nullptr && source->has_texture)
            {
                ApiResult<ViewTexture> texture = ResolveTexture(
                    *source,
                    textureSource);
                if (!texture.IsOk())
                {
                    return Failure<ResolvedViewAppearance>(
                        texture.Error()->code,
                        texture.Error()->message,
                        texture.Error()->detail);
                }
                resolvedMaterial.has_texture = true;
                const auto existingTexture = textureIndices.find(
                    texture.Value()->texture_identity);
                if (existingTexture == textureIndices.end())
                {
                    resolvedMaterial.texture_index =
                        resolved.appearance.textures.size();
                    resolvedMaterial.material.texture_id =
                        texture.Value()->texture_id;
                    textureIndices.emplace(
                        texture.Value()->texture_identity,
                        resolvedMaterial.texture_index);
                    resolved.appearance.textures.push_back(
                        std::move(*texture.Value()));
                }
                else
                {
                    resolvedMaterial.texture_index = existingTexture->second;
                    resolvedMaterial.material.texture_id =
                        resolved.appearance.textures.at(
                            existingTexture->second).texture_id;
                }
                resolved.has_texture = true;
            }

            const std::size_t materialIndex = resolved.materials.size();
            resolved.material_indices.emplace(sourceName, materialIndex);
            resolved.appearance.materials.push_back(
                resolvedMaterial.material);
            resolved.materials.push_back(std::move(resolvedMaterial));
        }

        for (std::size_t index{0U}; index < model.triangles.size(); ++index)
        {
            const auto materialResult = ResolveTriangleMaterialIndex(
                model,
                resolved,
                index);
            if (!materialResult.IsOk())
            {
                return Failure<ResolvedViewAppearance>(
                    materialResult.Error()->code,
                    materialResult.Error()->message,
                    materialResult.Error()->detail);
            }
            if (model.triangle_textures.empty())
            {
                continue;
            }
            const TriangleTextureInfo& binding =
                model.triangle_textures.at(index);
            const ResolvedViewMaterial& material =
                resolved.materials.at(*materialResult.Value());
            if (material.has_texture && !binding.has_uv)
            {
                return Failure<ResolvedViewAppearance>(
                    "PM-SLICER-INPUT-0002",
                    "textured ViewData triangle has no UV coordinates",
                    std::to_string(index));
            }
            if (binding.has_uv
                && (!IsFinite(binding.uv.at(0U))
                    || !IsFinite(binding.uv.at(1U))
                    || !IsFinite(binding.uv.at(2U))))
            {
                return Failure<ResolvedViewAppearance>(
                    "PM-SLICER-INPUT-0002",
                    "ViewData triangle contains non-finite UV coordinates",
                    std::to_string(index));
            }
        }

        resolved.appearance.appearance_identity =
            ComputeAppearanceIdentity(resolved.appearance);
        return ApiResult<ResolvedViewAppearance>::Success(
            std::move(resolved));
    }
    catch (const std::exception& error)
    {
        return Failure<ResolvedViewAppearance>(
            "PM-SLICER-INTERNAL-0099",
            "failed to resolve ViewData appearance",
            error.what());
    }
    catch (...)
    {
        return Failure<ResolvedViewAppearance>(
            "PM-SLICER-INTERNAL-0099",
            "failed to resolve ViewData appearance",
            "unknown exception");
    }
}

ApiResult<std::size_t> ResolveTriangleMaterialIndex(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    const std::size_t triangleIndex) noexcept
{
    if (triangleIndex >= model.triangles.size())
    {
        return Failure<std::size_t>(
            "PM-SLICER-INPUT-0002",
            "ViewData triangle index is out of range",
            std::to_string(triangleIndex));
    }
    const std::string name = SourceMaterialName(model, triangleIndex);
    const auto material = appearance.material_indices.find(name);
    if (material == appearance.material_indices.end())
    {
        return Failure<std::size_t>(
            "PM-SLICER-INPUT-0002",
            "ViewData triangle material binding is unresolved",
            name);
    }
    return ApiResult<std::size_t>::Success(material->second);
}

}  // namespace slicer_core::api::viewdata_detail
