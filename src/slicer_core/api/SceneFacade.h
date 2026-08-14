#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneDtos.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/SliceDtos.h"

namespace slicer_core::api {

/** @brief 提供权威场景 Commit 操作的无 Qt Facade。 */
class SceneFacade
{
public:
    virtual ~SceneFacade() = default;

    /** @brief 应用一个原子操作批次。 @param request Commit 请求。 @param cancel_token 取消源。 @return 完整 Commit 响应或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<SceneCommitResult> ApplyOperation(
        const SceneOperationRequest& request,
        const ICancelToken& cancel_token) noexcept = 0;

    /** @brief 读取场景快照。 @param scene_id 场景句柄。 @return 快照或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<SceneSnapshot> GetSnapshot(
        SceneId scene_id) const noexcept = 0;

    /** @brief 获取带纹理的俯视或三维视图数据。 @param request 视图请求。 @param cancel_token 取消源。 @return 视图数据或明确的纹理错误。 */
    [[nodiscard]] virtual ApiResult<SceneViewData> GetViewData(
        const SceneViewDataRequest& request,
        const ICancelToken& cancel_token) const noexcept = 0;

    /** @brief 评估权威碰撞。 @param snapshot 场景状态。 @param cancel_token 取消源。 @return 碰撞报告或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<CollisionReport> CheckCollision(
        const SceneSnapshot& snapshot,
        const ICancelToken& cancel_token) const noexcept = 0;
};

/** @brief 驻留在基础层的快速非权威预检 Facade。 */
class PreflightFacade
{
public:
    virtual ~PreflightFacade() = default;

    /** @brief 运行快速预检。 @param request 预检输入。 @param cancel_token 取消源。 @return 非权威结果或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<PreflightResult> RunFast(
        const PreflightRequest& request,
        const ICancelToken& cancel_token) const noexcept = 0;
};

}  // namespace slicer_core::api
