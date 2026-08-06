#pragma once

#include "slicer_core/api/SliceFacade.h"

#include <memory>

namespace slicer_core::engine
{

/**
 * @brief Create the production repair facade bound to conservative OBJ repair.
 * @return Owning facade pointer that writes, reimports and strictly verifies repair assets.
 */
[[nodiscard]] std::unique_ptr<api::RepairFacade>
CreateProductionRepairFacade();

}  // namespace slicer_core::engine
