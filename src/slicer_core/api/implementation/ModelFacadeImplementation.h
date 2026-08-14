#pragma once

#include "slicer_core/api/ModelFacade.h"

#include <memory>

namespace slicer_core::api::implementation
{

/**
 * @brief 创建由现有模型加载器支撑的无 Qt ModelFacade。
 * @return 由调用方独占持有的 ModelFacade 实例。
 */
std::unique_ptr<ModelFacade> CreateModelFacade();

}  // namespace slicer_core::api::implementation
