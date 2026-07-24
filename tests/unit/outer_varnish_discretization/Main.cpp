#include "slicer_core/materials/varnish_geometry/OuterVarnishDiscretization.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool ExpectNear(
    const double actual,
    const double expected,
    const std::string& message)
{
    return ExpectTrue(
        std::abs(actual - expected) <= 1.0e-12,
        message);
}

bool DisabledVarnishProducesZeroDiscretization()
{
    slicer_core::OuterVarnishShellConfig config;
    config.enabled = false;
    config.thickness_mm = 0.081;

    const auto result = slicer_core::ComputeOuterVarnishDiscretization(
        config,
        25.4 / 635.0,
        25.4 / 600.0);

    return ExpectTrue(!result.enabled, "disabled varnish remains disabled")
        && ExpectTrue(result.radius_x_px == 0, "disabled X radius is zero")
        && ExpectTrue(result.radius_y_px == 0, "disabled Y radius is zero")
        && ExpectNear(
            result.effective_thickness_x_mm,
            0.0,
            "disabled X effective thickness is zero")
        && ExpectNear(
            result.effective_thickness_y_mm,
            0.0,
            "disabled Y effective thickness is zero");
}

bool NonSquareDpiUsesIndependentAxisRadii()
{
    slicer_core::OuterVarnishShellConfig config;
    config.enabled = true;
    config.thickness_mm = 0.081;

    const double pixelSizeXmm = 25.4 / 635.0;
    const double pixelSizeYmm = 25.4 / 600.0;
    const auto result = slicer_core::ComputeOuterVarnishDiscretization(
        config,
        pixelSizeXmm,
        pixelSizeYmm);

    return ExpectTrue(result.enabled, "varnish discretization is enabled")
        && ExpectTrue(result.radius_x_px == 3, "X radius uses 635 DPI pitch")
        && ExpectTrue(result.radius_y_px == 2, "Y radius uses 600 DPI pitch")
        && ExpectTrue(
            slicer_core::IsOuterVarnishOffsetWithinThickness(
                result,
                3,
                0),
            "X discrete shell reaches the reported X radius")
        && ExpectTrue(
            slicer_core::IsOuterVarnishOffsetWithinThickness(
                result,
                0,
                2),
            "Y discrete shell reaches the reported Y radius")
        && ExpectTrue(
            !slicer_core::IsOuterVarnishOffsetWithinThickness(
                result,
                3,
                2),
            "discrete shell excludes the bounding-box corner")
        && ExpectNear(
            result.effective_thickness_x_mm,
            3.0 * pixelSizeXmm,
            "X effective thickness uses X pitch")
        && ExpectNear(
            result.effective_thickness_y_mm,
            2.0 * pixelSizeYmm,
            "Y effective thickness uses Y pitch");
}

bool LegacySquareDpiRemainsSymmetric()
{
    slicer_core::OuterVarnishShellConfig config;
    config.enabled = true;
    config.thickness_mm = 0.05;

    const double pixelSizeMm = 25.4 / 600.0;
    const auto result = slicer_core::ComputeOuterVarnishDiscretization(
        config,
        pixelSizeMm,
        pixelSizeMm);

    return ExpectTrue(result.radius_x_px == 2, "legacy X radius remains valid")
        && ExpectTrue(result.radius_y_px == 2, "legacy Y radius remains valid")
        && ExpectNear(
            result.effective_thickness_x_mm,
            result.effective_thickness_y_mm,
            "legacy effective thickness remains symmetric");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"disabled_varnish_produces_zero_discretization",
         DisabledVarnishProducesZeroDiscretization},
        {"non_square_dpi_uses_independent_axis_radii",
         NonSquareDpiUsesIndependentAxisRadii},
        {"legacy_square_dpi_remains_symmetric",
         LegacySquareDpiRemainsSymmetric},
    };

    bool passed{true};
    for (const auto& test : tests)
    {
        std::cout << "RUN: " << test.first << '\n';
        const bool current = test.second();
        std::cout << (current ? "PASS: " : "FAIL: ")
                  << test.first << '\n';
        passed = current && passed;
    }
    return passed ? 0 : 1;
}
