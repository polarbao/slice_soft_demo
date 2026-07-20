#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"

#include <cmath>
#include <map>
#include <stdexcept>
#include <tuple>

namespace slicer_core
{
namespace
{

using QuantizedVertex = std::tuple<long long, long long, long long>;

QuantizedVertex QuantizeVertex(const Vec3& vertex, const double epsilonMm)
{
    return {
        std::llround(vertex.x / epsilonMm),
        std::llround(vertex.y / epsilonMm),
        std::llround(vertex.z / epsilonMm),
    };
}

double TriangleAreaSquared(const Triangle& triangle)
{
    const double abX = triangle.b.x - triangle.a.x;
    const double abY = triangle.b.y - triangle.a.y;
    const double abZ = triangle.b.z - triangle.a.z;
    const double acX = triangle.c.x - triangle.a.x;
    const double acY = triangle.c.y - triangle.a.y;
    const double acZ = triangle.c.z - triangle.a.z;
    const double crossX = abY * acZ - abZ * acY;
    const double crossY = abZ * acX - abX * acZ;
    const double crossZ = abX * acY - abY * acX;
    return 0.25 * (crossX * crossX + crossY * crossY + crossZ * crossZ);
}

int AddVertex(
    TriangleMeshData& mesh,
    std::map<QuantizedVertex, int>& vertexByKey,
    const Vec3& vertex,
    const double epsilonMm)
{
    const QuantizedVertex key = QuantizeVertex(vertex, epsilonMm);
    const auto found = vertexByKey.find(key);
    if (found != vertexByKey.end())
    {
        return found->second;
    }
    const int index = static_cast<int>(mesh.vertices.size());
    mesh.vertices.push_back(vertex);
    vertexByKey.emplace(key, index);
    return index;
}

}  // namespace

AdaptedTriangleMesh AdaptSceneModelToTriangleMesh(
    const SceneModel& scene,
    const SceneModelTriangleMeshAdapterOptions& options)
{
    if (!(options.position_epsilon_mm > 0.0))
    {
        throw std::runtime_error("position epsilon must be positive");
    }

    AdaptedTriangleMesh result;
    result.mesh.source_name = scene.model_path.generic_string();
    result.mesh.bbox_mm = scene.bbox_mm;
    result.material_infos = scene.material_infos;
    result.topology.source_triangles = scene.triangles.size();

    std::map<QuantizedVertex, int> vertexByKey;
    for (std::size_t sourceIndex{0}; sourceIndex < scene.triangles.size(); ++sourceIndex)
    {
        const Triangle& triangle = scene.triangles.at(sourceIndex);
        if (TriangleAreaSquared(triangle) <= options.degenerate_area_epsilon_mm2)
        {
            ++result.topology.degenerate_triangles;
            result.rejected_degenerate_source_triangle_indices.push_back(sourceIndex);
            continue;
        }

        const int a = AddVertex(result.mesh, vertexByKey, triangle.a, options.position_epsilon_mm);
        const int b = AddVertex(result.mesh, vertexByKey, triangle.b, options.position_epsilon_mm);
        const int c = AddVertex(result.mesh, vertexByKey, triangle.c, options.position_epsilon_mm);
        if (a == b || b == c || a == c)
        {
            ++result.topology.degenerate_triangles;
            result.rejected_degenerate_source_triangle_indices.push_back(sourceIndex);
            continue;
        }
        result.mesh.triangles.push_back({a, b, c});

        SurfaceTriangleAttributes attributes;
        attributes.source_triangle_index = sourceIndex;
        if (sourceIndex < scene.triangle_textures.size())
        {
            const TriangleTextureInfo& source = scene.triangle_textures.at(sourceIndex);
            attributes.has_uv = source.has_uv;
            attributes.uv = source.uv;
            attributes.material_name = source.material_name;
        }
        result.triangle_attributes.push_back(attributes);
    }

    MeshTopologyReport analyzed = AnalyzeMeshTopology(result.mesh);
    analyzed.source_triangles = result.topology.source_triangles;
    analyzed.degenerate_triangles = result.topology.degenerate_triangles;
    result.topology = analyzed;

    if (options.normalize_orientation && result.topology.signed_volume_mm3 < 0.0)
    {
        for (std::size_t index{0}; index < result.mesh.triangles.size(); ++index)
        {
            std::swap(result.mesh.triangles.at(index).at(1), result.mesh.triangles.at(index).at(2));
            std::swap(result.triangle_attributes.at(index).uv.at(1), result.triangle_attributes.at(index).uv.at(2));
        }
        result.topology = AnalyzeMeshTopology(result.mesh);
        result.topology.source_triangles = scene.triangles.size();
        result.topology.degenerate_triangles = analyzed.degenerate_triangles;
        result.topology.orientation_flipped = true;
    }
    return result;
}

}  // namespace slicer_core
