#include "slicer_core/output/tiff/TiffBackendBuildInfo.h"

#ifdef SLICER_CORE_HAS_LIBTIFF
#include <tiffio.h>
#endif

namespace slicer_core
{

TiffBackendBuildInfo GetTiffBackendBuildInfo()
{
    TiffBackendBuildInfo info;
    info.configuredbackend = SLICESOFT_CONFIGURED_TIFF_BACKEND;

#ifdef SLICER_CORE_HAS_LIBTIFF
    info.libtiffdependencyavailable = true;
    const char* version = TIFFGetVersion();
    if (version != nullptr)
    {
        info.libtiffversion = version;
        const std::size_t lineEnd = info.libtiffversion.find_first_of("\r\n");
        if (lineEnd != std::string::npos)
        {
            info.libtiffversion.resize(lineEnd);
        }
    }
#endif

    return info;
}

}  // namespace slicer_core
