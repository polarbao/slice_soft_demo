#pragma once

#include "slicer_core/scene/MultiModelScene.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace slicer_core
{

/**
 * @brief Stable failures produced by deterministic grid layout.
 */
enum class GridLayoutErrorCode
{
    None,
    InstanceCapacityExceeded,
    ParameterOutOfRange,
    SceneRevisionStale,
    InstanceBoundsInvalid,
    LockedInstanceConflict,
    InstanceNotFound,
};

/**
 * @brief Structured deterministic grid-layout failure.
 */
struct GridLayoutError
{
    GridLayoutErrorCode code{GridLayoutErrorCode::None};
    std::string instanceid;
    std::string field;
    std::string message;
};

/**
 * @brief One stable scene item presented to the layout policy.
 */
struct GridLayoutItem
{
    ModelInstance instance;
    ModelTransform requestedtransform;
    ModelTransform currentderivedlayouttransform;
};

/**
 * @brief Complete immutable input for one grid-layout transaction.
 */
struct GridLayoutRequest
{
    SceneLayout layout;
    std::uint64_t currentscenerevision{0U};
    std::uint64_t expectedscenerevision{0U};
    std::vector<GridLayoutItem> items;
};

/**
 * @brief One derived row-major placement.
 */
struct GridLayoutPlacement
{
    std::string instanceid;
    int row{0};
    int column{0};
    ModelTransform requestedtransform;
    ModelTransform derivedlayouttransform;
    ModelTransform effectivetransform;
    BoundingBox effectivebboxmm;
    double layoutoffsetxmm{0.0};
    double layoutoffsetymm{0.0};
};

/**
 * @brief Atomic deterministic grid-layout result.
 */
struct GridLayoutResult
{
    bool changed{false};
    std::uint64_t sourcescenerevision{0U};
    std::uint64_t derivedscenerevision{0U};
    BoundingBox boundsmm;
    std::vector<GridLayoutPlacement> placements;
    std::optional<GridLayoutError> error;

    /**
     * @brief Report whether layout validation and placement succeeded.
     * @return True when no structured error exists.
     */
    bool IsValid() const;
};

/**
 * @brief Return the stable protocol name for a layout error.
 * @param code Grid-layout error code.
 * @return Stable ASCII error name.
 */
std::string_view GridLayoutErrorCodeName(GridLayoutErrorCode code);

/**
 * @brief Compute deterministic row-major placements without mutating input.
 * @param request Scene revision, layout settings, and stable ordered items.
 * @return Complete placements or one fail-closed structured error.
 */
GridLayoutResult ComputeGridLayout(const GridLayoutRequest& request);

}  // namespace slicer_core
