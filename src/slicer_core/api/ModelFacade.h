#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/ModelDtos.h"

namespace slicer_core::api {

/** @brief Qt-free facade for model import and handle lifetime. */
class ModelFacade
{
public:
    virtual ~ModelFacade() = default;

    /** @brief Imports one model. @param request Caller-owned import options. @param cancel_token Cancellation source. @return Metadata or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<ModelMetadata> Import(
        const ModelImportRequest& request,
        const ICancelToken& cancel_token) noexcept = 0;

    /** @brief Gets cached metadata. @param model_id Imported handle. @return Metadata or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<ModelMetadata> GetMetadata(
        ModelId model_id) const noexcept = 0;

    /** @brief Releases an imported handle. @param model_id Imported handle. @return Success or PM-SLICER error. */
    [[nodiscard]] virtual ApiResult<void> Release(ModelId model_id) noexcept = 0;
};

}  // namespace slicer_core::api
