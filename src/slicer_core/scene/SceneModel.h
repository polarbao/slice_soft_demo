#pragma once

#include "slicer_core/model.h"

#include <cstddef>

namespace slicer_core
{

using SceneModel = ModelReport;
using SceneTriangle = Triangle;
using SceneMaterialInfo = MaterialInfo;
using SceneTriangleTextureInfo = TriangleTextureInfo;

/**
 * @brief Lightweight summary for a scene model.
 */
struct SceneSummary
{
    std::size_t vertex_count{0};
    std::size_t triangle_count{0};
    std::size_t material_count{0};
    BoundingBox bbox_mm;
};

/**
 * @brief Build a lightweight summary from a scene model.
 * @param scene Scene model to summarize.
 * @return Summary containing counts and bounding box information.
 */
SceneSummary SummarizeScene(const SceneModel& scene);

}  // namespace slicer_core
