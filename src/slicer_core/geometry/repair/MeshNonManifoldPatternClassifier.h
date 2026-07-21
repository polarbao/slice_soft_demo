#pragma once

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

namespace slicer_core
{

/**
 * @brief Classify every indexed non-manifold edge without mutating the mesh.
 * @param mesh Final transformed mesh with one attribute record per triangle.
 * @return Stable edge-level patterns and aggregate fan-split feasibility.
 * @throws MeshRepairError when mesh attributes or indices are invalid.
 */
MeshNonManifoldAnalysis ClassifyMeshNonManifoldPatterns(
    const AdaptedTriangleMesh& mesh);

}  // namespace slicer_core
