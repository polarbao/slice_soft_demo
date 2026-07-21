#include "slicer_core/diagnostics/ModelPreflightReport.h"
#include "slicer_core/preflight/ModelPreflightCacheIdentity.h"
#include "slicer_core/preflight/ModelPreflightTypes.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

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

slicer_core::ModelPreflightCacheIdentity MakeIdentity()
{
    slicer_core::ModelPreflightCacheIdentity identity;
    identity.sourceHash = "source-hash";
    identity.resourceHash = "resource-hash";
    identity.transformHash = "transform-hash";
    identity.optionsHash = "options-hash";
    identity.algorithmVersion = "model_preflight_contract.1";
    return identity;
}

slicer_core::Json LoadGolden()
{
    const std::filesystem::path path = std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "tests" / "golden" / "expected"
        / "12e_r4_model_preflight_report_skeleton.json";
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("failed to open golden: " + path.string());
    }
    return slicer_core::Json::parse(input);
}

slicer_core::Json BuildStableProjection(const slicer_core::Json& report)
{
    return slicer_core::Json::object({
        {"schema", report.at("schema")},
        {"status", report.at("status")},
        {"cacheKeyLength", static_cast<int>(report.at("cacheKey").as_string().size())},
        {"identity", report.at("identity")},
        {"legacyAdmission", report.at("legacyAdmission")},
        {"globalAdmission", report.at("globalAdmission")},
        {"productionOutputWritten", report.at("productionOutputWritten")},
        {"issues", report.at("issues")},
    });
}

bool StableNamesMatchContract()
{
    const std::array<std::pair<slicer_core::ModelPreflightErrorCode, std::string>, 8>
        errorNames{{
            {slicer_core::ModelPreflightErrorCode::NotRun, "E_12E_PREFLIGHT_NOT_RUN"},
            {slicer_core::ModelPreflightErrorCode::Stale, "E_12E_PREFLIGHT_STALE"},
            {slicer_core::ModelPreflightErrorCode::ImportInvalid,
             "E_12E_PREFLIGHT_IMPORT_INVALID"},
            {slicer_core::ModelPreflightErrorCode::ResourceMissing,
             "E_12E_PREFLIGHT_RESOURCE_MISSING"},
            {slicer_core::ModelPreflightErrorCode::NonFiniteGeometry,
             "E_12E_PREFLIGHT_NON_FINITE_GEOMETRY"},
            {slicer_core::ModelPreflightErrorCode::AuditIncomplete,
             "E_12E_PREFLIGHT_AUDIT_INCOMPLETE"},
            {slicer_core::ModelPreflightErrorCode::GlobalTopologyBlocked,
             "E_12E_PREFLIGHT_GLOBAL_TOPOLOGY_BLOCKED"},
            {slicer_core::ModelPreflightErrorCode::BackendUnavailable,
             "E_12E_PREFLIGHT_BACKEND_UNAVAILABLE"},
        }};

    bool ok = true;
    for (const auto& [code, expectedName] : errorNames)
    {
        ok = ExpectTrue(
                 slicer_core::ModelPreflightErrorCodeName(code) == expectedName,
                 "preflight error name matches contract")
            && ok;
    }

    const std::array<std::pair<slicer_core::ModelPreflightStatus, std::string>, 7>
        statusNames{{
            {slicer_core::ModelPreflightStatus::NotRun, "not_run"},
            {slicer_core::ModelPreflightStatus::Pending, "pending"},
            {slicer_core::ModelPreflightStatus::Running, "running"},
            {slicer_core::ModelPreflightStatus::Passed, "passed"},
            {slicer_core::ModelPreflightStatus::Warning, "warning"},
            {slicer_core::ModelPreflightStatus::Blocked, "blocked"},
            {slicer_core::ModelPreflightStatus::Stale, "stale"},
        }};
    for (const auto& [status, expectedName] : statusNames)
    {
        ok = ExpectTrue(
                 slicer_core::ModelPreflightStatusName(status) == expectedName,
                 "preflight status name matches contract")
            && ok;
    }

    const std::array<std::pair<slicer_core::ModelPreflightPipelineMode, std::string>, 2>
        modeNames{{
            {slicer_core::ModelPreflightPipelineMode::Legacy, "legacy"},
            {slicer_core::ModelPreflightPipelineMode::GlobalSurfaceShell,
             "global_surface_shell"},
        }};
    for (const auto& [mode, expectedName] : modeNames)
    {
        ok = ExpectTrue(
                 slicer_core::ModelPreflightPipelineModeName(mode) == expectedName,
                 "preflight mode name matches contract")
            && ok;
    }

    const std::array<
        std::pair<slicer_core::ModelPreflightAdmissionStatus, std::string>,
        3>
        admissionNames{{
            {slicer_core::ModelPreflightAdmissionStatus::Passed, "passed"},
            {slicer_core::ModelPreflightAdmissionStatus::Warning, "warning"},
            {slicer_core::ModelPreflightAdmissionStatus::Blocked, "blocked"},
        }};
    for (const auto& [status, expectedName] : admissionNames)
    {
        ok = ExpectTrue(
                 slicer_core::ModelPreflightAdmissionStatusName(status) == expectedName,
                 "preflight admission name matches contract")
            && ok;
    }

    const std::array<std::pair<slicer_core::ModelPreflightIssueSeverity, std::string>, 3>
        severityNames{{
            {slicer_core::ModelPreflightIssueSeverity::Info, "info"},
            {slicer_core::ModelPreflightIssueSeverity::Warning, "warning"},
            {slicer_core::ModelPreflightIssueSeverity::Error, "error"},
        }};
    for (const auto& [severity, expectedName] : severityNames)
    {
        ok = ExpectTrue(
                 slicer_core::ModelPreflightIssueSeverityName(severity) == expectedName,
                 "preflight severity name matches contract")
            && ok;
    }
    return ok;
}

bool CacheKeyIsStableAndSensitive()
{
    const slicer_core::ModelPreflightCacheIdentity original = MakeIdentity();
    const std::string first = slicer_core::ComputeModelPreflightCacheKey(original);
    const std::string second = slicer_core::ComputeModelPreflightCacheKey(original);

    slicer_core::ModelPreflightCacheIdentity sourceChanged = original;
    sourceChanged.sourceHash = "source-hash-2";
    slicer_core::ModelPreflightCacheIdentity resourceChanged = original;
    resourceChanged.resourceHash = "resource-hash-2";
    slicer_core::ModelPreflightCacheIdentity transformChanged = original;
    transformChanged.transformHash = "transform-hash-2";
    slicer_core::ModelPreflightCacheIdentity optionsChanged = original;
    optionsChanged.optionsHash = "options-hash-2";
    slicer_core::ModelPreflightCacheIdentity algorithmChanged = original;
    algorithmChanged.algorithmVersion = "model_preflight_contract.2";

    return ExpectTrue(first == second, "cache key repeats")
        && ExpectTrue(first.size() == 64U, "cache key is SHA-256 hex")
        && ExpectTrue(
            first != slicer_core::ComputeModelPreflightCacheKey(sourceChanged),
            "source hash changes cache key")
        && ExpectTrue(
            first != slicer_core::ComputeModelPreflightCacheKey(resourceChanged),
            "resource hash changes cache key")
        && ExpectTrue(
            first != slicer_core::ComputeModelPreflightCacheKey(transformChanged),
            "transform hash changes cache key")
        && ExpectTrue(
            first != slicer_core::ComputeModelPreflightCacheKey(optionsChanged),
            "options hash changes cache key")
        && ExpectTrue(
            first != slicer_core::ComputeModelPreflightCacheKey(algorithmChanged),
            "algorithm version changes cache key");
}

bool ReportSkeletonMatchesGolden()
{
    const slicer_core::Json report =
        slicer_core::BuildModelPreflightReportSkeleton(MakeIdentity());

    return ExpectTrue(
               BuildStableProjection(report).dump(2) == LoadGolden().dump(2),
               "model preflight report skeleton matches golden")
        && ExpectTrue(
            !report.at("productionOutputWritten").as_bool(),
            "model preflight skeleton does not write production output");
}

bool BothModeAdmissionsCoexist()
{
    slicer_core::ModelPreflightResult result;
    result.status = slicer_core::ModelPreflightStatus::Warning;
    result.identity = MakeIdentity();
    result.cacheKey = slicer_core::ComputeModelPreflightCacheKey(result.identity);
    result.legacyAdmission.mode = slicer_core::ModelPreflightPipelineMode::Legacy;
    result.legacyAdmission.status = slicer_core::ModelPreflightAdmissionStatus::Warning;
    result.legacyAdmission.warningCodes = {"MESH_CONFIRMED_SELF_INTERSECTION"};
    result.globalAdmission.mode =
        slicer_core::ModelPreflightPipelineMode::GlobalSurfaceShell;
    result.globalAdmission.status = slicer_core::ModelPreflightAdmissionStatus::Blocked;
    result.globalAdmission.blockerCodes = {
        "E_12E_PREFLIGHT_GLOBAL_TOPOLOGY_BLOCKED"};

    slicer_core::ModelPreflightIssue issue;
    issue.code = "MESH_CONFIRMED_SELF_INTERSECTION";
    issue.category = "topology";
    issue.severity = slicer_core::ModelPreflightIssueSeverity::Error;
    issue.count = 12U;
    issue.summaryKey = "preflight.self_intersection.summary";
    issue.recommendationKey = "preflight.self_intersection.repair";
    issue.context = slicer_core::Json::object({{"caseId", "fixture"}});
    result.issues.push_back(issue);

    const slicer_core::Json report = slicer_core::BuildModelPreflightReport(result);
    return ExpectTrue(report.at("status").as_string() == "warning", "warning status is written")
        && ExpectTrue(
            report.at("legacyAdmission").at("status").as_string() == "warning",
            "legacy warning is preserved")
        && ExpectTrue(
            report.at("globalAdmission").at("status").as_string() == "blocked",
            "global blocker is preserved")
        && ExpectTrue(
            !report.at("productionOutputWritten").as_bool(),
            "contract report remains non-production");
}

}  // namespace

int main()
{
    try
    {
        const bool ok = StableNamesMatchContract()
            && CacheKeyIsStableAndSensitive()
            && ReportSkeletonMatchesGolden()
            && BothModeAdmissionsCoexist();
        if (!ok)
        {
            return 1;
        }

        std::cout << "model_preflight_contract_unit_tests: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL unexpected exception: " << error.what() << '\n';
        return 1;
    }
}
