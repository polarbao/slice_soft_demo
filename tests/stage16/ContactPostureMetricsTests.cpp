#include "slicer_core/geometry/ContactPostureMetrics.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

constexpr double kTolerance{1.0e-9};

bool Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
        return false;
    }
    return true;
}

slicer_core::ModelReport MakeArchedNail(const double rightEdgeZMm)
{
    slicer_core::ModelReport report;
    report.bbox_mm.min = {-5.0, -15.0, 0.0};
    report.bbox_mm.max = {5.0, 15.0, 2.5};

    const slicer_core::Vec3 leftLow{-5.0, -15.0, 0.0};
    const slicer_core::Vec3 centerLow{0.0, -15.0, 2.0};
    const slicer_core::Vec3 rightLow{-5.0 + 10.0, -15.0, rightEdgeZMm};
    const slicer_core::Vec3 leftHigh{-3.0, 15.0, 0.0};
    const slicer_core::Vec3 centerHigh{0.0, 15.0, 2.0};
    const slicer_core::Vec3 rightHigh{3.0, 15.0, rightEdgeZMm};
    report.triangles = {
        {leftLow, leftHigh, centerHigh},
        {leftLow, centerHigh, centerLow},
        {centerLow, centerHigh, rightHigh},
        {centerLow, rightHigh, rightLow},
    };
    return report;
}

bool MeasuresFrozenBandsAndSlabs()
{
    const slicer_core::ContactPostureMetrics first =
        slicer_core::MeasureContactPosture(MakeArchedNail(0.5));
    const slicer_core::ContactPostureMetrics second =
        slicer_core::MeasureContactPosture(MakeArchedNail(0.5));

    return Expect(first.valid, "arched nail is measurable")
        && Expect(first.leftbandvertexcount > 0U, "left band is populated")
        && Expect(first.rightbandvertexcount > 0U, "right band is populated")
        && Expect(first.centerbandvertexcount > 0U, "center band is populated")
        && Expect(first.sideenvelopedeltamm > 0.0, "right side is higher")
        && Expect(first.candidateangledeg > 0.0, "candidate angle keeps sign")
        && Expect(first.firsthalfslabareamm2 > 0.0, "half slab has contact proxy")
        && Expect(first.firstslabareamm2 >= first.firsthalfslabareamm2,
                  "first slab is monotonic")
        && Expect(first.secondslabareamm2 >= first.firstslabareamm2,
                  "second slab is monotonic")
        && Expect(first.positivezconstraintsatisfied, "+Z constraint passes")
        && Expect(first.positiveyconstraintsatisfied, "+Y tip constraint passes")
        && Expect(std::abs(first.candidateangledeg - second.candidateangledeg)
                      <= kTolerance,
                  "measurement is deterministic");
}

bool RejectsWrongLongAxis()
{
    slicer_core::ModelReport report = MakeArchedNail(0.0);
    report.bbox_mm.max = {30.0, 5.0, 2.5};
    const slicer_core::ContactPostureMetrics metrics =
        slicer_core::MeasureContactPosture(report);
    return Expect(!metrics.valid, "wrong long axis is rejected")
        && Expect(metrics.rejectionreason == "long_axis_is_not_positive_y",
                  "wrong long axis has stable reason");
}

bool RejectsInvalidPolicy()
{
    slicer_core::ContactPostureMetricPolicy policy;
    policy.sidebandfraction = 0.5;
    try
    {
        static_cast<void>(
            slicer_core::MeasureContactPosture(MakeArchedNail(0.0), policy));
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    return Expect(false, "invalid policy must throw");
}

}  // namespace

int main()
{
    const bool passed = MeasuresFrozenBandsAndSlabs()
        && RejectsWrongLongAxis()
        && RejectsInvalidPolicy();
    if (passed)
    {
        std::cout << "stage16 contact posture metrics tests passed\n";
    }
    return passed ? 0 : 1;
}
