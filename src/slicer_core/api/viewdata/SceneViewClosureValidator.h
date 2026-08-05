#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/SceneViewDtos.h"

namespace slicer_core::api::viewdata_detail
{

/**
 * @brief Validates all ViewData resource and material identity references.
 * @param viewData Candidate response.
 * @return Success when the payload is closed, otherwise a stable error.
 */
[[nodiscard]] ApiResult<void> ValidateViewDataClosure(
    const SceneViewData& viewData) noexcept;

}  // namespace slicer_core::api::viewdata_detail
