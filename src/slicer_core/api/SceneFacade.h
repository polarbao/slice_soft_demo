#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneDtos.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/SliceDtos.h"

namespace slicer_core::api {

/** @brief Qt-free facade for authoritative scene Commit operations. */
class SceneFacade
{
public:
    virtual ~SceneFacade() = default;

    /** @brief Applies one atomic operation batch. @param request Commit request. @param cancel_token Cancellation source. @return New snapshot or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<SceneSnapshot> ApplyOperation(
        const SceneOperationRequest& request,
        const ICancelToken& cancel_token) noexcept = 0;

    /** @brief Reads a scene snapshot. @param scene_id Scene handle. @return Snapshot or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<SceneSnapshot> GetSnapshot(
        SceneId scene_id) const noexcept = 0;

    /** @brief Gets textured top or 3D view data. @param request View request. @param cancel_token Cancellation source. @return View data or explicit texture error. */
    [[nodiscard]] virtual ApiResult<SceneViewData> GetViewData(
        const SceneViewDataRequest& request,
        const ICancelToken& cancel_token) const noexcept = 0;

    /** @brief Evaluates authoritative collisions. @param snapshot Scene state. @param cancel_token Cancellation source. @return Collision report or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<CollisionReport> CheckCollision(
        const SceneSnapshot& snapshot,
        const ICancelToken& cancel_token) const noexcept = 0;
};

/** @brief Fast non-authoritative preflight facade resident in base. */
class PreflightFacade
{
public:
    virtual ~PreflightFacade() = default;

    /** @brief Runs fast preflight. @param request Preflight input. @param cancel_token Cancellation source. @return Non-authoritative result or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<PreflightResult> RunFast(
        const PreflightRequest& request,
        const ICancelToken& cancel_token) const noexcept = 0;
};

}  // namespace slicer_core::api
