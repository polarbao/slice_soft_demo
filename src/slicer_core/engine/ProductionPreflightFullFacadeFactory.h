#pragma once

#include "slicer_core/api/SliceFacade.h"

#include <memory>

namespace slicer_core::engine
{

/**
 * @brief Create the authoritative full-preflight facade bound to production import rules.
 * @return Owning facade pointer that validates scene/Profile identity before geometry audit.
 */
[[nodiscard]] std::unique_ptr<api::PreflightFullFacade>
CreateProductionPreflightFullFacade();

}  // namespace slicer_core::engine
