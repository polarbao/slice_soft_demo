#include "slicer_core/scene/SceneModel.h"

namespace slicer_core
{

SceneSummary SummarizeScene(const SceneModel& scene)
{
    return SceneSummary{
        scene.vertex_count,
        scene.triangle_count,
        scene.material_infos.size(),
        scene.bbox_mm,
    };
}

}  // namespace slicer_core
