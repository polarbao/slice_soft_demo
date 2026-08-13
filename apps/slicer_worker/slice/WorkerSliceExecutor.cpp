#include "slicer_worker/slice/WorkerSliceExecutor.h"

#include "slicer_worker/slice/WorkerSliceRequestMaterializer.h"

#include "slicer_core/engine/ProductionPreflightFullFacadeFactory.h"
#include "slicer_core/engine/ProductionSliceFacadeFactory.h"
#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_core/rip_reader.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace slicesoft::worker
{
namespace
{

constexpr const char* kCancelledCode{"PM-SLICER-CANCELLED-0070"};
constexpr const char* kContractCode{"PM-SLICER-CONTRACT-0060"};
constexpr const char* kInternalCode{"PM-SLICER-INTERNAL-0099"};
constexpr const char* kLayoutCollisionCode{"PM-SLICER-LAYOUT-0020"};
constexpr const char* kLayoutBoundsCode{"PM-SLICER-LAYOUT-0021"};
constexpr const char* kLayoutStaleCode{"PM-SLICER-LAYOUT-0022"};
constexpr const char* kOutputCode{"PM-SLICER-OUTPUT-0050"};
constexpr const char* kTopologyCode{"PM-SLICER-TOPOLOGY-0010"};

using WorkerClock = std::chrono::steady_clock;

double ElapsedMilliseconds(const WorkerClock::time_point start)
{
    return std::chrono::duration<double, std::milli>(
        WorkerClock::now() - start).count();
}

slicer_core::Json OptionalMilliseconds(
    const std::optional<double>& value)
{
    return value.has_value()
        ? slicer_core::Json{value.value()}
        : slicer_core::Json{nullptr};
}

slicer_core::Json BuildImportTimings(
    const slicer_core::SliceRunProfile& profile)
{
    slicer_core::Json::Array values;
    values.reserve(profile.imports.size());
    for (const slicer_core::SliceRunImportProfile& item : profile.imports)
    {
        values.push_back(slicer_core::Json::object({
            {"modelId", item.modelid},
            {"sourcePath", item.sourcepath},
            {"parseBoundary", item.parseboundary},
            {"parseMs", OptionalMilliseconds(item.parsems)},
            {"textureMs", OptionalMilliseconds(item.texturems)},
            {"previewMs", OptionalMilliseconds(item.previewms)},
            {"hashMs", OptionalMilliseconds(item.hashms)},
        }));
    }
    return slicer_core::Json{std::move(values)};
}

slicer_core::Json BuildInstanceTimings(
    const slicer_core::SliceRunProfile& profile)
{
    slicer_core::Json::Array values;
    values.reserve(profile.instances.size());
    for (const slicer_core::SliceRunInstanceProfile& item : profile.instances)
    {
        values.push_back(slicer_core::Json::object({
            {"modelId", item.modelid},
            {"instanceId", item.instanceid},
            {"widthPx", item.widthpx},
            {"heightPx", item.heightpx},
            {"layerCount", item.layercount},
            {"coreSliceMs", OptionalMilliseconds(item.coreslicems)},
            {"composeMs", OptionalMilliseconds(item.composems)},
            {"totalMs", OptionalMilliseconds(item.totalms)},
        }));
    }
    return slicer_core::Json{std::move(values)};
}

WorkerCapabilityExecutionResult FacadeFailure(
    const slicer_core::api::ApiError* error,
    const std::string& fallbackMessage,
    const bool cancelled)
{
    const std::string code = error != nullptr && !error->code.empty()
        ? error->code
        : kInternalCode;
    return WorkerCapabilityExecutionResult::Failure(
        code,
        error != nullptr && !error->message.empty()
            ? error->message
            : fallbackMessage,
        error != nullptr && !error->detail.empty()
            ? std::optional<std::string>(error->detail)
            : std::nullopt,
        cancelled
            ? std::optional<WorkerResultCleanup>(
                  WorkerResultCleanup{true, false})
            : std::nullopt);
}

bool IsValidPublishedPackage(const std::filesystem::path& packageDirectory)
{
    try
    {
        (void)slicer_core::internal::ValidateSlicePackageArtifact(
            packageDirectory);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

WorkerCapabilityExecutionResult ArtifactCleanupFailure(
    const std::string& phase,
    const slicer_core::api::artifacts::PackageArtifactRecoveryResult& recovery)
{
    return WorkerCapabilityExecutionResult::Failure(
        kOutputCode,
        "Worker package artifact cleanup failed during " + phase,
        recovery.error.empty()
            ? std::nullopt
            : std::optional<std::string>(recovery.error),
        WorkerResultCleanup{recovery.staging_removed, false});
}

std::string AdmissionFailureCode(
    const slicer_core::api::PreflightResult& preflight)
{
    if (!preflight.collisions.empty())
    {
        return kLayoutCollisionCode;
    }
    if (preflight.out_of_bounds)
    {
        return kLayoutBoundsCode;
    }
    return kTopologyCode;
}

std::optional<std::string> AdmissionFailureDetail(
    const slicer_core::api::PreflightResult& preflight)
{
    if (!preflight.issues.empty())
    {
        const slicer_core::api::PreflightIssue& issue =
            preflight.issues.front();
        return issue.detail.empty()
            ? std::optional<std::string>{issue.code}
            : std::optional<std::string>{issue.code + ": " + issue.detail};
    }
    if (!preflight.out_of_bounds_instances.empty())
    {
        return "out_of_bounds instance="
            + preflight.out_of_bounds_instances.front();
    }
    return std::nullopt;
}

slicer_core::Json BuildBasicOutput(
    const slicer_core::api::SliceResult& result,
    const WorkerSliceMaterialization& materialized)
{
    const slicer_core::SliceRunProfile& profile = result.profile;
    return slicer_core::Json::object({
        {"packageDir", result.package_dir.generic_string()},
        {"manifestPath", result.manifest_path.generic_string()},
        {"layerCount", result.layer_count},
        {"grid", slicer_core::Json::object({
            {"widthPx", result.grid_px[0]},
            {"heightPx", result.grid_px[1]},
            {"dpiX", materialized.DpiX()},
            {"dpiY", materialized.DpiY()},
        })},
        {"profileEcho", slicer_core::Json::object({
            {"profileVersion", materialized.ProfileVersion()},
            {"profileHash", materialized.ProfileHash()},
        })},
        {"engineVersion", result.engine_version},
        {"elapsedMs", static_cast<double>(result.elapsed_ms)},
        {"timing", slicer_core::Json::object({
            {"available", profile.available},
            {"source", "worker_core"},
            {"profileLevel", profile.profile_level},
            {"engine", result.engine_version},
            {"configLoadMs", profile.config_load_ms},
            {"modelLoadMs", profile.model_load_ms},
            {"gridSetupMs", profile.grid_setup_ms},
            {"maskSamplingMs", profile.mask_sampling_ms},
            {"texturePrepareMs", profile.texture_prepare_ms},
            {"supportGenerationMs", profile.support_generation_ms},
            {"supportStatisticsScanCount",
             profile.support_statistics_scan_count},
            {"sliceProcessingMs", profile.slice_processing_ms},
            {"layerComputeMs", profile.layer_compute_ms},
            {"layerComposeMs", profile.layer_compose_ms},
            {"tiffWriteMs", profile.tiff_write_ms},
            {"previewWriteMs", profile.preview_write_ms},
            {"reportBuildMs", profile.report_build_ms},
            {"reportWriteMs", profile.report_write_ms},
            {"packagePublishMs", profile.package_publish_ms},
            {"outputWriteMs", profile.output_write_ms},
            {"totalMs", profile.total_ms},
            {"imports", BuildImportTimings(profile)},
            {"instances", BuildInstanceTimings(profile)},
        })},
        {"executionScope", "14D-08-R2-02"},
    });
}

}  // namespace

WorkerSliceExecutor::WorkerSliceExecutor(
    std::unique_ptr<slicer_core::api::PreflightFullFacade> preflightFacade,
    std::unique_ptr<slicer_core::api::SliceFacade> sliceFacade,
    std::ostream& protocolOutput)
    : m_preflightFacade(std::move(preflightFacade)),
      m_sliceFacade(std::move(sliceFacade)),
      m_protocolOutput(&protocolOutput)
{
    if (m_preflightFacade == nullptr || m_sliceFacade == nullptr)
    {
        throw std::invalid_argument(
            "Worker slice executor requires production preflight and slice facades");
    }
}

WorkerCapabilityExecutionResult WorkerSliceExecutor::Execute(
    const WorkerRequestEnvelope& request,
    const slicer_core::api::ICancelToken& cancelToken)
{
    const WorkerClock::time_point start = WorkerClock::now();
    std::optional<slicer_core::api::artifacts::PackageArtifactIdentity>
        artifactIdentity;
    const auto finalize = [&artifactIdentity](
        WorkerCapabilityExecutionResult result)
    {
        if (!artifactIdentity.has_value())
        {
            return result;
        }
        const auto recovery =
            slicer_core::api::artifacts::RecoverPackageArtifacts(
                *artifactIdentity,
                IsValidPublishedPackage);
        if (!recovery.success)
        {
            return ArtifactCleanupFailure("Worker exit", recovery);
        }
        return result;
    };
    try
    {
        const WorkerSliceMaterialization materialized =
            WorkerSliceRequestMaterializer::Materialize(request, cancelToken);
        artifactIdentity =
            slicer_core::api::artifacts::MakePackageArtifactIdentity(
                materialized.PackageDirectory(),
                request.Identity().JobId(),
                slicer_core::api::artifacts::MakePackageAttemptId(
                    request.Identity().CorrelationId()));
        const auto startupRecovery =
            slicer_core::api::artifacts::RecoverPackageArtifacts(
                *artifactIdentity,
                IsValidPublishedPackage);
        if (!startupRecovery.success)
        {
            return ArtifactCleanupFailure("Worker startup", startupRecovery);
        }

        slicer_core::api::PreflightRequest preflightRequest;
        preflightRequest.scene_config_path = materialized.SceneSnapshotPath();
        preflightRequest.profile_config_path = materialized.ProfilePath();
        preflightRequest.scene_hash = materialized.SceneHash();
        preflightRequest.profile_hash = materialized.ProfileHash();
        preflightRequest.expected_scene_revision = materialized.SceneRevision();
        preflightRequest.target_mode = materialized.TargetMode();
        preflightRequest.authoritative = true;
        const slicer_core::api::ApiResult<slicer_core::api::PreflightResult>
            preflight = m_preflightFacade->RunFull(
                preflightRequest, cancelToken);
        if (!preflight.IsOk())
        {
            const slicer_core::api::ApiError* error = preflight.Error();
            return finalize(FacadeFailure(
                error,
                "authoritative preflight failed before slicing",
                error != nullptr && error->code == kCancelledCode));
        }
        if (preflight.Value() == nullptr)
        {
            return finalize(WorkerCapabilityExecutionResult::Failure(
                kInternalCode,
                "authoritative preflight returned no result"));
        }
        const slicer_core::api::PreflightResult& admission = *preflight.Value();
        if (admission.cancelled || cancelToken.IsCancelRequested())
        {
            return finalize(WorkerCapabilityExecutionResult::Failure(
                kCancelledCode,
                "slice job was cancelled after authoritative preflight",
                std::nullopt,
                WorkerResultCleanup{true, false}));
        }
        if (!admission.authoritative
            || !admission.complete
            || !admission.admitted)
        {
            return finalize(WorkerCapabilityExecutionResult::Failure(
                AdmissionFailureCode(admission),
                "authoritative full preflight blocked production slicing",
                AdmissionFailureDetail(admission)));
        }
        if (!materialized.ProductionAdmissionCommitted())
        {
            return finalize(WorkerCapabilityExecutionResult::Failure(
                kLayoutStaleCode,
                "committed scene has no passing production admission"));
        }

        int lastPercent{-1};
        const slicer_core::api::ProgressSink progress =
            [this, start, &lastPercent](
                const slicer_core::api::ProgressEvent& event)
            {
                if (m_protocolOutput == nullptr)
                {
                    return;
                }
                const int percent = std::clamp(event.percent, 0, 100);
                if (percent < lastPercent)
                {
                    return;
                }
                lastPercent = percent;
                *m_protocolOutput
                    << "SLICE_PROGRESS phase=" << event.stage
                    << " current=" << std::max(0, event.layers_done)
                    << " total=" << std::max(0, event.layers_total)
                    << " percent=" << percent
                    << " elapsedMs=" << std::fixed << std::setprecision(3)
                    << ElapsedMilliseconds(start) << '\n';
                m_protocolOutput->flush();
            };

        slicer_core::api::SliceRequest sliceRequest;
        sliceRequest.job_id = request.Identity().JobId();
        sliceRequest.correlation_id = request.Identity().CorrelationId();
        sliceRequest.scene_hash = materialized.SceneHash();
        sliceRequest.scene_config_path = materialized.SceneConfigPath();
        sliceRequest.package_dir = materialized.PackageDirectory();
        const slicer_core::api::ApiResult<slicer_core::api::SliceResult> sliced =
            m_sliceFacade->Run(sliceRequest, cancelToken, progress);
        if (!sliced.IsOk())
        {
            const slicer_core::api::ApiError* error = sliced.Error();
            return finalize(FacadeFailure(
                error,
                "production SliceFacade failed without an error",
                error != nullptr && error->code == kCancelledCode));
        }
        if (sliced.Value() == nullptr)
        {
            return finalize(WorkerCapabilityExecutionResult::Failure(
                kInternalCode,
                "production SliceFacade returned no package result"));
        }
        const slicer_core::api::SliceResult& result = *sliced.Value();
        if (result.package_dir != materialized.PackageDirectory()
            || result.manifest_path
                != materialized.PackageDirectory() / "manifest.json"
            || !std::filesystem::is_regular_file(result.manifest_path)
            || result.layer_count <= 0
            || result.grid_px[0] <= 0
            || result.grid_px[1] <= 0)
        {
            return finalize(WorkerCapabilityExecutionResult::Failure(
                kContractCode,
                "production SliceFacade returned incomplete package evidence"));
        }
        if (m_protocolOutput != nullptr)
        {
            if (lastPercent < 100)
            {
                *m_protocolOutput
                    << "SLICE_PROGRESS phase=completed current="
                    << result.layer_count
                    << " total=" << result.layer_count
                    << " percent=100 elapsedMs=" << std::fixed
                    << std::setprecision(3) << ElapsedMilliseconds(start)
                    << '\n';
            }
            *m_protocolOutput
                << "SLICE_TIMING engine=" << result.engine_version
                << " profileLevel=" << result.profile.profile_level
                << std::fixed << std::setprecision(3)
                << " configLoadMs=" << result.profile.config_load_ms
                << " modelLoadMs=" << result.profile.model_load_ms
                << " gridSetupMs=" << result.profile.grid_setup_ms
                << " sliceProcessingMs="
                << result.profile.slice_processing_ms
                << " layerComputeMs=" << result.profile.layer_compute_ms
                << " layerComposeMs=" << result.profile.layer_compose_ms
                << " tiffWriteMs=" << result.profile.tiff_write_ms
                << " previewWriteMs=" << result.profile.preview_write_ms
                << " reportBuildMs=" << result.profile.report_build_ms
                << " reportWriteMs=" << result.profile.report_write_ms
                << " packagePublishMs="
                << result.profile.package_publish_ms
                << " outputWriteMs=" << result.profile.output_write_ms
                << " totalMs=" << ElapsedMilliseconds(start)
                << " workingSetBytes=0 peakWorkingSetBytes=0\n";
            m_protocolOutput->flush();
        }
        return finalize(WorkerCapabilityExecutionResult::Success(
            BuildBasicOutput(result, materialized)));
    }
    catch (const WorkerSliceRequestMaterializationError& error)
    {
        return finalize(WorkerCapabilityExecutionResult::Failure(
            error.Code(),
            error.what(),
            std::nullopt,
            error.Code() == kCancelledCode
                ? std::optional<WorkerResultCleanup>(
                      WorkerResultCleanup{true, false})
                : std::nullopt));
    }
    catch (const std::exception& error)
    {
        return finalize(WorkerCapabilityExecutionResult::Failure(
            kInternalCode,
            "unexpected slice Worker executor failure",
            std::string(error.what())));
    }
}

std::unique_ptr<IWorkerCapabilityExecutor>
CreateProductionWorkerSliceExecutor(std::ostream& protocolOutput)
{
    return std::make_unique<WorkerSliceExecutor>(
        slicer_core::engine::CreateProductionPreflightFullFacade(),
        slicer_core::engine::CreateProductionSliceFacade(),
        protocolOutput);
}

}  // namespace slicesoft::worker
