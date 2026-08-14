#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/ModelDtos.h"

namespace slicer_core::api {

/** @brief 管理模型导入及句柄生命周期的无 Qt Facade。 */
class ModelFacade
{
public:
    virtual ~ModelFacade() = default;

    /** @brief 导入一个模型。 @param request 调用方持有的导入选项。 @param cancel_token 取消源。 @return 元数据或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<ModelMetadata> Import(
        const ModelImportRequest& request,
        const ICancelToken& cancel_token) noexcept = 0;

    /** @brief 获取缓存元数据。 @param model_id 已导入句柄。 @return 元数据或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<ModelMetadata> GetMetadata(
        ModelId model_id) const noexcept = 0;

    /** @brief 释放已导入句柄。 @param model_id 已导入句柄。 @return 成功结果或 PM-SLICER 错误。 */
    [[nodiscard]] virtual ApiResult<void> Release(ModelId model_id) noexcept = 0;
};

}  // namespace slicer_core::api
