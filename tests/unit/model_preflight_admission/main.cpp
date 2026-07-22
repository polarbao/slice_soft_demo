#include "slicer_core/preflight/ModelPreflightAdmissionPolicy.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace
{

struct TestCase
{
    std::string name;
    std::function<bool()> run;
};

bool ExpectTrue(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool ContainsCode(const std::vector<std::string>& codes, const std::string& code)
{
    return std::find(codes.begin(), codes.end(), code) != codes.end();
}

slicer_core::ModelPreflightIssue MakeIssue(
    const std::string& code,
    const slicer_core::ModelPreflightIssueSeverity severity,
    const std::string& category = "topology")
{
    slicer_core::ModelPreflightIssue issue;
    issue.code = code;
    issue.category = category;
    issue.severity = severity;
    issue.count = 1U;
    issue.summaryKey = "preflight." + code + ".summary";
    issue.recommendationKey = "preflight." + code + ".recommendation";
    return issue;
}

slicer_core::ModelPreflightResult MakeResult(
    const slicer_core::ModelPreflightStatus status = slicer_core::ModelPreflightStatus::Passed)
{
    slicer_core::ModelPreflightResult result;
    result.status = status;
    result.cacheKey = "fresh-cache-key";
    return result;
}

slicer_core::ModelPreflightAdmissionContext AvailableContext()
{
    slicer_core::ModelPreflightAdmissionContext context;
    context.global_backend_available = true;
    return context;
}

bool CleanInputPassesBothModes()
{
    const slicer_core::ModelPreflightResult admitted =
        slicer_core::EvaluateModelPreflightAdmissions(MakeResult(), AvailableContext());

    return ExpectTrue(
               admitted.legacyAdmission.status
                   == slicer_core::ModelPreflightAdmissionStatus::Passed,
               "clean legacy admission passes")
        && ExpectTrue(
            admitted.globalAdmission.status
                == slicer_core::ModelPreflightAdmissionStatus::Passed,
            "clean global admission passes")
        && ExpectTrue(!admitted.productionOutputWritten, "preflight never writes production output");
}

bool TopologyIssuesWarnLegacyAndBlockGlobal()
{
    const std::vector<std::string> topologyCodes{
        "MESH_SELF_INTERSECTION_CONFIRMED",
        "MESH_BOUNDARY_EDGES",
        "MESH_NON_MANIFOLD_EDGES",
        "MESH_DEGENERATE_TRIANGLES",
        "MESH_DUPLICATE_FACES",
        "MESH_OPPOSITE_DUPLICATE_FACES",
        "MESH_DUPLICATE_FACE_ATTRIBUTE_CONFLICT",
        "MESH_LOCAL_WINDING_INCONSISTENCY",
    };
    const std::string umbrella = slicer_core::ModelPreflightErrorCodeName(
        slicer_core::ModelPreflightErrorCode::GlobalTopologyBlocked);

    bool passed{true};
    for (const std::string& code : topologyCodes)
    {
        slicer_core::ModelPreflightResult diagnostic = MakeResult(
            slicer_core::ModelPreflightStatus::Warning);
        diagnostic.issues.push_back(MakeIssue(
            code,
            slicer_core::ModelPreflightIssueSeverity::Error));
        const auto admitted = slicer_core::EvaluateModelPreflightAdmissions(
            diagnostic,
            AvailableContext());

        passed = ExpectTrue(
                     admitted.legacyAdmission.status
                         == slicer_core::ModelPreflightAdmissionStatus::Warning,
                     code + " remains a legacy compatibility warning")
            && ExpectTrue(
                ContainsCode(admitted.legacyAdmission.warningCodes, code),
                code + " is preserved in legacy warnings")
            && ExpectTrue(
                admitted.globalAdmission.status
                    == slicer_core::ModelPreflightAdmissionStatus::Blocked,
                code + " blocks global mode")
            && ExpectTrue(
                ContainsCode(admitted.globalAdmission.blockerCodes, code),
                code + " is preserved in global blockers")
            && ExpectTrue(
                ContainsCode(admitted.globalAdmission.blockerCodes, umbrella),
                code + " adds the global topology umbrella blocker")
            && passed;
    }
    return passed;
}

bool SharedFatalIssuesBlockBothModes()
{
    const std::vector<slicer_core::ModelPreflightErrorCode> fatalCodes{
        slicer_core::ModelPreflightErrorCode::ImportInvalid,
        slicer_core::ModelPreflightErrorCode::ResourceMissing,
        slicer_core::ModelPreflightErrorCode::NonFiniteGeometry,
        slicer_core::ModelPreflightErrorCode::AuditIncomplete,
    };

    bool passed{true};
    for (const auto errorCode : fatalCodes)
    {
        const std::string code = slicer_core::ModelPreflightErrorCodeName(errorCode);
        slicer_core::ModelPreflightResult diagnostic = MakeResult(
            slicer_core::ModelPreflightStatus::Blocked);
        diagnostic.issues.push_back(MakeIssue(
            code,
            slicer_core::ModelPreflightIssueSeverity::Error,
            "shared"));
        const auto admitted = slicer_core::EvaluateModelPreflightAdmissions(
            diagnostic,
            AvailableContext());

        passed = ExpectTrue(
                     admitted.legacyAdmission.status
                         == slicer_core::ModelPreflightAdmissionStatus::Blocked,
                     code + " blocks legacy")
            && ExpectTrue(
                admitted.globalAdmission.status
                    == slicer_core::ModelPreflightAdmissionStatus::Blocked,
                code + " blocks global")
            && ExpectTrue(
                ContainsCode(admitted.legacyAdmission.blockerCodes, code),
                code + " remains traceable for legacy")
            && ExpectTrue(
                ContainsCode(admitted.globalAdmission.blockerCodes, code),
                code + " remains traceable for global")
            && passed;
    }
    return passed;
}

bool WarningFallbackIsPreserved()
{
    slicer_core::ModelPreflightResult diagnostic = MakeResult(
        slicer_core::ModelPreflightStatus::Warning);
    const std::string code = slicer_core::ModelPreflightErrorCodeName(
        slicer_core::ModelPreflightErrorCode::ResourceMissing);
    diagnostic.issues.push_back(MakeIssue(
        code,
        slicer_core::ModelPreflightIssueSeverity::Warning,
        "resource"));

    const auto admitted = slicer_core::EvaluateModelPreflightAdmissions(
        diagnostic,
        AvailableContext());
    return ExpectTrue(
               admitted.legacyAdmission.status
                   == slicer_core::ModelPreflightAdmissionStatus::Warning,
               "resource fallback warns legacy")
        && ExpectTrue(
            admitted.globalAdmission.status
                == slicer_core::ModelPreflightAdmissionStatus::Warning,
            "resource fallback warns global")
        && ExpectTrue(
            ContainsCode(admitted.globalAdmission.warningCodes, code),
            "resource fallback code is preserved");
}

bool BackendUnavailableOnlyBlocksGlobal()
{
    slicer_core::ModelPreflightAdmissionContext context;
    context.global_backend_available = false;
    const auto admitted = slicer_core::EvaluateModelPreflightAdmissions(
        MakeResult(),
        context);
    const std::string code = slicer_core::ModelPreflightErrorCodeName(
        slicer_core::ModelPreflightErrorCode::BackendUnavailable);

    return ExpectTrue(
               admitted.legacyAdmission.status
                   == slicer_core::ModelPreflightAdmissionStatus::Passed,
               "legacy does not depend on global backend")
        && ExpectTrue(
            admitted.globalAdmission.status
                == slicer_core::ModelPreflightAdmissionStatus::Blocked,
            "unavailable backend blocks global")
        && ExpectTrue(
            ContainsCode(admitted.globalAdmission.blockerCodes, code),
            "backend blocker is stable");
}

bool LifecycleStatesFailClosed()
{
    const std::vector<slicer_core::ModelPreflightStatus> states{
        slicer_core::ModelPreflightStatus::NotRun,
        slicer_core::ModelPreflightStatus::Pending,
        slicer_core::ModelPreflightStatus::Running,
        slicer_core::ModelPreflightStatus::Blocked,
        slicer_core::ModelPreflightStatus::Stale,
        slicer_core::ModelPreflightStatus::Cancelled,
    };

    bool passed{true};
    for (const auto state : states)
    {
        const auto admitted = slicer_core::EvaluateModelPreflightAdmissions(
            MakeResult(state),
            AvailableContext());
        passed = ExpectTrue(
                     admitted.legacyAdmission.status
                         == slicer_core::ModelPreflightAdmissionStatus::Blocked,
                     "lifecycle state blocks legacy")
            && ExpectTrue(
                admitted.globalAdmission.status
                    == slicer_core::ModelPreflightAdmissionStatus::Blocked,
                "lifecycle state blocks global")
            && ExpectTrue(
                !admitted.legacyAdmission.blockerCodes.empty(),
                "lifecycle state has stable legacy blocker")
            && passed;
    }
    return passed;
}

bool UnknownErrorFailsClosed()
{
    slicer_core::ModelPreflightResult diagnostic = MakeResult(
        slicer_core::ModelPreflightStatus::Warning);
    diagnostic.issues.push_back(MakeIssue(
        "UNKNOWN_GEOMETRY_ERROR",
        slicer_core::ModelPreflightIssueSeverity::Error));
    const auto admitted = slicer_core::EvaluateModelPreflightAdmissions(
        diagnostic,
        AvailableContext());

    return ExpectTrue(
               admitted.legacyAdmission.status
                   == slicer_core::ModelPreflightAdmissionStatus::Blocked,
               "unknown error blocks legacy")
        && ExpectTrue(
            admitted.globalAdmission.status
                == slicer_core::ModelPreflightAdmissionStatus::Blocked,
            "unknown error blocks global");
}

bool AdmissionOrderingIsDeterministic()
{
    slicer_core::ModelPreflightResult first = MakeResult(
        slicer_core::ModelPreflightStatus::Warning);
    first.issues = {
        MakeIssue("MESH_BOUNDARY_EDGES", slicer_core::ModelPreflightIssueSeverity::Error),
        MakeIssue("RESOURCE_WARNING", slicer_core::ModelPreflightIssueSeverity::Warning, "resource"),
        MakeIssue("MESH_NON_MANIFOLD_EDGES", slicer_core::ModelPreflightIssueSeverity::Error),
        MakeIssue("RESOURCE_WARNING", slicer_core::ModelPreflightIssueSeverity::Warning, "resource"),
    };
    slicer_core::ModelPreflightResult second = first;
    std::reverse(second.issues.begin(), second.issues.end());

    const auto admittedFirst = slicer_core::EvaluateModelPreflightAdmissions(
        first,
        AvailableContext());
    const auto admittedSecond = slicer_core::EvaluateModelPreflightAdmissions(
        second,
        AvailableContext());

    return ExpectTrue(
               admittedFirst.legacyAdmission.warningCodes
                   == admittedSecond.legacyAdmission.warningCodes,
               "legacy warning order is deterministic")
        && ExpectTrue(
            admittedFirst.globalAdmission.blockerCodes
                == admittedSecond.globalAdmission.blockerCodes,
            "global blocker order is deterministic")
        && ExpectTrue(
            admittedFirst.globalAdmission.warningCodes
                == admittedSecond.globalAdmission.warningCodes,
            "global warning order is deterministic")
        && ExpectTrue(
            std::is_sorted(
                admittedFirst.globalAdmission.blockerCodes.begin(),
                admittedFirst.globalAdmission.blockerCodes.end()),
            "global blockers are sorted");
}

}  // namespace

int main()
{
    const std::vector<TestCase> tests{
        {"clean_input_passes_both_modes", CleanInputPassesBothModes},
        {"topology_issues_warn_legacy_and_block_global", TopologyIssuesWarnLegacyAndBlockGlobal},
        {"shared_fatal_issues_block_both_modes", SharedFatalIssuesBlockBothModes},
        {"warning_fallback_is_preserved", WarningFallbackIsPreserved},
        {"backend_unavailable_only_blocks_global", BackendUnavailableOnlyBlocksGlobal},
        {"lifecycle_states_fail_closed", LifecycleStatesFailClosed},
        {"unknown_error_fails_closed", UnknownErrorFailsClosed},
        {"admission_ordering_is_deterministic", AdmissionOrderingIsDeterministic},
    };

    bool passed{true};
    for (const TestCase& test : tests)
    {
        const bool current = test.run();
        std::cout << (current ? "PASS: " : "FAIL: ") << test.name << '\n';
        passed = current && passed;
    }
    return passed ? 0 : 1;
}
