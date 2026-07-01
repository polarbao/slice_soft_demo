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

bool ContainsCode(const std::vector<std::string>& codes, const std::string& code)
{
    for (const std::string& existingCode : codes)
    {
        if (existingCode == code)
        {
            return true;
        }
    }
    return false;
}

bool ExpectContainsCode(
    const std::vector<std::string>& codes,
    const std::string& code,
    const std::string& message)
{
    return ExpectTrue(ContainsCode(codes, code), message + " contains " + code);
}

const std::vector<std::string>& RequiredGateCodes()
{
    static const std::vector<std::string> codes{
        "MESH_BOUNDARY_EDGES",
        "MESH_SELF_INTERSECTION_CONFIRMED",
        "MESH_NON_MANIFOLD_EDGES",
        "MESH_DUPLICATE_FACES",
        "MESH_OPPOSITE_DUPLICATE_FACES",
        "MESH_LOCAL_WINDING_INCONSISTENCY",
        "OPENVDB_UNAVAILABLE",
        "OPENVDB_LEVEL_SET_FAILED",
    };
    return codes;
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

bool StrictClosedBoundaryBlocksProduction()
{
    const std::vector<slicer_core::ValidationIssue> issues{
        MakeIssue("MESH_BOUNDARY_EDGES"),
    };
    const slicer_core::ProductionAdmissionDecision decision =
        slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::StrictClosed);
    return ExpectTrue(decision.status == slicer_core::AdmissionStatus::NonProductionOnly, "boundary nonProduction")
        && ExpectTrue(!decision.productionAllowed, "boundary not production")
        && ExpectTrue(decision.nonProduction, "boundary nonProduction flag");
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

bool StrictClosedRequiredGateMatrix()
{
    bool ok{true};
    for (const std::string& code : RequiredGateCodes())
    {
        const std::vector<slicer_core::ValidationIssue> issues{
            MakeIssue(code),
        };
        const slicer_core::ProductionAdmissionDecision decision =
            slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::StrictClosed);
        if (code == "MESH_SELF_INTERSECTION_CONFIRMED")
        {
            ok = ExpectTrue(decision.status == slicer_core::AdmissionStatus::FailFast, code + " fail_fast")
                && ExpectTrue(!decision.productionAllowed, code + " not production")
                && ExpectTrue(!decision.nonProduction, code + " rejected before nonProduction output")
                && ExpectContainsCode(decision.blockerCodes, code, code + " blocker")
                && ok;
        }
        else
        {
            ok = ExpectTrue(decision.status == slicer_core::AdmissionStatus::NonProductionOnly, code + " blocked")
                && ExpectTrue(!decision.productionAllowed, code + " not production")
                && ExpectTrue(decision.nonProduction, code + " nonProduction")
                && ExpectContainsCode(decision.blockerCodes, code, code + " blocker")
                && ok;
        }
    }
    return ok;
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

bool WarnAndAttemptRequiredGateMatrix()
{
    bool ok{true};
    for (const std::string& code : RequiredGateCodes())
    {
        const std::vector<slicer_core::ValidationIssue> issues{
            MakeIssue(code),
        };
        const slicer_core::ProductionAdmissionDecision decision =
            slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::WarnAndAttempt);
        ok = ExpectTrue(decision.status == slicer_core::AdmissionStatus::NonProductionOnly, code + " warn status")
            && ExpectTrue(!decision.productionAllowed, code + " warn not production")
            && ExpectTrue(decision.nonProduction, code + " warn nonProduction")
            && ExpectContainsCode(decision.blockerCodes, code, code + " warn blocker")
            && ok;
    }
    return ok;
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

bool DiagnosticOnlyRequiredGateMatrix()
{
    bool ok{true};
    for (const std::string& code : RequiredGateCodes())
    {
        const std::vector<slicer_core::ValidationIssue> issues{
            MakeIssue(code),
        };
        const slicer_core::ProductionAdmissionDecision decision =
            slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::DiagnosticOnly);
        ok = ExpectTrue(decision.status == slicer_core::AdmissionStatus::DiagnosticOnly, code + " diagnostic status")
            && ExpectTrue(!decision.productionAllowed, code + " diagnostic not production")
            && ExpectTrue(decision.nonProduction, code + " diagnostic nonProduction")
            && ExpectContainsCode(decision.blockerCodes, code, code + " diagnostic blocker")
            && ok;
    }
    return ok;
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

bool RepairThenStrictRequiredGateMatrix()
{
    bool ok{true};
    for (const std::string& code : RequiredGateCodes())
    {
        const std::vector<slicer_core::ValidationIssue> issues{
            MakeIssue(code),
        };
        const slicer_core::ProductionAdmissionDecision decision =
            slicer_core::EvaluateProductionAdmission(issues, slicer_core::AdmissionMode::RepairThenStrict);
        ok = ExpectTrue(
                 decision.status == slicer_core::AdmissionStatus::NonProductionOnly,
                 code + " repair placeholder status")
            && ExpectTrue(!decision.productionAllowed, code + " repair placeholder not production")
            && ExpectTrue(decision.nonProduction, code + " repair placeholder nonProduction")
            && ExpectContainsCode(decision.blockerCodes, code, code + " repair placeholder blocker")
            && ok;
    }
    return ok;
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
        {"strict_closed_boundary_blocks_production", StrictClosedBoundaryBlocksProduction},
        {"strict_closed_duplicate_blocks_production", StrictClosedDuplicateBlocksProduction},
        {"strict_closed_opposite_duplicate_blocks_production", StrictClosedOppositeDuplicateBlocksProduction},
        {"strict_closed_local_winding_blocks_production", StrictClosedLocalWindingBlocksProduction},
        {"strict_closed_required_gate_matrix", StrictClosedRequiredGateMatrix},
        {"warn_and_attempt_never_allows_production", WarnAndAttemptNeverAllowsProduction},
        {"warn_and_attempt_required_gate_matrix", WarnAndAttemptRequiredGateMatrix},
        {"diagnostic_only_returns_diagnostic_only", DiagnosticOnlyReturnsDiagnosticOnly},
        {"diagnostic_only_required_gate_matrix", DiagnosticOnlyRequiredGateMatrix},
        {"repair_then_strict_placeholder_never_allows_production", RepairThenStrictPlaceholderNeverAllowsProduction},
        {"repair_then_strict_required_gate_matrix", RepairThenStrictRequiredGateMatrix},
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
