#include "slicer_core/scene/SceneViewGeometry.h"

#include "slicer_core/geometry/TransformedModelAdapter.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace slicer_core
{
namespace
{

constexpr int kSurfacePreviewMaximumDimension{768};
constexpr double kRasterEpsilon{1.0e-9};

SceneViewGeometryError MakeError(
    const SceneViewGeometryErrorCode code,
    const SceneViewGeometryRequest& request,
    const std::string_view field,
    const std::string_view message)
{
    SceneViewGeometryError error;
    error.code = code;
    error.sceneid = request.sceneid;
    error.modelid = request.instance.modelid;
    error.instanceid = request.instance.instanceid;
    error.field = field;
    error.message = message;
    return error;
}

bool IsFinite(const SceneViewPoint& point)
{
    return std::isfinite(point.xmm) && std::isfinite(point.ymm);
}

bool IsFinite(const TexCoord& uv)
{
    return std::isfinite(uv.u) && std::isfinite(uv.v);
}

std::array<std::uint8_t, 3> ResolveSurfaceColor(
    const SceneViewTriangle& triangle,
    const std::array<double, 3>& weights,
    const std::vector<SceneViewMaterialAppearance>& appearances,
    const std::map<int, TextureImage>& textures,
    const TextureSampleOptions& textureOptions,
    bool& textured)
{
    textured = false;
    if (triangle.materialindex < 0
        || static_cast<std::size_t>(triangle.materialindex)
            >= appearances.size())
    {
        return {66U, 144U, 139U};
    }

    const SceneViewMaterialAppearance& appearance =
        appearances.at(
            static_cast<std::size_t>(triangle.materialindex));
    const auto texture = textures.find(triangle.materialindex);
    if (triangle.hasuv && texture != textures.end())
    {
        const double u =
            triangle.uv.at(0U).u * weights.at(0U)
            + triangle.uv.at(1U).u * weights.at(1U)
            + triangle.uv.at(2U).u * weights.at(2U);
        const double v =
            triangle.uv.at(0U).v * weights.at(0U)
            + triangle.uv.at(1U).v * weights.at(1U)
            + triangle.uv.at(2U).v * weights.at(2U);
        bool uvOutOfRange{false};
        textured = true;
        return sample_texture_rgb(
            texture->second,
            u,
            v,
            textureOptions,
            uvOutOfRange);
    }
    return appearance.hasdiffuse
        ? appearance.diffusergb
        : std::array<std::uint8_t, 3>{66U, 144U, 139U};
}

SceneViewSurfacePreview BuildSurfacePreview(
    const SceneViewGeometry& geometry,
    const TextureSampleOptions& textureOptions)
{
    SceneViewSurfacePreview preview;
    const double worldWidth =
        geometry.worldboundsmm.max.xmm
        - geometry.worldboundsmm.min.xmm;
    const double worldHeight =
        geometry.worldboundsmm.max.ymm
        - geometry.worldboundsmm.min.ymm;
    if (!(worldWidth > 0.0) || !(worldHeight > 0.0))
    {
        return preview;
    }

    if (worldWidth >= worldHeight)
    {
        preview.width = kSurfacePreviewMaximumDimension;
        preview.height = std::max(
            2,
            static_cast<int>(std::lround(
                kSurfacePreviewMaximumDimension
                * worldHeight / worldWidth)));
    }
    else
    {
        preview.height = kSurfacePreviewMaximumDimension;
        preview.width = std::max(
            2,
            static_cast<int>(std::lround(
                kSurfacePreviewMaximumDimension
                * worldWidth / worldHeight)));
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(preview.width)
        * static_cast<std::size_t>(preview.height);
    preview.rgba.assign(pixelCount * 4U, 0U);
    std::vector<double> depth(
        pixelCount,
        std::numeric_limits<double>::lowest());
    std::vector<std::uint8_t> pixelSource(pixelCount, 0U);

    std::map<int, TextureImage> textures;
    for (std::size_t index = 0U;
         index < geometry.materialappearances.size();
         ++index)
    {
        const SceneViewMaterialAppearance& appearance =
            geometry.materialappearances.at(index);
        if (!appearance.hastexture
            || !appearance.textureexists
            || appearance.texturepath.empty())
        {
            continue;
        }
        try
        {
            textures.emplace(
                static_cast<int>(index),
                load_texture_image(appearance.texturepath));
        }
        catch (const std::exception&)
        {
            // Display preview falls back to the material diffuse color.
        }
    }

    for (const SceneViewTriangle& triangle : geometry.triangles)
    {
        const std::array<double, 3> x{
            (triangle.a.xmm - geometry.worldboundsmm.min.xmm)
                / worldWidth * (preview.width - 1),
            (triangle.b.xmm - geometry.worldboundsmm.min.xmm)
                / worldWidth * (preview.width - 1),
            (triangle.c.xmm - geometry.worldboundsmm.min.xmm)
                / worldWidth * (preview.width - 1),
        };
        const std::array<double, 3> y{
            (geometry.worldboundsmm.max.ymm - triangle.a.ymm)
                / worldHeight * (preview.height - 1),
            (geometry.worldboundsmm.max.ymm - triangle.b.ymm)
                / worldHeight * (preview.height - 1),
            (geometry.worldboundsmm.max.ymm - triangle.c.ymm)
                / worldHeight * (preview.height - 1),
        };
        const double denominator =
            (y.at(1U) - y.at(2U))
                * (x.at(0U) - x.at(2U))
            + (x.at(2U) - x.at(1U))
                * (y.at(0U) - y.at(2U));
        if (std::abs(denominator) <= kRasterEpsilon)
        {
            continue;
        }

        const int minX = std::clamp(
            static_cast<int>(std::floor(
                std::min({x.at(0U), x.at(1U), x.at(2U)}))),
            0,
            preview.width - 1);
        const int maxX = std::clamp(
            static_cast<int>(std::ceil(
                std::max({x.at(0U), x.at(1U), x.at(2U)}))),
            0,
            preview.width - 1);
        const int minY = std::clamp(
            static_cast<int>(std::floor(
                std::min({y.at(0U), y.at(1U), y.at(2U)}))),
            0,
            preview.height - 1);
        const int maxY = std::clamp(
            static_cast<int>(std::ceil(
                std::max({y.at(0U), y.at(1U), y.at(2U)}))),
            0,
            preview.height - 1);

        for (int pixelY = minY; pixelY <= maxY; ++pixelY)
        {
            for (int pixelX = minX; pixelX <= maxX; ++pixelX)
            {
                const double weightA =
                    ((y.at(1U) - y.at(2U))
                         * (pixelX - x.at(2U))
                     + (x.at(2U) - x.at(1U))
                         * (pixelY - y.at(2U)))
                    / denominator;
                const double weightB =
                    ((y.at(2U) - y.at(0U))
                         * (pixelX - x.at(2U))
                     + (x.at(0U) - x.at(2U))
                         * (pixelY - y.at(2U)))
                    / denominator;
                const double weightC =
                    1.0 - weightA - weightB;
                if (weightA < -kRasterEpsilon
                    || weightB < -kRasterEpsilon
                    || weightC < -kRasterEpsilon)
                {
                    continue;
                }

                const double z =
                    triangle.zmm.at(0U) * weightA
                    + triangle.zmm.at(1U) * weightB
                    + triangle.zmm.at(2U) * weightC;
                const std::size_t pixelIndex =
                    static_cast<std::size_t>(pixelY)
                        * static_cast<std::size_t>(preview.width)
                    + static_cast<std::size_t>(pixelX);
                if (z + kRasterEpsilon < depth.at(pixelIndex))
                {
                    continue;
                }

                bool textured{false};
                const auto color = ResolveSurfaceColor(
                    triangle,
                    {weightA, weightB, weightC},
                    geometry.materialappearances,
                    textures,
                    textureOptions,
                    textured);
                const std::size_t byteOffset = pixelIndex * 4U;
                preview.rgba.at(byteOffset + 0U) = color.at(0U);
                preview.rgba.at(byteOffset + 1U) = color.at(1U);
                preview.rgba.at(byteOffset + 2U) = color.at(2U);
                preview.rgba.at(byteOffset + 3U) = 255U;
                depth.at(pixelIndex) = z;
                pixelSource.at(pixelIndex) = textured ? 2U : 1U;
            }
        }
    }

    preview.texturedpixelcount = static_cast<std::size_t>(
        std::count(pixelSource.begin(), pixelSource.end(), 2U));
    preview.materialpixelcount = static_cast<std::size_t>(
        std::count(pixelSource.begin(), pixelSource.end(), 1U));
    std::string hashPayload =
        std::to_string(preview.width)
        + "x" + std::to_string(preview.height) + "|";
    hashPayload.append(
        reinterpret_cast<const char*>(preview.rgba.data()),
        preview.rgba.size());
    preview.contenthash = ComputeSha256(hashPayload);
    return preview;
}

std::string ComputeGeometryHash(const SceneViewGeometry& geometry)
{
    std::ostringstream payload;
    payload.imbue(std::locale::classic());
    payload << std::setprecision(17);
    payload << "slicesoft.scene_view_geometry.13a.2\n";
    payload << geometry.sceneid.size() << ':' << geometry.sceneid << '\n';
    payload << geometry.modelid.size() << ':' << geometry.modelid << '\n';
    payload << geometry.instanceid.size() << ':' << geometry.instanceid
            << '\n';
    payload << geometry.scenerevision << '\n';
    payload << geometry.transformrevision << '\n';
    payload << geometry.triangles.size() << '\n';
    payload << geometry.texturedtrianglecount << '\n';
    payload << geometry.materialcount << '\n';
    payload << geometry.materialappearances.size() << '\n';
    for (const SceneViewMaterialAppearance& appearance :
         geometry.materialappearances)
    {
        payload << appearance.name.size() << ':' << appearance.name
                << ';' << static_cast<int>(appearance.diffusergb.at(0U))
                << ',' << static_cast<int>(appearance.diffusergb.at(1U))
                << ',' << static_cast<int>(appearance.diffusergb.at(2U))
                << ';' << appearance.hasdiffuse
                << ';' << appearance.hastexture
                << ';' << appearance.textureexists << '\n';
    }
    for (const SceneViewTriangle& triangle : geometry.triangles)
    {
        payload << triangle.a.xmm << ',' << triangle.a.ymm << ';'
                << triangle.b.xmm << ',' << triangle.b.ymm << ';'
                << triangle.c.xmm << ',' << triangle.c.ymm << ';'
                << triangle.zmm.at(0U) << ','
                << triangle.zmm.at(1U) << ','
                << triangle.zmm.at(2U) << ';'
                << triangle.hasuv << ';'
                << triangle.materialindex << ';';
        for (const TexCoord& uv : triangle.uv)
        {
            payload << uv.u << ',' << uv.v << ';';
        }
        payload << '\n';
    }
    return ComputeSha256(payload.str());
}

}  // namespace

bool SceneViewSurfacePreview::IsValid() const
{
    return width > 0
        && height > 0
        && rgba.size()
            == static_cast<std::size_t>(width)
                * static_cast<std::size_t>(height)
                * 4U;
}

bool SceneViewGeometryResult::IsValid() const
{
    return !error.has_value();
}

std::string_view SceneViewGeometryErrorCodeName(
    const SceneViewGeometryErrorCode code)
{
    switch (code)
    {
    case SceneViewGeometryErrorCode::None:
        return "NONE";
    case SceneViewGeometryErrorCode::SceneIdEmpty:
        return "SCENE_VIEW_SCENE_ID_EMPTY";
    case SceneViewGeometryErrorCode::ModelIdEmpty:
        return "SCENE_VIEW_MODEL_ID_EMPTY";
    case SceneViewGeometryErrorCode::InstanceIdEmpty:
        return "SCENE_VIEW_INSTANCE_ID_EMPTY";
    case SceneViewGeometryErrorCode::RevisionStale:
        return "SCENE_VIEW_REVISION_STALE";
    case SceneViewGeometryErrorCode::SourceGeometryInvalid:
        return "SCENE_VIEW_SOURCE_GEOMETRY_INVALID";
    case SceneViewGeometryErrorCode::GeometryNonFinite:
        return "SCENE_VIEW_GEOMETRY_NON_FINITE";
    case SceneViewGeometryErrorCode::TransformInvalid:
        return "SCENE_VIEW_TRANSFORM_INVALID";
    }
    return "SCENE_VIEW_UNKNOWN";
}

void RefreshSceneViewGeometryHash(SceneViewGeometry& geometry)
{
    geometry.geometryhash = ComputeGeometryHash(geometry);
}

SceneViewGeometryResult BuildSceneViewGeometry(
    const SceneModel& source,
    const SceneViewGeometryRequest& request)
{
    if (request.sceneid.empty())
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::SceneIdEmpty,
                request,
                "sceneid",
                "scene view requires a stable scene id")};
    }
    if (request.instance.modelid.empty())
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::ModelIdEmpty,
                request,
                "modelid",
                "scene view requires a stable model id")};
    }
    if (request.instance.instanceid.empty())
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::InstanceIdEmpty,
                request,
                "instanceid",
                "scene view requires a stable instance id")};
    }
    if ((request.expectedscenerevision.has_value()
         && request.expectedscenerevision.value()
             != request.scenerevision)
        || (request.expectedtransformrevision.has_value()
            && request.expectedtransformrevision.value()
                != request.instance.transformrevision))
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::RevisionStale,
                request,
                "revision",
                "scene or transform revision changed before projection")};
    }
    if (source.triangles.empty())
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::SourceGeometryInvalid,
                request,
                "source.triangles",
                "source model geometry must not be empty")};
    }

    const ModelTransformHashResult transformHash =
        ComputeModelTransformHash(
            request.instance.transform,
            request.instance.sourcetransformidentity,
            request.instance.instanceid,
            request.instance.modelid);
    if (!transformHash.IsValid())
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::TransformInvalid,
                request,
                transformHash.error->field,
                transformHash.error->message)};
    }

    TransformedModelResult transformed =
        AdaptTransformedModel(source, request.instance);
    if (!transformed.IsValid())
    {
        const SceneViewGeometryErrorCode code =
            transformed.error->code == ModelTransformErrorCode::NonFinite
            ? SceneViewGeometryErrorCode::GeometryNonFinite
            : SceneViewGeometryErrorCode::TransformInvalid;
        return {
            {},
            MakeError(
                code,
                request,
                transformed.error->field,
                transformed.error->message)};
    }

    SceneViewGeometry geometry;
    geometry.sceneid = request.sceneid;
    geometry.modelid = request.instance.modelid;
    geometry.instanceid = request.instance.instanceid;
    geometry.scenerevision = request.scenerevision;
    geometry.transformrevision = request.instance.transformrevision;
    geometry.sourcebboxmm = source.bbox_mm;
    geometry.effectivebboxmm = transformed.geometry.bboxmm;
    geometry.visible = request.instance.visible;
    geometry.locked = request.instance.locked;
    geometry.admissionstatus = request.admissionstatus;
    geometry.sourcetrianglecount = source.triangles.size();
    geometry.texturedtrianglecount = static_cast<std::size_t>(
        std::count_if(
            source.triangle_textures.begin(),
            source.triangle_textures.end(),
            [](const TriangleTextureInfo& texture)
            {
                return texture.has_uv;
            }));
    geometry.materialcount = source.material_infos.size();
    geometry.hastexturecoordinates =
        geometry.texturedtrianglecount > 0U;
    geometry.materialappearances.reserve(
        source.material_infos.size());
    std::unordered_map<std::string, int> materialIndices;
    materialIndices.reserve(source.material_infos.size());
    for (const MaterialInfo& material : source.material_infos)
    {
        const int materialIndex = static_cast<int>(
            geometry.materialappearances.size());
        SceneViewMaterialAppearance appearance;
        appearance.name = material.name;
        appearance.diffusergb = material.diffuse_rgb;
        appearance.texturepath = material.diffuse_texture_path;
        appearance.hasdiffuse = material.has_diffuse;
        appearance.hastexture = material.has_texture;
        appearance.textureexists = material.texture_exists;
        geometry.materialappearances.push_back(
            std::move(appearance));
        materialIndices.emplace(material.name, materialIndex);
    }
    geometry.transformhash = transformHash.hash;
    geometry.worldboundsmm = {
        {transformed.geometry.bboxmm.min.x,
         transformed.geometry.bboxmm.min.y},
        {transformed.geometry.bboxmm.max.x,
         transformed.geometry.bboxmm.max.y},
    };
    if (!IsFinite(geometry.worldboundsmm.min)
        || !IsFinite(geometry.worldboundsmm.max))
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::GeometryNonFinite,
                request,
                "worldboundsmm",
                "projected model bounds must be finite")};
    }
    if (geometry.worldboundsmm.max.xmm
            <= geometry.worldboundsmm.min.xmm
        || geometry.worldboundsmm.max.ymm
            <= geometry.worldboundsmm.min.ymm)
    {
        return {
            {},
            MakeError(
                SceneViewGeometryErrorCode::SourceGeometryInvalid,
                request,
                "worldboundsmm",
                "projected model bounds must have positive width and height")};
    }

    geometry.triangles.reserve(transformed.geometry.triangles.size());
    for (std::size_t index = 0U;
         index < transformed.geometry.triangles.size();
         ++index)
    {
        const Triangle& triangle =
            transformed.geometry.triangles.at(index);
        SceneViewTriangle projected{
            {triangle.a.x, triangle.a.y},
            {triangle.b.x, triangle.b.y},
            {triangle.c.x, triangle.c.y},
            {triangle.a.z, triangle.b.z, triangle.c.z},
        };
        if (index < transformed.geometry.triangletextures.size())
        {
            const TriangleTextureInfo& texture =
                transformed.geometry.triangletextures.at(index);
            if (texture.has_uv
                && (!IsFinite(texture.uv.at(0U))
                    || !IsFinite(texture.uv.at(1U))
                    || !IsFinite(texture.uv.at(2U))))
            {
                return {
                    {},
                    MakeError(
                        SceneViewGeometryErrorCode::GeometryNonFinite,
                        request,
                        "triangle_textures.uv",
                        "scene view texture coordinates must be finite")};
            }
            projected.hasuv = texture.has_uv;
            projected.uv = texture.uv;
            const auto material =
                materialIndices.find(texture.material_name);
            if (material != materialIndices.end())
            {
                projected.materialindex = material->second;
            }
        }
        if (!IsFinite(projected.a)
            || !IsFinite(projected.b)
            || !IsFinite(projected.c))
        {
            return {
                {},
                MakeError(
                    SceneViewGeometryErrorCode::GeometryNonFinite,
                    request,
                    "triangles",
                    "projected triangle coordinates must be finite")};
        }
        geometry.triangles.push_back(projected);
    }
    std::stable_sort(
        geometry.triangles.begin(),
        geometry.triangles.end(),
        [](const SceneViewTriangle& left, const SceneViewTriangle& right)
        {
            const double leftDepth =
                left.zmm.at(0U)
                + left.zmm.at(1U)
                + left.zmm.at(2U);
            const double rightDepth =
                right.zmm.at(0U)
                + right.zmm.at(1U)
                + right.zmm.at(2U);
            return leftDepth < rightDepth;
        });
    geometry.surfacepreview = BuildSurfacePreview(
        geometry,
        request.textureoptions);
    RefreshSceneViewGeometryHash(geometry);
    return {std::move(geometry), std::nullopt};
}

}  // namespace slicer_core
