#pragma once

#include "slicer_core/api/SliceFacade.h"

#include <memory>

namespace slicer_core::engine
{

/**
 * @brief 创建绑定保守 OBJ 修复策略的生产 RepairFacade。
 * @return 由调用方独占持有的 Facade；负责写入、重新导入并严格验证修复产物。
 */
[[nodiscard]] std::unique_ptr<api::RepairFacade>
CreateProductionRepairFacade();

}  // namespace slicer_core::engine
