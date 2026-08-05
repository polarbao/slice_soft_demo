#pragma once

#include "slicer_core/api/PackageQueryFacade.h"

#include <memory>

namespace slicer_core::api::implementation
{

/**
 * @brief Create the Qt-free read-only package facade.
 * @return Owning package-query facade instance.
 */
std::unique_ptr<PackageQueryFacade> CreatePackageQueryFacade();

}  // namespace slicer_core::api::implementation
