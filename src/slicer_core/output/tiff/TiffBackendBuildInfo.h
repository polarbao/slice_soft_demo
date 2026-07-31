#pragma once

#include <string>

namespace slicer_core
{

/**
 * @brief Describes the TIFF dependency capabilities compiled into this binary.
 */
struct TiffBackendBuildInfo
{
    std::string configuredbackend;
    bool handwrittenavailable{true};
    bool libtiffdependencyavailable{false};
    bool libtiffwriterimplemented{false};
    bool libtiffstrippedwriterimplemented{false};
    bool libtifftiledwriterimplemented{false};
    std::string libtiffversion;
};

/**
 * @brief Returns build-time TIFF backend and dependency information.
 * @return Stable capability metadata for diagnostics and runtime packaging.
 */
TiffBackendBuildInfo GetTiffBackendBuildInfo();

}  // namespace slicer_core
