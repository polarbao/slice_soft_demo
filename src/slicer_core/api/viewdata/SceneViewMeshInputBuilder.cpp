#include "slicer_core/api/viewdata/SceneViewMeshInputBuilder.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>

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

bool IsFinite(const Vec3& point)
{
    return std::isfinite(point.x)
        && std::isfinite(point.y)
        && std::isfinite(point.z);
}

Vec3 TransformPoint(const Vec3& point, const Matrix4d& matrix)
{
    return {
        matrix.values.at(0U) * point.x
            + matrix.values.at(1U) * point.y
            + matrix.values.at(2U) * point.z
            + matrix.values.at(3U),
        matrix.values.at(4U) * point.x
            + matrix.values.at(5U) * point.y
            + matrix.values.at(6U) * point.z
            + matrix.values.at(7U),
        matrix.values.at(8U) * point.x
            + matrix.values.at(9U) * point.y
            + matrix.values.at(10U) * point.z
            + matrix.values.at(11U)};
}

bool IsDegenerate(const Triangle& triangle)
{
    const Vec3 ab{
        triangle.b.x - triangle.a.x,
        triangle.b.y - triangle.a.y,
        triangle.b.z - triangle.a.z};
    const Vec3 ac{
        triangle.c.x - triangle.a.x,
        triangle.c.y - triangle.a.y,
        triangle.c.z - triangle.a.z};
    const Vec3 cross{
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x};
    const double squaredLength = cross.x * cross.x
        + cross.y * cross.y
        + cross.z * cross.z;
    return !std::isfinite(squaredLength) || squaredLength <= 1.0e-24;
}

std::uint32_t FloatBits(const float value)
{
    return value == 0.0F
        ? 0U
        : std::bit_cast<std::uint32_t>(value);
}

struct TopologyVertexKey
{
    std::array<std::uint32_t, 5> values{};

    [[nodiscard]] bool operator<(
        const TopologyVertexKey& other) const noexcept
    {
        return values < other.values;
    }
};

TopologyVertexKey MakeTopologyVertexKey(
    const Vec3& point,
    const TexCoord& uv)
{
    const std::array<float, 5> values{
        static_cast<float>(point.x),
        static_cast<float>(point.y),
        static_cast<float>(point.z),
        static_cast<float>(uv.u),
        static_cast<float>(uv.v)};
    TopologyVertexKey key;
    std::transform(
        values.begin(),
        values.end(),
        key.values.begin(),
        FloatBits);
    return key;
}

std::uint32_t ResolveTopologyVertex(
    MeshSimplificationInput& input,
    std::map<TopologyVertexKey, std::uint32_t>& vertexIndices,
    const Vec3& point,
    const TexCoord& uv)
{
    const TopologyVertexKey key = MakeTopologyVertexKey(point, uv);
    const auto existing = vertexIndices.find(key);
    if (existing != vertexIndices.end())
    {
        return existing->second;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(
        input.positions.size() / 3U);
    input.positions.insert(
        input.positions.end(),
        {static_cast<float>(point.x),
         static_cast<float>(point.y),
         static_cast<float>(point.z)});
    input.texcoord0.insert(
        input.texcoord0.end(),
        {static_cast<float>(uv.u), static_cast<float>(uv.v)});
    vertexIndices.emplace(key, index);
    return index;
}

}  // namespace

ApiResult<MeshSimplificationInput> BuildViewMeshGroupInput(
    const SceneModel& model,
    const std::vector<std::size_t>& triangleIndices,
    const Matrix4d& worldMatrix,
    const MeshTransform meshTransform,
    const ICancelToken& cancelToken)
{
    MeshSimplificationInput input;
    input.positions.reserve(triangleIndices.size() * 9U);
    input.texcoord0.reserve(triangleIndices.size() * 6U);
    input.indices.reserve(triangleIndices.size() * 3U);
    std::map<TopologyVertexKey, std::uint32_t> vertexIndices;
    for (const std::size_t triangleIndex : triangleIndices)
    {
        if (cancelToken.IsCancelRequested())
        {
            return Failure<MeshSimplificationInput>(
                "PM-SLICER-CANCELLED-0070",
                "ViewData mesh generation was cancelled",
                model.model_path.generic_string());
        }
        Triangle triangle = model.triangles.at(triangleIndex);
        if (meshTransform == MeshTransform::World)
        {
            triangle.a = TransformPoint(triangle.a, worldMatrix);
            triangle.b = TransformPoint(triangle.b, worldMatrix);
            triangle.c = TransformPoint(triangle.c, worldMatrix);
        }
        if (!IsFinite(triangle.a)
            || !IsFinite(triangle.b)
            || !IsFinite(triangle.c))
        {
            return Failure<MeshSimplificationInput>(
                "PM-SLICER-INPUT-0002",
                "ViewData mesh contains non-finite geometry",
                std::to_string(triangleIndex));
        }
        if (IsDegenerate(triangle))
        {
            return Failure<MeshSimplificationInput>(
                "PM-SLICER-INPUT-0002",
                "ViewData mesh contains a degenerate triangle",
                std::to_string(triangleIndex));
        }

        const std::array<Vec3, 3> points{
            triangle.a, triangle.b, triangle.c};
        const TriangleTextureInfo* binding =
            model.triangle_textures.empty()
            ? nullptr
            : &model.triangle_textures.at(triangleIndex);
        for (std::size_t vertex{0U}; vertex < 3U; ++vertex)
        {
            const TexCoord uv = binding != nullptr && binding->has_uv
                ? binding->uv.at(vertex)
                : TexCoord{};
            input.indices.push_back(ResolveTopologyVertex(
                input,
                vertexIndices,
                points.at(vertex),
                uv));
        }
    }
    return ApiResult<MeshSimplificationInput>::Success(std::move(input));
}

Vec3 ReadSimplificationPoint(
    const MeshSimplificationInput& input,
    const std::uint32_t index)
{
    const std::size_t offset = static_cast<std::size_t>(index) * 3U;
    return {
        input.positions.at(offset),
        input.positions.at(offset + 1U),
        input.positions.at(offset + 2U)};
}

TexCoord ReadSimplificationUv(
    const MeshSimplificationInput& input,
    const std::uint32_t index)
{
    const std::size_t offset = static_cast<std::size_t>(index) * 2U;
    return {
        input.texcoord0.at(offset),
        input.texcoord0.at(offset + 1U)};
}

}  // namespace slicer_core::api::viewdata_detail
