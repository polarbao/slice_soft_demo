#pragma once

#include "slicer_module/CapabilityJsonAdapter.h"

#include <memory>
#include <string>

namespace slicesoft::module
{

/** @brief Wires read-only RGBWSV package capabilities to PackageQueryFacade. */
class PackageCapabilityAdapter final
{
public:
    /** @brief Creates an adapter with the production package facade. */
    PackageCapabilityAdapter();

    /** @brief Destroys the production package facade. */
    ~PackageCapabilityAdapter();

    /** @brief Executes a package capability. @param capability Frozen ID. @param request DTO object. @return Result envelope. */
    [[nodiscard]] slicer_core::Json Execute(
        const std::string& capability,
        const slicer_core::Json& request) const;

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

}  // namespace slicesoft::module
