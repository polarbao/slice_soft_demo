#pragma once

#include "slicer_core/api/SliceFacade.h"

#include <memory>

namespace slicer_core::engine
{

/**
 * @brief Creates the engine SliceFacade bound to the existing scene producer.
 * @return Owning facade pointer using the production Legacy scene entry.
 */
[[nodiscard]] std::unique_ptr<api::SliceFacade>
CreateProductionSliceFacade();

}  // namespace slicer_core::engine
