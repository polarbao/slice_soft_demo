#pragma once

#include "slicer_core/scene/ModelInstance.h"
#include "slicer_core/scene/SceneModel.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace slicer_core
{

/**
 * @brief Immutable transformed geometry and its audit metadata.
 */
struct TransformedModelGeometry
{
    std::vector<Triangle> triangles;
    std::vector<TriangleTextureInfo> triangletextures;
    BoundingBox bboxmm;
    Vec3 pivotmm;
    double landingoffsetzmm{0.0};
    double landingreferencezmm{0.0};
    std::size_t landingignoredcomponentcount{0U};
    int determinantsign{1};
    bool mirrored{false};
    std::uint64_t transformrevision{0U};
};

/**
 * @brief Result of adapting a source scene model through an instance transform.
 */
struct TransformedModelResult
{
    TransformedModelGeometry geometry;
    std::optional<ModelTransformError> error;

    /**
     * @brief Report whether transformed geometry was produced.
     * @return True when no validation or source error is present.
     */
    bool IsValid() const;
};

/**
 * @brief Adapt source geometry through a Stage 13 instance transform.
 * @param source Source-transformed scene model; it is never mutated.
 * @param instance Instance transform and stable identity.
 * @return Transformed triangles, texture attributes, bounds, and audit data.
 */
TransformedModelResult AdaptTransformedModel(
    const SceneModel& source,
    const ModelInstance& instance);

}  // namespace slicer_core
