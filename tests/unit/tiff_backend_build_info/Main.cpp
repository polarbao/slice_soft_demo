#include "slicer_core/output/tiff/TiffBackendBuildInfo.h"

#include <iostream>
#include <string>

namespace
{

int Fail(const std::string& message)
{
    std::cerr << "tiff_backend_build_info_unit_tests: " << message << '\n';
    return 1;
}

}  // namespace

int main()
{
    const slicer_core::TiffBackendBuildInfo info =
        slicer_core::GetTiffBackendBuildInfo();

    if (info.configuredbackend != SLICESOFT_TEST_EXPECTED_TIFF_BACKEND)
    {
        return Fail(
            "configured backend mismatch: " + info.configuredbackend);
    }
    if (!info.handwrittenavailable)
    {
        return Fail("handwritten writer must remain available");
    }

    const bool expectsLibTiff =
        info.configuredbackend == "libtiff";
    if (info.libtiffdependencyavailable != expectsLibTiff)
    {
        return Fail("LibTIFF dependency availability mismatch");
    }
    if (expectsLibTiff && info.libtiffversion.empty())
    {
        return Fail("LibTIFF build must expose a version string");
    }
    if (info.libtiffwriterimplemented)
    {
        return Fail("03D-02 must not claim the LibTIFF writer is implemented");
    }

    std::cout << "tiff_backend_build_info_unit_tests: PASS\n";
    return 0;
}
