#pragma once

#include "slicer_core/geometry/MeshTopologyDiagnostics.h"
#include "slicer_core/scene/SceneModel.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief Source attributes retained for one accepted triangle.
 */
struct SurfaceTriangleAttributes
{
    std::size_t source_triangle_index{0};
    bool has_uv{false};
    std::array<TexCoord, 3> uv{};
    std::string material_name;
};

/**
 * @brief Scene model adapted for OpenVDB and surface texture transfer.
 */
struct AdaptedTriangleMesh
{
    TriangleMeshData mesh;
    std::vector<SurfaceTriangleAttributes> triangle_attributes;
    std::vector<std::size_t> rejected_degenerate_source_triangle_indices;
    std::vector<MaterialInfo> material_infos;
    MeshTopologyReport topology;
};

/**
 * @brief Options for adapting a scene model to an indexed triangle mesh.
 */
struct SceneModelTriangleMeshAdapterOptions
{
    double position_epsilon_mm{1.0e-6};
    double degenerate_area_epsilon_mm2{1.0e-12};
    bool normalize_orientation{true};
};

/**
 * @brief Convert the current SceneModel into an indexed mesh with source attributes.
 * @param scene Imported scene model.
 * @param options Adapter options.
 * @return Adapted mesh and topology diagnostics.
 */
AdaptedTriangleMesh AdaptSceneModelToTriangleMesh(
    const SceneModel& scene,
    const SceneModelTriangleMeshAdapterOptions& options = {});

}  // namespace slicer_core
