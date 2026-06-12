#include "slicer_core/geometry/TriangleMeshData.h"

#include <algorithm>

namespace slicer_core
{

TriangleMeshData MakeGeneratedBoxMesh(const double widthMm, const double depthMm, const double heightMm)
{
    TriangleMeshData mesh;
    mesh.source_name = "generated-box";
    mesh.vertices = {
        {0.0, 0.0, 0.0},
        {widthMm, 0.0, 0.0},
        {widthMm, depthMm, 0.0},
        {0.0, depthMm, 0.0},
        {0.0, 0.0, heightMm},
        {widthMm, 0.0, heightMm},
        {widthMm, depthMm, heightMm},
        {0.0, depthMm, heightMm},
    };
    mesh.triangles = {
        {0, 2, 1},
        {0, 3, 2},
        {4, 5, 6},
        {4, 6, 7},
        {0, 1, 5},
        {0, 5, 4},
        {1, 2, 6},
        {1, 6, 5},
        {2, 3, 7},
        {2, 7, 6},
        {3, 0, 4},
        {3, 4, 7},
    };
    mesh.bbox_mm.min = {0.0, 0.0, 0.0};
    mesh.bbox_mm.max = {widthMm, depthMm, heightMm};
    return mesh;
}

std::string ValidateTriangleMesh(const TriangleMeshData& mesh)
{
    if (mesh.vertices.empty())
    {
        return "mesh has no vertices";
    }
    if (mesh.triangles.empty())
    {
        return "mesh has no triangles";
    }

    const int vertexCount = static_cast<int>(mesh.vertices.size());
    for (const std::array<int, 3>& triangle : mesh.triangles)
    {
        for (const int index : triangle)
        {
            if (index < 0 || index >= vertexCount)
            {
                return "mesh has invalid triangle index";
            }
        }
        if (triangle.at(0) == triangle.at(1) || triangle.at(1) == triangle.at(2) || triangle.at(0) == triangle.at(2))
        {
            return "mesh has degenerate triangle index";
        }
    }

    if (!(mesh.bbox_mm.max.x > mesh.bbox_mm.min.x)
        || !(mesh.bbox_mm.max.y > mesh.bbox_mm.min.y)
        || !(mesh.bbox_mm.max.z > mesh.bbox_mm.min.z))
    {
        return "mesh bbox is invalid";
    }
    return {};
}

}  // namespace slicer_core
