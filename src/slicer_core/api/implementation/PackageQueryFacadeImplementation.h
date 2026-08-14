#pragma once

#include "slicer_core/api/PackageQueryFacade.h"

#include <memory>

namespace slicer_core::api::implementation
{

/**
 * @brief 创建无 Qt 的只读 PackageQueryFacade。
 * @return 由调用方独占持有的 PackageQueryFacade 实例。
 */
std::unique_ptr<PackageQueryFacade> CreatePackageQueryFacade();

}  // namespace slicer_core::api::implementation
