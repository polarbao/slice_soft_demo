#pragma once

#include "slicer_core/api/SliceFacade.h"

#include <memory>

namespace slicer_core::engine
{

/**
 * @brief 创建绑定生产导入规则的权威 PreflightFullFacade。
 * @return 由调用方独占持有的 Facade；几何审计前会验证场景和 Profile 标识。
 */
[[nodiscard]] std::unique_ptr<api::PreflightFullFacade>
CreateProductionPreflightFullFacade();

}  // namespace slicer_core::engine
