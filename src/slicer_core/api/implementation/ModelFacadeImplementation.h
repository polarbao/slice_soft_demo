#pragma once

#include "slicer_core/api/ModelFacade.h"

#include <memory>

namespace slicer_core::api::implementation
{

/**
 * @brief Create the Qt-free model facade backed by the existing model loader.
 * @return Owning model facade instance.
 */
std::unique_ptr<ModelFacade> CreateModelFacade();

}  // namespace slicer_core::api::implementation
