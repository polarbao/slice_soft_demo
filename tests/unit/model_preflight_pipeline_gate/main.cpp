#include "slicer_core/pipeline/ModelPreflightGate.h"

#include "slicer_core/pipeline/OpenVdbCandidatePipeline.h"
#include "slicer_core/pipeline/SlicePipeline.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
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

std::filesystem::path MakeTestDirectory()
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / ("slicesoft_preflight_gate_" + std::to_string(suffix));
    std::filesystem::create_directories(directory);
    return directory;
}

void WriteText(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream output{path, std::ios::binary};
    if (!output)
    {
        throw std::runtime_error("failed to write fixture: " + path.string());
    }
    output << content;
}

std::filesystem::path WriteConfig(
    const std::filesystem::path& directory,
    const std::filesystem::path& modelPath,
    const std::filesystem::path& packagePath)
{
    const std::filesystem::path configPath = directory / "slice.json";
    WriteText(
        configPath,
        "{\n"
        "  \"input\": {\"modelPath\": \""
            + modelPath.generic_string()
            + "\", \"format\": \"obj\"},\n"
              "  \"output\": {\"packageDir\": \""
            + packagePath.generic_string()
            + "\"},\n"
              "  \"modelTransform\": {\"unit\": \"mm\", \"scale\": [1, 1, 1], "
              "\"rotationDeg\": [0, 0, 0], \"translationMm\": [0, 0, 0]}\n"
              "}\n");
    return configPath;
}

slicer_core::ModelPreflightExecutionResult MakeExecution(
    const slicer_core::ModelPreflightStatus status = slicer_core::ModelPreflightStatus::Passed)
{
    slicer_core::ModelPreflightExecutionResult execution;
    execution.fastComplete = true;
    execution.fullComplete = status == slicer_core::ModelPreflightStatus::Passed
        || status == slicer_core::ModelPreflightStatus::Warning;
    execution.result.status = status;
    execution.result.cacheKey = "gate-cache-key";
    return execution;
}

slicer_core::ModelPreflightIssue TopologyIssue()
{
    slicer_core::ModelPreflightIssue issue;
    issue.code = "MESH_SELF_INTERSECTION_CONFIRMED";
    issue.category = "topology";
    issue.severity = slicer_core::ModelPreflightIssueSeverity::Error;
    issue.count = 1U;
    return issue;
}

slicer_core::ModelPreflightAdmissionContext AvailableContext()
{
    slicer_core::ModelPreflightAdmissionContext context;
    context.global_backend_available = true;
    return context;
}

bool ModeGateInvokesOnlyAdmittedAction()
{
    slicer_core::ModelPreflightExecutionResult execution = MakeExecution(
        slicer_core::ModelPreflightStatus::Warning);
    execution.result.issues.push_back(TopologyIssue());

    int legacyCalls{0};
    const auto legacy = slicer_core::ExecuteModelPreflightPipelineGate(
        execution,
        slicer_core::ModelPreflightPipelineMode::Legacy,
        AvailableContext(),
        [&](const slicer_core::ModelPreflightGateResult&)
        {
            ++legacyCalls;
        });

    int globalCalls{0};
    const auto global = slicer_core::ExecuteModelPreflightPipelineGate(
        execution,
        slicer_core::ModelPreflightPipelineMode::GlobalSurfaceShell,
        AvailableContext(),
        [&](const slicer_core::ModelPreflightGateResult&)
        {
            ++globalCalls;
        });

    return ExpectTrue(legacy.pipeline_allowed, "legacy warning is admitted")
        && ExpectTrue(legacy.action_invoked, "legacy action is invoked")
        && ExpectTrue(legacyCalls == 1, "legacy runner executes once")
        && ExpectTrue(!global.pipeline_allowed, "global topology is blocked")
        && ExpectTrue(!global.action_invoked, "global action is not invoked")
        && ExpectTrue(globalCalls == 0, "global core and writer callback stay at zero");
}

bool BackendAndLifecycleAreFailClosed()
{
    slicer_core::ModelPreflightAdmissionContext unavailable;
    unavailable.global_backend_available = false;

    int backendCalls{0};
    const auto backend = slicer_core::ExecuteModelPreflightPipelineGate(
        MakeExecution(),
        slicer_core::ModelPreflightPipelineMode::GlobalSurfaceShell,
        unavailable,
        [&](const slicer_core::ModelPreflightGateResult&)
        {
            ++backendCalls;
        });

    int staleCalls{0};
    slicer_core::ModelPreflightExecutionResult stale = MakeExecution(
        slicer_core::ModelPreflightStatus::Stale);
    stale.stale = true;
    const auto staleGate = slicer_core::ExecuteModelPreflightPipelineGate(
        stale,
        slicer_core::ModelPreflightPipelineMode::Legacy,
        AvailableContext(),
        [&](const slicer_core::ModelPreflightGateResult&)
        {
            ++staleCalls;
        });

    return ExpectTrue(!backend.pipeline_allowed, "backend unavailable blocks global")
        && ExpectTrue(backendCalls == 0, "backend blocker invokes no action")
        && ExpectTrue(!staleGate.pipeline_allowed, "stale result blocks legacy")
        && ExpectTrue(staleCalls == 0, "stale result invokes no action");
}

bool ServiceGateReusesFreshDiagnostics()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = directory / "box.obj";
    WriteText(
        modelPath,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "v 0 0 1\n"
        "v 1 0 1\n"
        "v 1 1 1\n"
        "v 0 1 1\n"
        "f 1 3 2\n"
        "f 1 4 3\n"
        "f 5 6 7\n"
        "f 5 7 8\n"
        "f 1 2 6\n"
        "f 1 6 5\n"
        "f 2 3 7\n"
        "f 2 7 6\n"
        "f 3 4 8\n"
        "f 3 8 7\n"
        "f 4 1 5\n"
        "f 4 5 8\n");
    const std::filesystem::path configPath = WriteConfig(
        directory,
        modelPath,
        directory / "package");

    slicer_core::ModelPreflightService service;
    slicer_core::ModelPreflightGateRequest request;
    request.preflight_request.configPath = configPath;
    request.selected_mode = slicer_core::ModelPreflightPipelineMode::Legacy;
    request.admission_context = AvailableContext();

    int calls{0};
    const auto first = slicer_core::RunModelPreflightPipelineGate(
        service,
        request,
        [&](const slicer_core::ModelPreflightGateResult&)
        {
            ++calls;
        });
    const auto second = slicer_core::RunModelPreflightPipelineGate(
        service,
        request,
        [&](const slicer_core::ModelPreflightGateResult&)
        {
            ++calls;
        });

    return ExpectTrue(first.pipeline_allowed, "fresh clean model is admitted")
        && ExpectTrue(second.preflight.cacheHit, "second gate reuses service cache")
        && ExpectTrue(calls == 2, "each admitted request invokes exactly one action");
}

bool BlockedCandidateCreatesNoOutput()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = directory / "open.obj";
    const std::filesystem::path packagePath = directory / "package";
    WriteText(
        modelPath,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n");
    const std::filesystem::path configPath = WriteConfig(directory, modelPath, packagePath);

    slicer_core::OpenVdbCandidatePipelineOptions options;
    options.write_tiff_layers = true;
    options.write_preview_files = true;
    options.write_reports = true;
    options.publish_package = true;
    try
    {
        (void)slicer_core::RunOpenVdbCandidatePipeline(configPath, options);
    }
    catch (const std::exception& error)
    {
        const std::string message{error.what()};
        return ExpectTrue(
                   message.find("E_12E_PREFLIGHT_GLOBAL_TOPOLOGY_BLOCKED")
                       != std::string::npos,
                   "candidate is rejected by the R4 global topology gate")
            && ExpectTrue(
                !std::filesystem::exists(packagePath),
                "blocked candidate creates no package or writer output");
    }
    return ExpectTrue(false, "blocked candidate must not enter the candidate pipeline");
}

bool BlockedLegacyCreatesNoOutput()
{
    const std::filesystem::path directory = MakeTestDirectory();
    const std::filesystem::path modelPath = directory / "non_finite.obj";
    const std::filesystem::path packagePath = directory / "package";
    WriteText(
        modelPath,
        "v 0 0 0\n"
        "v nan 1 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n");
    const std::filesystem::path configPath = WriteConfig(directory, modelPath, packagePath);

    try
    {
        (void)slicer_core::RunSlicePipelineLegacy(
            configPath,
            slicer_core::SliceRunOptions{});
    }
    catch (const std::exception& error)
    {
        const std::string message{error.what()};
        return ExpectTrue(
                   message.find("E_12E_PREFLIGHT_NON_FINITE_GEOMETRY")
                       != std::string::npos,
                   "legacy is rejected by the shared fatal gate")
            && ExpectTrue(
                !std::filesystem::exists(packagePath),
                "blocked legacy input creates no package or writer output");
    }
    return ExpectTrue(false, "blocked legacy input must not enter the legacy pipeline");
}

bool FailureMessagePreservesImporterDetail()
{
    slicer_core::ModelPreflightExecutionResult execution = MakeExecution(
        slicer_core::ModelPreflightStatus::Blocked);
    slicer_core::ModelPreflightIssue issue;
    issue.code = slicer_core::ModelPreflightErrorCodeName(
        slicer_core::ModelPreflightErrorCode::ImportInvalid);
    issue.category = "import";
    issue.severity = slicer_core::ModelPreflightIssueSeverity::Error;
    issue.count = 1U;
    issue.context = slicer_core::Json::object({
        {"detail", "E_3MF_CONTENT_TYPES_MISSING: fixture detail"},
    });
    execution.result.issues.push_back(issue);

    const auto gate = slicer_core::ExecuteModelPreflightPipelineGate(
        execution,
        slicer_core::ModelPreflightPipelineMode::Legacy,
        AvailableContext(),
        {});
    const std::string message = slicer_core::FormatModelPreflightGateFailure(gate);
    return ExpectTrue(
        message.find("E_3MF_CONTENT_TYPES_MISSING") != std::string::npos,
        "gate failure preserves importer-specific stable detail");
}

}  // namespace

int main()
{
    const std::vector<TestCase> tests{
        {"mode_gate_invokes_only_admitted_action", ModeGateInvokesOnlyAdmittedAction},
        {"backend_and_lifecycle_are_fail_closed", BackendAndLifecycleAreFailClosed},
        {"service_gate_reuses_fresh_diagnostics", ServiceGateReusesFreshDiagnostics},
        {"blocked_candidate_creates_no_output", BlockedCandidateCreatesNoOutput},
        {"blocked_legacy_creates_no_output", BlockedLegacyCreatesNoOutput},
        {"failure_message_preserves_importer_detail", FailureMessagePreservesImporterDetail},
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
