#include "slicer_core/diagnostics/ProductionAdmissionPolicy.h"

#include <exception>
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

slicer_core::ValidationIssue MakeIssue(
    const std::string& code,
    const slicer_core::ValidationSeverity severity = slicer_core::ValidationSeverity::Error)
{
    return slicer_core::MakeValidationIssue(code, severity, "test issue");
}

bool StrictClosedAllowsEmptyIssues()
{
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission({}, slicer_core::AdmissionMode::StrictClosed);
    return ExpectTrue(
               decision.status == slicer_core::AdmissionStatus::ProductionAllowed,
               "empty strict_closed production allowed")
        && ExpectTrue(decision.productionAllowed, "empty strict_closed production flag")
        && ExpectTrue(!decision.nonProduction, "empty strict_closed nonProduction flag");
}

bool StrictClosedSelfIntersectionFailsFast()
{
    const std::vector<slicer_core::ValidationIssue> issues{
        MakeIssue("MESH_SELF_INTERSECTION_CONFIRMED"),
    };
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::StrictClosed);
    return ExpectTrue(decision.status == slicer_core::AdmissionStatus::FailFast, "confirmed self-intersection fail fast")
        && ExpectTrue(!decision.productionAllowed, "confirmed self-intersection not production")
        && ExpectTrue(!decision.blockerCodes.empty(), "confirmed self-intersection blocker");
}

bool StrictClosedNonManifoldBlocksProduction()
{
    const std::vector<slicer_core::ValidationIssue> issues{
        MakeIssue("MESH_NON_MANIFOLD_EDGES"),
    };
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::StrictClosed);
    return ExpectTrue(decision.status == slicer_core::AdmissionStatus::NonProductionOnly, "non-manifold nonProduction")
        && ExpectTrue(!decision.productionAllowed, "non-manifold not production")
        && ExpectTrue(decision.nonProduction, "non-manifold nonProduction flag");
}

bool StrictClosedDuplicateBlocksProduction()
{
    const std::vector<slicer_core::ValidationIssue> issues{
        MakeIssue("MESH_DUPLICATE_FACES"),
    };
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::StrictClosed);
    return ExpectTrue(decision.status == slicer_core::AdmissionStatus::NonProductionOnly, "duplicate nonProduction")
        && ExpectTrue(!decision.productionAllowed, "duplicate not production");
}

bool StrictClosedOppositeDuplicateBlocksProduction()
{
    const std::vector<slicer_core::ValidationIssue> issues{
        MakeIssue("MESH_OPPOSITE_DUPLICATE_FACES"),
    };
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::StrictClosed);
    return ExpectTrue(
               decision.status == slicer_core::AdmissionStatus::NonProductionOnly,
               "opposite duplicate nonProduction")
        && ExpectTrue(!decision.productionAllowed, "opposite duplicate not production");
}

bool StrictClosedLocalWindingBlocksProduction()
{
    const std::vector<slicer_core::ValidationIssue> issues{
        MakeIssue("MESH_LOCAL_WINDING_INCONSISTENCY"),
    };
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::StrictClosed);
    return ExpectTrue(decision.status == slicer_core::AdmissionStatus::NonProductionOnly, "local winding nonProduction")
        && ExpectTrue(!decision.productionAllowed, "local winding not production");
}

bool WarnAndAttemptNeverAllowsProduction()
{
    const std::vector<slicer_core::ValidationIssue> issues{
        MakeIssue("TEXTURE_UV_OUT_OF_RANGE", slicer_core::ValidationSeverity::Warning),
    };
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::WarnAndAttempt);
    return ExpectTrue(decision.status == slicer_core::AdmissionStatus::NonProductionOnly, "warn_and_attempt status")
        && ExpectTrue(!decision.productionAllowed, "warn_and_attempt not production")
        && ExpectTrue(decision.nonProduction, "warn_and_attempt nonProduction");
}

bool DiagnosticOnlyReturnsDiagnosticOnly()
{
    const std::vector<slicer_core::ValidationIssue> issues{
        MakeIssue("MESH_DUPLICATE_FACES"),
    };
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::DiagnosticOnly);
    return ExpectTrue(decision.status == slicer_core::AdmissionStatus::DiagnosticOnly, "diagnostic_only status")
        && ExpectTrue(!decision.productionAllowed, "diagnostic_only not production");
}

bool RepairThenStrictPlaceholderNeverAllowsProduction()
{
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission({}, slicer_core::AdmissionMode::RepairThenStrict);
    return ExpectTrue(
               decision.status == slicer_core::AdmissionStatus::NonProductionOnly,
               "repair_then_strict placeholder status")
        && ExpectTrue(!decision.productionAllowed, "repair_then_strict not production");
}

bool StableNames()
{
    return ExpectTrue(
               slicer_core::AdmissionModeName(slicer_core::AdmissionMode::StrictClosed) == "strict_closed",
               "strict_closed mode name")
        && ExpectTrue(
               slicer_core::AdmissionStatusName(slicer_core::AdmissionStatus::FailFast) == "fail_fast",
               "fail_fast status name");
}

}  // namespace

int main()
{
    const std::vector<std::pair<std::string, bool (*)()>> tests{
        {"strict_closed_allows_empty_issues", StrictClosedAllowsEmptyIssues},
        {"strict_closed_self_intersection_fails_fast", StrictClosedSelfIntersectionFailsFast},
        {"strict_closed_non_manifold_blocks_production", StrictClosedNonManifoldBlocksProduction},
        {"strict_closed_duplicate_blocks_production", StrictClosedDuplicateBlocksProduction},
        {"strict_closed_opposite_duplicate_blocks_production", StrictClosedOppositeDuplicateBlocksProduction},
        {"strict_closed_local_winding_blocks_production", StrictClosedLocalWindingBlocksProduction},
        {"warn_and_attempt_never_allows_production", WarnAndAttemptNeverAllowsProduction},
        {"diagnostic_only_returns_diagnostic_only", DiagnosticOnlyReturnsDiagnosticOnly},
        {"repair_then_strict_placeholder_never_allows_production", RepairThenStrictPlaceholderNeverAllowsProduction},
        {"stable_names", StableNames},
    };

    for (const auto& test : tests)
    {
        std::cout << "RUN " << test.first << std::endl;
        bool passed{false};
        try
        {
            passed = test.second();
        }
        catch (const std::exception& error)
        {
            std::cerr << "FAIL " << test.first << " exception=" << error.what() << '\n';
            return 1;
        }
        if (!passed)
        {
            return 1;
        }
        std::cout << "PASS " << test.first << '\n';
    }

    std::cout << "Production admission policy unit tests complete.\n";
    return 0;
}
