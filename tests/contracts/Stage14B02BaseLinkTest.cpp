#include "slicer_core/api/implementation/ModelFacadeImplementation.h"
#include "slicer_core/api/implementation/PackageQueryFacadeImplementation.h"

#include <memory>

int main()
{
    const std::unique_ptr<slicer_core::api::ModelFacade> modelFacade =
        slicer_core::api::implementation::CreateModelFacade();
    const std::unique_ptr<slicer_core::api::PackageQueryFacade> packageFacade =
        slicer_core::api::implementation::CreatePackageQueryFacade();
    return modelFacade != nullptr && packageFacade != nullptr ? 0 : 1;
}
