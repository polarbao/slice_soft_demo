#include "slicer_core/geometry/OpenVdbAdapter.h"
#include "slicer_core/materials/texture_application/SurfaceShellTexturePrototype.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

bool ExpectEqual(const int actual, const int expected, const std::string& message)
{
    if (actual != expected)
    {
        std::cerr << "FAIL " << message << " expected=" << expected << " actual=" << actual << '\n';
        return false;
    }
    return true;
}

slicer_core::SurfaceShellTextureResult RunBox(const double shellMm)
{
    slicer_core::SurfaceShellTextureOptions options;
    options.case_name = "generated-box";
    options.voxel_size_mm = 0.05;
    options.shell_thickness_mm = shellMm;
    options.texture_source = slicer_core::SurfaceShellTextureSource::Checker;
    return slicer_core::RunSurfaceShellTexturePrototype(options);
}

bool OpenVdbOffGracefulSkip()
{
    const slicer_core::OpenVdbStatus status = slicer_core::GetOpenVdbStatus();
    if (status.compiled_with_openvdb)
    {
        return true;
    }

    const slicer_core::SurfaceShellTextureResult result = RunBox(0.10);
    return ExpectTrue(!result.error.empty(), "OFF build should report unavailable OpenVDB");
}

bool GeneratedBoxLevelSetNonEmpty()
{
    const slicer_core::SurfaceShellTextureResult result = RunBox(0.10);
    if (!ExpectTrue(result.error.empty(), "generated box should run"))
    {
        std::cerr << "  error=" << result.error << '\n';
        return false;
    }
    return ExpectTrue(result.level_set.active_voxels > 0, "level set active voxels")
        && ExpectTrue(result.shell.inside_voxels > 0, "inside voxels");
}

bool ShellAndInteriorDisjoint()
{
    const slicer_core::SurfaceShellTextureResult result = RunBox(0.10);
    if (!ExpectTrue(result.error.empty(), "generated box should run"))
    {
        return false;
    }

    for (std::size_t index{0}; index < result.shell.shell_mask.size(); ++index)
    {
        if (result.shell.shell_mask.at(index) != 0 && result.shell.interior_mask.at(index) != 0)
        {
            std::cerr << "FAIL shell and interior overlap at index=" << index << '\n';
            return false;
        }
    }
    return true;
}

bool ShellPlusInteriorEqualsInside()
{
    const slicer_core::SurfaceShellTextureResult result = RunBox(0.10);
    if (!ExpectTrue(result.error.empty(), "generated box should run"))
    {
        return false;
    }
    return ExpectEqual(
        result.shell.shell_voxels + result.shell.interior_voxels,
        result.shell.inside_voxels,
        "shell + interior equals inside");
}

bool OutsideColoredVoxelsZero()
{
    const slicer_core::SurfaceShellTextureResult result = RunBox(0.10);
    if (!ExpectTrue(result.error.empty(), "generated box should run"))
    {
        return false;
    }
    return ExpectEqual(result.outside_colored_voxels, 0, "outside colored voxels")
        && ExpectEqual(result.shell.outside_colored_voxels, 0, "shell outside colored voxels");
}

bool ShellThicknessMonotonic()
{
    const slicer_core::SurfaceShellTextureResult thin = RunBox(0.05);
    const slicer_core::SurfaceShellTextureResult medium = RunBox(0.10);
    const slicer_core::SurfaceShellTextureResult thick = RunBox(0.20);
    if (!ExpectTrue(thin.error.empty(), "thin shell should run")
        || !ExpectTrue(medium.error.empty(), "medium shell should run")
        || !ExpectTrue(thick.error.empty(), "thick shell should run"))
    {
        return false;
    }

    return ExpectTrue(medium.shell.shell_voxels >= thin.shell.shell_voxels, "0.10 shell >= 0.05 shell")
        && ExpectTrue(thick.shell.shell_voxels >= medium.shell.shell_voxels, "0.20 shell >= 0.10 shell")
        && ExpectTrue(medium.shell.interior_voxels <= thin.shell.interior_voxels, "0.10 interior <= 0.05 interior")
        && ExpectTrue(thick.shell.interior_voxels <= medium.shell.interior_voxels, "0.20 interior <= 0.10 interior");
}

bool InvalidShellThicknessRejected()
{
    slicer_core::SurfaceShellTextureOptions options;
    options.shell_thickness_mm = 0.0;
    const slicer_core::SurfaceShellTextureResult result = slicer_core::RunSurfaceShellTexturePrototype(options);
    return ExpectTrue(!result.error.empty(), "invalid shell thickness should be rejected");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"openvdb_off_graceful_skip", OpenVdbOffGracefulSkip},
        {"generated_box_level_set_non_empty", GeneratedBoxLevelSetNonEmpty},
        {"shell_and_interior_disjoint", ShellAndInteriorDisjoint},
        {"shell_plus_interior_equals_inside", ShellPlusInteriorEqualsInside},
        {"outside_colored_voxels_zero", OutsideColoredVoxelsZero},
        {"shell_thickness_monotonic", ShellThicknessMonotonic},
        {"invalid_shell_thickness_rejected", InvalidShellThicknessRejected},
    };

    const bool openVdbEnabled = slicer_core::GetOpenVdbStatus().compiled_with_openvdb;
    for (const auto& test : tests)
    {
        if (!openVdbEnabled && test.first != "openvdb_off_graceful_skip" && test.first != "invalid_shell_thickness_rejected")
        {
            std::cout << "SKIP " << test.first << " USE_OPENVDB=OFF\n";
            continue;
        }

        if (!test.second())
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }

    std::cout << "Surface shell texture unit tests complete.\n";
    return 0;
}
