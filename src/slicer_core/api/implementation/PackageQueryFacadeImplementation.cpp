#include "slicer_core/api/implementation/PackageQueryFacadeImplementation.h"

#include "slicer_core/api/implementation/PackageQueryFacadeInternal.h"

#include <memory>

namespace slicer_core::api::implementation
{

std::unique_ptr<PackageQueryFacade> CreatePackageQueryFacade()
{
    return std::make_unique<detail::PackageQueryFacadeService>();
}

}  // namespace slicer_core::api::implementation
