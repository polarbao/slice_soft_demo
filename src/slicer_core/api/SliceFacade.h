#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SliceDtos.h"

namespace slicer_core::api {

/** @brief 仅供引擎使用的生产 RGBWSV 切片 Facade。 */
class SliceFacade
{
public:
    virtual ~SliceFacade() = default;

    /** @brief 运行生产切片。 @param request 调用方持有的路径和场景哈希。 @param cancel_token 取消源。 @param progress_sink 进度回调。 @return 生产包结果或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<SliceResult> Run(
        const SliceRequest& request,
        const ICancelToken& cancel_token,
        const ProgressSink& progress_sink) noexcept = 0;
};

/** @brief 仅供引擎使用的权威预检 Facade。 */
class PreflightFullFacade
{
public:
    virtual ~PreflightFullFacade() = default;

    /** @brief 运行完整预检。 @param request 预检输入。 @param cancel_token 取消源。 @return 权威结果或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<PreflightResult> RunFull(
        const PreflightRequest& request,
        const ICancelToken& cancel_token) const noexcept = 0;
};

/** @brief 仅供引擎使用的显式几何修复 Facade。 */
class RepairFacade
{
public:
    virtual ~RepairFacade() = default;

    /** @brief 修复一个模型。 @param request 调用方持有的源和目标。 @param cancel_token 取消源。 @return 修复证据或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<RepairResult> Run(
        const RepairRequest& request,
        const ICancelToken& cancel_token) noexcept = 0;
};

}  // namespace slicer_core::api
