#pragma once

#include "slicer_core/api/SliceFacade.h"

#include <memory>

namespace slicer_core::engine
{

/**
 * @brief 创建绑定现有场景生产器的引擎 SliceFacade。
 * @return 使用生产 Legacy 场景入口、由调用方独占持有的 SliceFacade。
 */
[[nodiscard]] std::unique_ptr<api::SliceFacade>
CreateProductionSliceFacade();

}  // namespace slicer_core::engine
