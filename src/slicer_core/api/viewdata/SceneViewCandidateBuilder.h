#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneDtos.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"
#include "slicer_core/model.h"

#include <memory>
#include <string>
#include <vector>

namespace slicer_core::api::viewdata_detail
{

/** @brief Imported model and its fail-closed resolved appearance. */
struct PreparedViewModel
{
    std::shared_ptr<const SceneModel> model;
    ResolvedViewAppearance appearance;
};

/** @brief Scene instance bound to one prepared model. */
struct PreparedViewInstance
{
    SceneInstanceState state;
    std::shared_ptr<const PreparedViewModel> model;
};

/** @brief One bounded candidate requested by the ViewData budget policy. */
struct ViewCandidateOptions
{
    int preview_dimension{0};
    ViewLod lod{ViewLod::Lod0};
    std::size_t max_texture_edge_px{0U};
    bool degraded{false};
    std::string degradation_reason;
};

/**
 * @brief Builds a closed textured ViewData candidate.
 * @param request Validated ViewData request.
 * @param snapshot Authoritative scene snapshot.
 * @param prepared Selected and prepared scene instances.
 * @param options Resolution and degradation choices for this candidate.
 * @param cancelToken Cooperative cancellation token.
 * @return Closed candidate or a stable PM-SLICER error.
 */
[[nodiscard]] ApiResult<SceneViewData> BuildViewCandidate(
    const SceneViewDataRequest& request,
    const SceneSnapshot& snapshot,
    const std::vector<PreparedViewInstance>& prepared,
    const ViewCandidateOptions& options,
    const ICancelToken& cancelToken) noexcept;

}  // namespace slicer_core::api::viewdata_detail
