#include "slicer_core/materials/texture_application/SurfaceAttributeMap.h"

#include <algorithm>

namespace slicer_core
{

SurfaceAttributeMap::SurfaceAttributeMap(const AdaptedTriangleMesh& adaptedMesh) : m_adaptedMesh(adaptedMesh)
{
}

const SurfaceTriangleAttributes& SurfaceAttributeMap::TriangleAttributes(const std::size_t triangleIndex) const
{
    return m_adaptedMesh.triangle_attributes.at(triangleIndex);
}

const MaterialInfo* SurfaceAttributeMap::FindMaterial(const std::string& materialName) const
{
    const auto found = std::find_if(
        m_adaptedMesh.material_infos.begin(),
        m_adaptedMesh.material_infos.end(),
        [&](const MaterialInfo& material)
        {
            return material.name == materialName;
        });
    return found == m_adaptedMesh.material_infos.end() ? nullptr : &(*found);
}

}  // namespace slicer_core
