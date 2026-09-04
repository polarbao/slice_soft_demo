#include "CpuRasterResources.h"
#include "CpuRasterDecor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace cpu_raster_detail
{
namespace
{
struct Vector4 final
{
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float w{1.0F};
};

struct RasterVertex final
{
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float inverseW{1.0F};
    float uOverW{0.0F};
    float vOverW{0.0F};
};

using Matrix = std::array<float, 16>;

Matrix ReadMatrix(const float value[16])
{
    Matrix result{};
    std::copy(value, value + 16, result.begin());
    return result;
}

Matrix Multiply(const Matrix& a, const Matrix& b)
{
    Matrix result{};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            float value{0.0F};
            for (int item = 0; item < 4; ++item)
            {
                value += a.at(static_cast<std::size_t>(row * 4 + item))
                    * b.at(static_cast<std::size_t>(item * 4 + column));
            }
            result.at(static_cast<std::size_t>(row * 4 + column)) = value;
        }
    }
    return result;
}

Vector4 Transform(const Matrix& matrix, const Vector4& value)
{
    return {
        matrix.at(0U) * value.x + matrix.at(1U) * value.y
            + matrix.at(2U) * value.z + matrix.at(3U) * value.w,
        matrix.at(4U) * value.x + matrix.at(5U) * value.y
            + matrix.at(6U) * value.z + matrix.at(7U) * value.w,
        matrix.at(8U) * value.x + matrix.at(9U) * value.y
            + matrix.at(10U) * value.z + matrix.at(11U) * value.w,
        matrix.at(12U) * value.x + matrix.at(13U) * value.y
            + matrix.at(14U) * value.z + matrix.at(15U) * value.w};
}

bool Project(
    const Matrix& matrix,
    float x,
    float y,
    float z,
    float u,
    float v,
    std::uint32_t width,
    std::uint32_t height,
    RasterVertex* output)
{
    const Vector4 clip = Transform(matrix, {x, y, z, 1.0F});
    if (output == nullptr || !std::isfinite(clip.w)
        || std::abs(clip.w) < 1.0e-7F)
    {
        return false;
    }
    const float inverseW = 1.0F / clip.w;
    const float ndcX = clip.x * inverseW;
    const float ndcY = clip.y * inverseW;
    output->x = (ndcX * 0.5F + 0.5F) * static_cast<float>(width - 1U);
    output->y = (0.5F - ndcY * 0.5F) * static_cast<float>(height - 1U);
    output->z = clip.z * inverseW;
    output->inverseW = inverseW;
    output->uOverW = u * inverseW;
    output->vOverW = v * inverseW;
    return std::isfinite(output->x) && std::isfinite(output->y)
        && std::isfinite(output->z);
}

float Edge(
    const RasterVertex& a,
    const RasterVertex& b,
    float x,
    float y)
{
    return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
}

std::array<std::uint8_t, 4> Sample(
    const Resources& resources,
    const MaterialResource& material,
    float u,
    float v)
{
    float transformedU = material.uvTransform.at(0U) * u
        + material.uvTransform.at(1U) * v
        + material.uvTransform.at(2U);
    float transformedV = material.uvTransform.at(3U) * u
        + material.uvTransform.at(4U) * v
        + material.uvTransform.at(5U);
    std::array<float, 4> color = material.baseColor;
    const auto texture = resources.textures.find(material.textureIdentity);
    if (texture != resources.textures.end())
    {
        transformedU = std::clamp(transformedU, 0.0F, 1.0F);
        transformedV = std::clamp(transformedV, 0.0F, 1.0F);
        const TextureResource& image = texture->second;
        const std::uint32_t x = static_cast<std::uint32_t>(std::lround(
            transformedU * static_cast<float>(image.widthPx - 1U)));
        const float rowV = image.rowOriginTopLeft
            ? 1.0F - transformedV : transformedV;
        const std::uint32_t y = static_cast<std::uint32_t>(std::lround(
            rowV * static_cast<float>(image.heightPx - 1U)));
        const std::size_t offset =
            (static_cast<std::size_t>(y) * image.widthPx + x) * 4U;
        for (std::size_t channel = 0U; channel < 4U; ++channel)
        {
            color.at(channel) *= static_cast<float>(
                image.rgba8.at(offset + channel)) / 255.0F;
        }
    }
    std::array<std::uint8_t, 4> output{};
    for (std::size_t channel = 0U; channel < 4U; ++channel)
    {
        output.at(channel) = static_cast<std::uint8_t>(std::lround(
            std::clamp(color.at(channel), 0.0F, 1.0F) * 255.0F));
    }
    return output;
}

void BlendPixel(
    std::vector<std::uint8_t>* pixels,
    std::size_t offset,
    const std::array<std::uint8_t, 4>& source)
{
    const float alpha = static_cast<float>(source.at(3U)) / 255.0F;
    for (std::size_t channel = 0U; channel < 3U; ++channel)
    {
        const float value = static_cast<float>(source.at(channel)) * alpha
            + static_cast<float>(pixels->at(offset + channel))
                * (1.0F - alpha);
        pixels->at(offset + channel) = static_cast<std::uint8_t>(
            std::lround(value));
    }
    pixels->at(offset + 3U) = 255U;
}

/**
 * @brief 一个延后到透明 pass 才绘制的三角形。
 *
 * 透明片元必须在全部不透明几何之后、按视深由远到近绘制，
 * 因此它的投影结果要跨实例收集后统一排序。
 */
struct DeferredTriangle
{
    RasterVertex vertices[3];
    const MaterialResource* material{nullptr};
    bool selected{false};
    bool outOfBounds{false};
    float sortDepth{0.0F};
};

/// @brief 该材质是否走透明 pass。
[[nodiscard]] bool IsBlended(const MaterialResource& material)
{
    return material.alphaMode == "blend";
}

void RasterTriangle(
    const RasterVertex (&vertices)[3],
    const Resources& resources,
    const MaterialResource& material,
    bool selected,
    bool outOfBounds,
    bool writeDepth,
    std::vector<float>* depth,
    slicer::render::ImageOut* output)
{
    const float area = Edge(vertices[0], vertices[1], vertices[2].x, vertices[2].y);
    if (std::abs(area) < 1.0e-6F)
    {
        return;
    }
    const int minX = (std::max)(0, static_cast<int>(std::floor((std::min)({
        vertices[0].x, vertices[1].x, vertices[2].x}))));
    const int maxX = (std::min)(static_cast<int>(output->widthPx) - 1,
        static_cast<int>(std::ceil((std::max)({
            vertices[0].x, vertices[1].x, vertices[2].x}))));
    const int minY = (std::max)(0, static_cast<int>(std::floor((std::min)({
        vertices[0].y, vertices[1].y, vertices[2].y}))));
    const int maxY = (std::min)(static_cast<int>(output->heightPx) - 1,
        static_cast<int>(std::ceil((std::max)({
            vertices[0].y, vertices[1].y, vertices[2].y}))));
    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            float weights[3]{
                Edge(vertices[1], vertices[2], x + 0.5F, y + 0.5F) / area,
                Edge(vertices[2], vertices[0], x + 0.5F, y + 0.5F) / area,
                Edge(vertices[0], vertices[1], x + 0.5F, y + 0.5F) / area};
            if (weights[0] < 0.0F || weights[1] < 0.0F || weights[2] < 0.0F)
            {
                continue;
            }
            const float z = weights[0] * vertices[0].z
                + weights[1] * vertices[1].z + weights[2] * vertices[2].z;
            const std::size_t pixel = static_cast<std::size_t>(y)
                * output->widthPx + static_cast<std::size_t>(x);
            if (z >= depth->at(pixel))
            {
                continue;
            }
            const float reciprocal = weights[0] * vertices[0].inverseW
                + weights[1] * vertices[1].inverseW
                + weights[2] * vertices[2].inverseW;
            if (std::abs(reciprocal) < 1.0e-8F)
            {
                continue;
            }
            const float u = (weights[0] * vertices[0].uOverW
                + weights[1] * vertices[1].uOverW
                + weights[2] * vertices[2].uOverW) / reciprocal;
            const float v = (weights[0] * vertices[0].vOverW
                + weights[1] * vertices[1].vOverW
                + weights[2] * vertices[2].vOverW) / reciprocal;
            auto color = Sample(resources, material, u, v);
            if (material.alphaMode == "mask"
                && static_cast<float>(color[3]) / 255.0F < material.alphaCutoff)
            {
                continue;
            }
            if (outOfBounds)
            {
                color[0] = 255U;
                color[1] = static_cast<std::uint8_t>(color[1] / 3U);
                color[2] = static_cast<std::uint8_t>(color[2] / 3U);
            }
            else if (selected)
            {
                color[0] = static_cast<std::uint8_t>((color[0] + 242U) / 2U);
                color[1] = static_cast<std::uint8_t>((color[1] + 193U) / 2U);
            }
            BlendPixel(&output->rgba8, pixel * 4U, color);
// 透明片元只做深度测试、不写深度，否则近处的透明面会遮挡其后的几何。
            if (writeDepth)
            {
                depth->at(pixel) = z;
            }
        }
    }
}

bool DrawInstance(
    const Resources& resources,
    const slicer::render::FrameDesc& frame,
    const slicer::render::InstanceDraw& instance,
    std::vector<float>* depth,
    slicer::render::ImageOut* output,
    std::uint32_t* drawCallCount,
    std::vector<DeferredTriangle>* deferred)
{
    const auto mesh = resources.meshes.find(instance.meshIdentity);
    if (mesh == resources.meshes.end())
    {
        return false;
    }
    const Matrix world = ReadMatrix(instance.worldMatrix);
    const Matrix view = ReadMatrix(frame.camera.viewMatrix);
    const Matrix projection = ReadMatrix(frame.camera.projMatrix);
    const Matrix transform = Multiply(projection, Multiply(view, world));
    const std::size_t vertexCount = mesh->second.positions.size() / 3U;
    std::vector<RasterVertex> projected(vertexCount);
    std::vector<std::uint8_t> projectedValid(vertexCount, 0U);
    for (std::size_t vertex = 0U; vertex < vertexCount; ++vertex)
    {
        projectedValid[vertex] = Project(
            transform,
            mesh->second.positions[vertex * 3U],
            mesh->second.positions[vertex * 3U + 1U],
            mesh->second.positions[vertex * 3U + 2U],
            mesh->second.texcoord0[vertex * 2U],
            mesh->second.texcoord0[vertex * 2U + 1U],
            output->widthPx,
            output->heightPx,
            &projected[vertex]) ? 1U : 0U;
    }
    for (const slicer::render::SubmeshDesc& submesh : mesh->second.submeshes)
    {
        const auto material = resources.materials.find(MaterialKey(
            instance.appearanceIdentity, submesh.materialId));
        if (material == resources.materials.end())
        {
            return false;
        }
        const std::size_t end = static_cast<std::size_t>(submesh.firstIndex)
            + static_cast<std::size_t>(submesh.indexCount);
        if (submesh.indexCount % 3U != 0U
            || end > mesh->second.indices.size())
        {
            return false;
        }
        ++(*drawCallCount);
        for (std::size_t item = submesh.firstIndex;
             item < end;
             item += 3U)
        {
            RasterVertex triangle[3];
            bool visible{true};
            for (std::size_t vertex = 0U; vertex < 3U; ++vertex)
            {
                const std::uint32_t index = mesh->second.indices[item + vertex];
                visible = visible && projectedValid[index] != 0U;
                triangle[vertex] = projected[index];
            }
            if (!visible)
            {
                continue;
            }
            if (IsBlended(material->second))
            {
                DeferredTriangle entry;
                entry.vertices[0] = triangle[0];
                entry.vertices[1] = triangle[1];
                entry.vertices[2] = triangle[2];
                entry.material = &material->second;
                entry.selected = instance.selected;
                entry.outOfBounds = instance.outOfBuildVolume;
                entry.sortDepth = (triangle[0].z + triangle[1].z + triangle[2].z)
                    / 3.0F;
                deferred->push_back(entry);
                continue;
            }
            RasterTriangle(
                triangle, resources, material->second,
                instance.selected, instance.outOfBuildVolume,
                true, depth, output);
        }
    }
    return true;
}
}

std::string MaterialKey(
    const std::string& appearanceIdentity,
    const std::string& materialId)
{
    return appearanceIdentity.empty() || materialId.empty()
        ? std::string{} : appearanceIdentity + '\n' + materialId;
}

bool Rasterize(
    const Resources& resources,
    const slicer::render::FrameDesc& frame,
    slicer::render::ImageOut* output,
    std::uint32_t* drawCallCount,
    std::string* errorCode)
{
    if (output == nullptr || drawCallCount == nullptr || errorCode == nullptr
        || frame.viewMode != slicer::render::ViewMode::ThreeD
        || frame.viewportWidthPx == 0U || frame.viewportHeightPx == 0U)
    {
        return false;
    }
    output->widthPx = frame.viewportWidthPx;
    output->heightPx = frame.viewportHeightPx;
    output->rgba8.assign(
        static_cast<std::size_t>(output->widthPx) * output->heightPx * 4U,
        255U);
    for (std::size_t offset = 0U; offset < output->rgba8.size(); offset += 4U)
    {
        output->rgba8[offset] = 43U;
        output->rgba8[offset + 1U] = 45U;
        output->rgba8[offset + 2U] = 49U;
    }
    std::vector<float> depth(
        static_cast<std::size_t>(output->widthPx) * output->heightPx,
        std::numeric_limits<float>::infinity());
    *drawCallCount = 0U;
    errorCode->clear();
    const Matrix viewProjection = Multiply(
        ReadMatrix(frame.camera.projMatrix),
        ReadMatrix(frame.camera.viewMatrix));
    cpu_raster_detail::DrawSceneDecor(frame, output);
// 第一趟只画不透明几何，绘制顺序与深度写入行为与引入透明 pass 之前完全一致；
// 透明三角形被跨实例收集下来，留给第二趟处理。
    std::vector<DeferredTriangle> deferred;
    for (const slicer::render::InstanceDraw& instance : frame.instances)
    {
        if (!DrawInstance(
                resources, frame, instance, &depth, output, drawCallCount,
                &deferred))
        {
            *errorCode = "HOST-RENDER-RESOURCE-MISSING";
            return false;
        }
    }
// 第二趟按视深由远到近绘制透明几何，且不写深度缓冲。
// 场景内没有 blend 材质时 deferred 为空，本段不产生任何像素写入。
    std::stable_sort(
        deferred.begin(),
        deferred.end(),
        [](const DeferredTriangle& left, const DeferredTriangle& right) {
            return left.sortDepth > right.sortDepth;
        });
    for (const DeferredTriangle& entry : deferred)
    {
        RasterTriangle(
            entry.vertices, resources, *entry.material,
            entry.selected, entry.outOfBounds, false, &depth, output);
    }
    return true;
}

slicer::render::PickResult Pick(
    const Resources& resources,
    const slicer::render::FrameDesc& frame,
    const int xPx,
    const int yPx)
{
    (void)resources;
    const Matrix viewProjection = Multiply(
        ReadMatrix(frame.camera.projMatrix),
        ReadMatrix(frame.camera.viewMatrix));
    for (auto item = frame.instances.rbegin(); item != frame.instances.rend(); ++item)
    {
        const Matrix transform = Multiply(
            viewProjection,
            ReadMatrix(item->worldMatrix));
        RasterVertex corners[4];
        const bool projected = Project(transform, item->localBoundsMm[0],
            item->localBoundsMm[1], 0.0F, 0.0F, 0.0F,
            frame.viewportWidthPx, frame.viewportHeightPx, &corners[0])
            && Project(transform, item->localBoundsMm[2],
                item->localBoundsMm[1], 0.0F, 0.0F, 0.0F,
                frame.viewportWidthPx, frame.viewportHeightPx, &corners[1])
            && Project(transform, item->localBoundsMm[2],
                item->localBoundsMm[3], 0.0F, 0.0F, 0.0F,
                frame.viewportWidthPx, frame.viewportHeightPx, &corners[2])
            && Project(transform, item->localBoundsMm[0],
                item->localBoundsMm[3], 0.0F, 0.0F, 0.0F,
                frame.viewportWidthPx, frame.viewportHeightPx, &corners[3]);
        if (!projected)
        {
            continue;
        }
        float minX = corners[0].x;
        float maxX = corners[0].x;
        float minY = corners[0].y;
        float maxY = corners[0].y;
        for (const RasterVertex& corner : corners)
        {
            minX = (std::min)(minX, corner.x);
            maxX = (std::max)(maxX, corner.x);
            minY = (std::min)(minY, corner.y);
            maxY = (std::max)(maxY, corner.y);
        }
        if (xPx >= minX && xPx <= maxX && yPx >= minY && yPx <= maxY)
        {
            slicer::render::PickResult result;
            result.hit = true;
            result.instanceId = item->instanceId;
            return result;
        }
    }
    return {};
}

}  // namespace cpu_raster_detail
