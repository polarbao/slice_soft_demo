#pragma once

#include "slicer_core/model.h"

#include <string_view>

namespace slicer_core
{
/** @brief Exact, case-sensitive non-printing material reservation. */
bool IsFrameMaterial(std::string_view name) noexcept;

struct ImportedFrameGeometry
{
    std::vector<Vec3> vertices;
    Vec3 printableanchor;
    std::size_t trianglecount{0U};
    std::size_t sourcevertexcount{0U};
};

/** @brief Separate frame faces before orientation/topology; reject frame-only assets. */
ImportedFrameGeometry ExtractFrameGeometry(
    std::vector<Vec3>& vertices,
    std::vector<std::array<std::size_t, 3>>& faces,
    std::vector<TriangleTextureInfo>& textures);

/** @brief Apply the printable orientation to frame vertices and retain only XY bounds. */
ModelReport AttachFrameGeometry(ModelReport report, ImportedFrameGeometry frame);

/** @brief Extend XY only; frame height is never a printing constraint. */
void IncludeFramePointXY(BoundingBox& bounds, const Vec3& point);
}
