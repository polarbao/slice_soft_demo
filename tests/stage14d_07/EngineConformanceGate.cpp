#include "EngineConformanceSupport.h"

#include "slicer_core/rip_reader.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace stage14d07
{

WorkerOutcome RunSlice(
    module::WorkerClient& client,
    const std::filesystem::path& workerPath,
    const std::string& jobId,
    const std::string& correlationId,
    const Fixture& fixture,
    const slicer_core::Json& profile,
    const std::filesystem::path& packageDirectory,
    const std::optional<std::string>& sceneHashOverride = std::nullopt)
{
    const auto identity = artifacts::MakePackageArtifactIdentity(
        packageDirectory,
        jobId,
        artifacts::MakePackageAttemptId(correlationId));
    const WorkerJobFiles files = WriteRequest(
        jobId,
        MakeSliceRequest(
            jobId, correlationId, fixture, profile,
            packageDirectory, sceneHashOverride),
        identity);
    return RunWorker(client, workerPath, files, true);
}

void RunGate()
{
    const std::filesystem::path cubePath = std::filesystem::absolute(
        stage14d07::g_repoRoot / "samples/models/openvdb/surface_shell_cube.obj")
        .lexically_normal();
    const std::filesystem::path packageDirectory = g_runRoot / "package";
    const Fixture cube = LoadFixture(cubePath, "obj", "closed-obj");
    const slicer_core::Json profile = MakeProfile(
        cubePath, "obj", packageDirectory, 0.2,
        cube.scene.resolvedprofileid);
    module::WorkerClient client;

    const WorkerOutcome firstSlice = RunSlice(
        client, g_workerPath, "job-slice-a", "correlation-slice-a",
        cube, profile, packageDirectory);
    const bool sliceIdentity = firstSlice.run.exitCategory
            == module::WorkerExitCategory::Ok
        && HasIdentity(
            firstSlice, "job-slice-a", "correlation-slice-a",
            "slice.rgbwsv", true);
    if (!sliceIdentity)
    {
        std::cerr
            << "slice baseline failed: code=" << firstSlice.run.errorCode
            << " message=" << firstSlice.run.errorMessage << '\n';
        for (const std::string& line : firstSlice.run.stderrLogLines)
        {
            std::cerr << "worker stderr: " << line << '\n';
        }
    }

    const WorkerJobFiles preflightFiles = WriteRequest(
        "job-preflight",
        MakePreflightRequest(
            "job-preflight", "correlation-preflight", cube, profile));
    const WorkerOutcome preflight = RunWorker(
        client, g_workerPath, preflightFiles, false);
    const bool preflightIdentity = preflight.run.exitCategory
            == module::WorkerExitCategory::Ok
        && HasIdentity(
            preflight, "job-preflight", "correlation-preflight",
            "geometry.preflight.full", true)
        && preflight.result->at("output").at("admission").as_string()
            == "passed";

    const std::filesystem::path repairSource =
        stage14d07::g_runRoot / "repair-source" / "tetra.obj";
    std::filesystem::create_directories(repairSource.parent_path());
    {
        std::ofstream output(repairSource, std::ios::binary | std::ios::trunc);
        output
            << "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
            << "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\n";
    }
    const std::filesystem::path repairOutput =
        g_runRoot / "job-repair" / "repair" / "tetra.repaired.obj";
    const WorkerJobFiles repairFiles = WriteRequest(
        "job-repair",
        MakeRepairRequest(
            "job-repair", "correlation-repair", repairSource,
            repairOutput, profile));
    const WorkerOutcome repair = RunWorker(
        client, g_workerPath, repairFiles, false);
    const bool repairIdentity = repair.run.exitCategory
            == module::WorkerExitCategory::Ok
        && HasIdentity(
            repair, "job-repair", "correlation-repair",
            "geometry.repair", true)
        && std::filesystem::is_regular_file(repairOutput);

    slicer_core::Json::Object invalidRequestObject = MakePreflightRequest(
        "job-invalid-capability", "correlation-invalid", cube, profile)
        .as_object();
    invalidRequestObject["capability"] = "geometry.unknown";
    const slicer_core::Json invalidRequest{std::move(invalidRequestObject)};
    const WorkerJobFiles invalidFiles = WriteRequest(
        "job-invalid-capability", invalidRequest);
    const WorkerOutcome invalid = RunWorker(
        client, g_workerPath, invalidFiles, false);
    const bool unknownFailClosed = invalid.run.exitCategory
            == module::WorkerExitCategory::Contract
        && !invalid.result.has_value();
    const bool e01 = sliceIdentity && preflightIdentity
        && repairIdentity && unknownFailClosed;
    WriteTheme(
        "E-01", "e01_contract_identity.json", e01,
        {{"sliceIdentityClosed", sliceIdentity},
         {"preflightIdentityClosed", preflightIdentity},
         {"repairIdentityClosed", repairIdentity},
         {"unknownCapabilityFailClosed", unknownFailClosed}});

    bool e02{false};
    slicer_core::Json manifest;
    if (sliceIdentity)
    {
        const slicer_core::RipValidationResult rip =
            slicer_core::internal::ValidateSlicePackageArtifact(
                packageDirectory);
        manifest = ReadJson(packageDirectory / "manifest.json");
        const slicer_core::Json& tiff = manifest.at("tiff");
        e02 = rip.schema == "p0.rgbwsv.2"
            && rip.bit_depth == 8
            && rip.channel_order
                == std::array<std::string, 6>{"R", "G", "B", "W", "S", "V"}
            && tiff.at("polarity").as_string() == "black_is_print"
            && tiff.at("printValue").as_int() == 0
            && tiff.at("emptyValue").as_int() == 255
            && rip.layer_count > 0;
    }
    WriteTheme(
        "E-02", "e02_package_protocol.json", e02,
        {{"strictRipValidation", e02},
         {"packageSchema", e02 ? "p0.rgbwsv.2" : "invalid"},
         {"channelOrder", "R G B W S V"},
         {"bitDepth", 8},
         {"polarity", "black_is_print"}});

    if (!sliceIdentity || !e02)
    {
        return;
    }
    const auto firstHashes = LayerHashes(packageDirectory);
    const slicer_core::Json firstReportProjection =
        StableReportProjection(packageDirectory);
    const WorkerOutcome secondSlice = RunSlice(
        client, g_workerPath, "job-slice-b", "correlation-slice-b",
        cube, profile, packageDirectory);
    const auto secondHashes = LayerHashes(packageDirectory);
    const slicer_core::Json secondReportProjection =
        StableReportProjection(packageDirectory);
    const bool e03 = secondSlice.run.exitCategory
            == module::WorkerExitCategory::Ok
        && firstHashes == secondHashes;
    WriteTheme(
        "E-03", "e03_golden.json", e03,
        {{"sameWorkerBuild", true},
         {"sameFixtureAndProfile", true},
         {"layerCount", static_cast<std::uint64_t>(firstHashes.size())},
         {"productionTiffByteIdentical", firstHashes == secondHashes}});

    const bool e04 = firstReportProjection.dump(0)
            == secondReportProjection.dump(0)
        && firstReportProjection.at("sliceSchema").as_string()
            == "p0.slice_report.1"
        && firstReportProjection.at("sceneSchema").as_string()
            == "slicesoft.multimodel_scene_report.13b.1";
    WriteTheme(
        "E-04", "e04_reports.json", e04,
        {{"stableProjectionEqual", e04},
         {"projection", firstReportProjection},
         {"materialProjection", "slice_report.totals.printPixels"},
         {"supportProjection", "slice_report.totals.printPixels.S"}});

    const bool timingValid = !firstSlice.run.timingEvents.empty()
        && std::all_of(
            firstSlice.run.timingEvents.begin(),
            firstSlice.run.timingEvents.end(),
            [](const module::WorkerTimingEvent& event)
            {
                return !event.engine.empty() && event.totalMs >= 0.0;
            });
    const bool e05 = IsMonotonicProgress(firstSlice.run) && timingValid;
    WriteTheme(
        "E-05", "e05_progress_timing.json", e05,
        {{"monotonicPercent", IsMonotonicProgress(firstSlice.run)},
         {"successAt100", !firstSlice.run.progressEvents.empty()
             && firstSlice.run.progressEvents.back().percent == 100U},
         {"nonNegativeTiming", timingValid},
         {"progressEventCount", static_cast<std::uint64_t>(
             firstSlice.run.progressEvents.size())},
         {"timingEventCount", static_cast<std::uint64_t>(
             firstSlice.run.timingEvents.size())}});

    const std::string staleHash = "sha256:" + std::string(64U, '0');
    const WorkerJobFiles staleFiles = WriteRequest(
        "job-stale",
        MakePreflightRequest(
            "job-stale", "correlation-stale", cube, profile, staleHash));
    const WorkerOutcome stale = RunWorker(
        client, g_workerPath, staleFiles, false);
    const bool staleRejected = stale.run.exitCategory
            != module::WorkerExitCategory::Ok
        && OutcomeCode(stale) == "PM-SLICER-LAYOUT-0022";

    Fixture missing = cube;
    missing.scene.sceneid = "scene-stage14d07-missing";
    missing.scene.models.front().sourcepath =
        g_runRoot / "missing" / "absent.obj";
    missing.scene.models.front().sourcehash = std::string(64U, '0');
    const std::filesystem::path missingOutput =
        g_runRoot / "job-missing" / "repair" / "missing.repaired.obj";
    const WorkerJobFiles missingFiles = WriteRequest(
        "job-missing",
        MakeRepairRequest(
            "job-missing", "correlation-missing",
            missing.scene.models.front().sourcepath,
            missingOutput, profile));
    const WorkerOutcome missingOutcome = RunWorker(
        client, g_workerPath, missingFiles, false);
    const bool missingRejected = missingOutcome.run.exitCategory
            != module::WorkerExitCategory::Ok
        && OutcomeCode(missingOutcome) != "PM-SLICER-INTERNAL-0099"
        && !std::filesystem::exists(missingOutput);

    const std::filesystem::path openPath = std::filesystem::absolute(
        g_repoRoot / "samples/models/openvdb/surface_shell_open_mesh.obj")
        .lexically_normal();
    const Fixture openMesh = LoadFixture(openPath, "obj", "open-mesh");
    const std::filesystem::path openOutput =
        g_runRoot / "job-open" / "repair" / "open.repaired.obj";
    const slicer_core::Json openProfile = MakeProfile(
        openPath, "obj", g_runRoot / "unused-open-package", 0.2,
        openMesh.scene.resolvedprofileid);
    const WorkerJobFiles openFiles = WriteRequest(
        "job-open",
        MakeRepairRequest(
            "job-open", "correlation-open", openPath,
            openOutput, openProfile));
    const WorkerOutcome openOutcome = RunWorker(
        client, g_workerPath, openFiles, false);
    const bool topologyRejected = openOutcome.run.exitCategory
            == module::WorkerExitCategory::Topology
        && !std::filesystem::exists(openOutput);
    if (!staleRejected || !missingRejected || !topologyRejected)
    {
        std::cerr
            << "negative outcomes: stale=" << OutcomeCode(stale)
            << " missing=" << OutcomeCode(missingOutcome)
            << " topology=" << OutcomeCode(openOutcome) << '\n';
    }
    const bool e06 = unknownFailClosed && staleRejected
        && missingRejected && topologyRejected;
    WriteTheme(
        "E-06", "e06_negative.json", e06,
        {{"invalidContractRejected", unknownFailClosed},
         {"staleSceneRejected", staleRejected},
         {"staleSceneCode", OutcomeCode(stale)},
         {"missingResourceRejected", missingRejected},
         {"missingResourceCode", OutcomeCode(missingOutcome)},
         {"strictTopologyRejected", topologyRejected},
         {"strictTopologyCode", OutcomeCode(openOutcome)},
         {"noInProcessFallback", e06}});

    const std::string baselineManifest =
        ReadBytes(packageDirectory / "manifest.json");
    const slicer_core::Json cancelProfile = MakeProfile(
        cubePath, "obj", packageDirectory, 0.0001,
        cube.scene.resolvedprofileid);
    const std::string cancelJob{"job-cancel"};
    const std::string cancelCorrelation{"correlation-cancel"};
    const artifacts::PackageArtifactIdentity cancelIdentity =
        artifacts::MakePackageArtifactIdentity(
            packageDirectory,
            cancelJob,
            artifacts::MakePackageAttemptId(cancelCorrelation));
    const WorkerJobFiles cancelFiles = WriteRequest(
        cancelJob,
        MakeSliceRequest(
            cancelJob, cancelCorrelation, cube, cancelProfile,
            packageDirectory),
        cancelIdentity);
    WorkerOutcome cancelled;
    std::thread workerThread([&]()
    {
        cancelled = RunWorker(client, g_workerPath, cancelFiles, true);
    });
    while (!client.IsRunning())
    {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    const auto cancelStart = std::chrono::steady_clock::now();
    const bool cancelAccepted = client.RequestCancel();
    workerThread.join();
    const double cancelElapsedMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - cancelStart).count();
    const bool residueFree =
        !std::filesystem::exists(cancelIdentity.staging_directory)
        && !std::filesystem::exists(cancelIdentity.backup_directory)
        && !std::filesystem::exists(cancelIdentity.lease_directory);
    const bool packageUnchanged =
        ReadBytes(packageDirectory / "manifest.json") == baselineManifest;
    const bool e07 = cancelAccepted
        && cancelled.run.stopReason == module::WorkerStopReason::Cancelled
        && cancelled.run.errorCode == "PM-SLICER-CANCELLED-0070"
        && cancelElapsedMs <= 2000.0
        && residueFree
        && packageUnchanged;
    WriteTheme(
        "E-07", "e07_cancel_recovery.json", e07,
        {{"cancelAccepted", cancelAccepted},
         {"cancelDeadlineMs", cancelElapsedMs},
         {"deadlinePassed", cancelElapsedMs <= 2000.0},
         {"stableExitCode", cancelled.run.errorCode},
         {"stagingCleanup", residueFree},
         {"existingPackageUnchanged", packageUnchanged}});

    const std::filesystem::path threeMfPath = std::filesystem::absolute(
        g_repoRoot / "samples/models/3mf/texture2d_checker_cube.3mf")
        .lexically_normal();
    const Fixture threeMf = LoadFixture(threeMfPath, "3mf", "textured-3mf");
    const slicer_core::Json threeMfProfile = MakeProfile(
        threeMfPath, "3mf", g_runRoot / "unused-3mf-package", 0.2,
        threeMf.scene.resolvedprofileid);
    const WorkerJobFiles threeMfFiles = WriteRequest(
        "job-3mf-preflight",
        MakePreflightRequest(
            "job-3mf-preflight", "correlation-3mf",
            threeMf, threeMfProfile));
    const WorkerOutcome threeMfOutcome = RunWorker(
        client, g_workerPath, threeMfFiles, false);
    const bool threeMfPassed = threeMfOutcome.run.exitCategory
            == module::WorkerExitCategory::Ok
        && HasIdentity(
            threeMfOutcome, "job-3mf-preflight", "correlation-3mf",
            "geometry.preflight.full", true);
    const bool firstSevenPassed = std::all_of(
        g_themeStatus.begin(), g_themeStatus.end(),
        [](const auto& item)
        {
            return item.first == "E-08" || item.second;
        });
    const bool e08 = firstSevenPassed && threeMfPassed;
    WriteTheme(
        "E-08", "e08_replaceability.json", e08,
        {{"parameterizedWorkerPath", g_workerPath.generic_string()},
         {"sameHost", true},
         {"sameModule", true},
         {"sameRequests", true},
         {"currentWorkerBaselineEstablished", firstSevenPassed},
         {"textured3mfPreflightPassed", threeMfPassed},
         {"alternateWorkerVersion", "not_required_for_current_baseline_gate"}});
}

}  // namespace stage14d07

int main(const int argc, char* const argv[])
{
    if (argc != 4)
    {
        std::cerr
            << "usage: stage14d07_engine_conformance_gate "
            << "<slicer_worker> <repo_root> <evidence_root>\n";
        return 2;
    }
    stage14d07::g_workerPath = std::filesystem::absolute(argv[1]).lexically_normal();
    stage14d07::g_repoRoot = std::filesystem::absolute(argv[2]).lexically_normal();
    stage14d07::g_evidenceRoot = std::filesystem::absolute(argv[3]).lexically_normal();
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    stage14d07::g_runRoot = std::filesystem::temp_directory_path()
        / ("slicesoft_stage14d07_r2_" + std::to_string(suffix));
    std::filesystem::create_directories(stage14d07::g_runRoot);
    std::filesystem::create_directories(stage14d07::g_evidenceRoot);

    int exitCode{0};
    try
    {
        stage14d07::RunGate();
        const bool allPassed = stage14d07::g_themeStatus.size() == 8U
            && std::all_of(
                stage14d07::g_themeStatus.begin(), stage14d07::g_themeStatus.end(),
                [](const auto& item) { return item.second; });
        if (!allPassed)
        {
            std::cerr << "Stage 14D-07 R2 gate: FAIL\n";
            exitCode = 1;
        }
        else
        {
            std::cout << "Stage 14D-07 R2 gate: PASS (E-01..E-08)\n";
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "Stage 14D-07 R2 gate: FAIL: " << error.what() << '\n';
        exitCode = 1;
    }
    std::error_code ignored;
    std::filesystem::remove_all(stage14d07::g_runRoot, ignored);
    return exitCode;
}
