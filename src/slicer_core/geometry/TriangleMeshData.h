#pragma once

#include "slicer_core/model.h"

#include <array>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Indexed triangle mesh used by experimental geometry kernel prototypes.
 */
struct TriangleMeshData
{
    std::vector<Vec3> vertices;
    std::vector<std::array<int, 3>> triangles;
    BoundingBox bbox_mm;
    std::string source_name;
};

/**
 * @brief Build a closed box fixture mesh.
 * @param widthMm Box width in millimeters.
 * @param depthMm Box depth in millimeters.
 * @param heightMm Box height in millimeters.
 * @return Closed triangle mesh with outward-facing triangles.
 */
TriangleMeshData MakeGeneratedBoxMesh(double widthMm, double depthMm, double heightMm);

/**
 * @brief Validate a triangle mesh before OpenVDB conversion.
 * @param mesh Mesh to validate.
 * @return Empty string when valid; otherwise an error message.
 */
std::string ValidateTriangleMesh(const TriangleMeshData& mesh);

}  // namespace slicer_core
