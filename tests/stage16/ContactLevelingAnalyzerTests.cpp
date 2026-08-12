#include "slicer_core/geometry/ContactLevelingAnalyzer.h"

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
    report.triangles = {
        {
            {-5.0, -15.0, 0.0},
            {-3.0, 15.0, 0.0},
            {0.0, 15.0, 2.0},
        },
        {
            {-5.0, -15.0, 0.0},
            {0.0, 15.0, 2.0},
            {0.0, -15.0, 2.0},
        },
        {
            {0.0, -15.0, 2.0},
            {0.0, 15.0, 2.0},
            {3.0, 15.0, rightEdgeZMm},
        },
        {
            {0.0, -15.0, 2.0},
            {3.0, 15.0, rightEdgeZMm},
            {5.0, -15.0, rightEdgeZMm},
        },
    };
    return report;
}

bool FindsDeterministicDiagnosticCandidate()
{
    const slicer_core::ModelReport model{MakeArchedNail(0.5)};
    const slicer_core::ContactLevelingCandidate first{
        slicer_core::AnalyzeContactLeveling(model)};
    const slicer_core::ContactLevelingCandidate second{
        slicer_core::AnalyzeContactLeveling(model)};
    return Expect(first.available, "candidate is available")
        && Expect(first.status == "diagnostic_only", "candidate stays diagnostic")
        && Expect(first.evaluatedcandidatecount > 49, "coarse and refine scans run")
        && Expect(first.contactareaimprovementmm2 >= -kTolerance,
                  "candidate does not reduce primary contact score")
        && Expect(first.positivezconstraintsatisfied, "+Z constraint is preserved")
        && Expect(first.positiveyconstraintsatisfied, "+Y constraint is preserved")
        && Expect(first.angleconstraintsatisfied, "angle constraint is preserved")
        && Expect(first.heightconstraintsatisfied, "height budget is preserved")
        && Expect(first.footprintconstraintsatisfied, "footprint budget is preserved")
        && Expect(std::abs(first.candidateangledeg - second.candidateangledeg)
                      <= kTolerance,
                  "search is deterministic")
        && Expect(model.bbox_mm.min.z == 0.0
                      && model.triangles.front().a.z == 0.0,
                  "analyzer does not mutate source geometry");
}

bool ReportsStableBaselineRejection()
{
    slicer_core::ModelReport model{MakeArchedNail(0.0)};
    model.bbox_mm.max = {35.0, 5.0, 2.5};
    const slicer_core::ContactLevelingCandidate result{
        slicer_core::AnalyzeContactLeveling(model)};
    return Expect(!result.available, "invalid nail pose is rejected")
        && Expect(result.rejectionreason
                      == "baseline_long_axis_is_not_positive_y",
                  "rejection reason is stable");
}

bool RejectsInvalidSearchPolicy()
{
    slicer_core::ContactLevelingPolicy policy;
    policy.coarseangleincrementdeg = 0.0;
    try
    {
        static_cast<void>(
            slicer_core::AnalyzeContactLeveling(MakeArchedNail(0.5), {}, policy));
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    return Expect(false, "invalid search policy must throw");
}

}  // namespace

int main()
{
    const bool passed = FindsDeterministicDiagnosticCandidate()
        && ReportsStableBaselineRejection()
        && RejectsInvalidSearchPolicy();
    if (passed)
    {
        std::cout << "stage16 contact leveling analyzer tests passed\n";
    }
    return passed ? 0 : 1;
}
