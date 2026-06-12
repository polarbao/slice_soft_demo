#pragma once

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"

#include <cstddef>

namespace slicer_core
{

/**
 * @brief Read-only source attributes for real-model texture transfer.
 */
class SurfaceAttributeMap
{
public:
    /**
     * @brief Construct an attribute map from an adapted mesh.
     * @param adaptedMesh Adapted mesh whose lifetime must exceed this map.
     */
    explicit SurfaceAttributeMap(const AdaptedTriangleMesh& adaptedMesh);

    /**
     * @brief Get attributes for an accepted triangle.
     * @param triangleIndex Accepted triangle index.
     * @return Triangle attributes.
     */
    const SurfaceTriangleAttributes& TriangleAttributes(std::size_t triangleIndex) const;

    /**
     * @brief Find a material by stable material name.
     * @param materialName Material name.
     * @return Material pointer or nullptr.
     */
    const MaterialInfo* FindMaterial(const std::string& materialName) const;

private:
    const AdaptedTriangleMesh& m_adaptedMesh;
};

}  // namespace slicer_core
