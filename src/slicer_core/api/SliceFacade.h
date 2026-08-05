#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SliceDtos.h"

namespace slicer_core::api {

/** @brief Engine-only facade for production RGBWSV slicing. */
class SliceFacade
{
public:
    virtual ~SliceFacade() = default;

    /** @brief Runs production slicing. @param request Caller-owned paths and scene hash. @param cancel_token Cancellation source. @param progress_sink Progress callback. @return Package result or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<SliceResult> Run(
        const SliceRequest& request,
        const ICancelToken& cancel_token,
        const ProgressSink& progress_sink) noexcept = 0;
};

/** @brief Engine-only authoritative preflight facade. */
class PreflightFullFacade
{
public:
    virtual ~PreflightFullFacade() = default;

    /** @brief Runs full preflight. @param request Preflight input. @param cancel_token Cancellation source. @return Authoritative result or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<PreflightResult> RunFull(
        const PreflightRequest& request,
        const ICancelToken& cancel_token) const noexcept = 0;
};

/** @brief Engine-only facade for explicit geometry repair. */
class RepairFacade
{
public:
    virtual ~RepairFacade() = default;

    /** @brief Repairs one model. @param request Caller-owned source and destination. @param cancel_token Cancellation source. @return Repair evidence or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<RepairResult> Run(
        const RepairRequest& request,
        const ICancelToken& cancel_token) noexcept = 0;
};

}  // namespace slicer_core::api
