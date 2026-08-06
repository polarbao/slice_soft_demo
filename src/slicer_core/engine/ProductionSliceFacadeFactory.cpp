#include "slicer_core/engine/ProductionSliceFacadeFactory.h"

#include "slicer_core/api/artifacts/PackageArtifactSafety.h"
#include "slicer_core/engine/SliceFacadeAdapter.h"
#include "slicer_core/json_value.h"
#include "slicer_core/pipeline/MultiModelProductionService.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace slicer_core::engine
{
namespace
{

api::ApiError MakeError(
    std::string code,
    std::string message,
    std::string detail = {})
{
    api::ApiError error;
    error.code = std::move(code);
    error.message = std::move(message);
    error.detail = std::move(detail);
    return error;
}

std::filesystem::path ResolvePath(
    const std::filesystem::path& path,
    const std::filesystem::path& baseDirectory)
{
    if (path.empty())
    {
        return {};
    }
    const std::filesystem::path candidate =
        path.is_absolute() ? path : baseDirectory / path;
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(candidate, error);
    return (error ? candidate : absolute).lexically_normal();
}

api::ApiResult<SliceSubmissionContract> ResolveSubmissionContract(
    const std::filesystem::path& effectiveConfigPath)
{
    const SceneEffectiveConfigResult effective =
        ReadSceneEffectiveConfig(effectiveConfigPath);
    if (!effective.IsValid())
    {
        return api::ApiResult<SliceSubmissionContract>::Failure(
            MakeError(
                "PM-SLICER-PROFILE-0030",
                "scene effective config is invalid",
                effective.error.has_value()
                    ? effective.error->message
                    : effectiveConfigPath.generic_string()));
    }

    try
    {
        SliceSubmissionContract contract;
        contract.scenehash = effective.document
                                 .at("identity")
                                 .at("sceneHash")
                                 .as_string();
        contract.packagedir = ResolvePath(
            effective.document
                .at("sliceContract")
                .at("outputPackageDir")
                .as_string(),
            effectiveConfigPath.parent_path());
        if (contract.scenehash.empty()
            || contract.packagedir.empty())
        {
            throw std::runtime_error(
                "sceneHash or outputPackageDir is empty");
        }
        return api::ApiResult<SliceSubmissionContract>::Success(
            std::move(contract));
    }
    catch (const std::exception& exception)
    {
        return api::ApiResult<SliceSubmissionContract>::Failure(
            MakeError(
                "PM-SLICER-PROFILE-0030",
                "scene effective config lacks the production contract",
                exception.what()));
    }
}

api::ApiError MapProductionError(
    const MultiModelProductionResult& result)
{
    const MultiModelProductionErrorCode code =
        result.error.has_value()
        ? result.error->code
        : MultiModelProductionErrorCode::ProductionPackageInvalid;
    std::string pmCode;
    switch (code)
    {
    case MultiModelProductionErrorCode::EffectiveConfigInvalid:
        pmCode = "PM-SLICER-PROFILE-0030";
        break;
    case MultiModelProductionErrorCode::EffectiveConfigStale:
        pmCode = "PM-SLICER-LAYOUT-0022";
        break;
    case MultiModelProductionErrorCode::ResourceUnresolved:
        pmCode = "PM-SLICER-INPUT-0001";
        break;
    case MultiModelProductionErrorCode::ProfileMismatch:
    case MultiModelProductionErrorCode::BuildVolumeUndefined:
    case MultiModelProductionErrorCode::PipelineModeNotAdmitted:
        pmCode = "PM-SLICER-PROFILE-0031";
        break;
    case MultiModelProductionErrorCode::ProductionPackageInvalid:
        pmCode = "PM-SLICER-CONTRACT-0060";
        break;
    case MultiModelProductionErrorCode::OutputPublicationFailed:
        pmCode = "PM-SLICER-OUTPUT-0050";
        break;
    case MultiModelProductionErrorCode::PackageTargetBusy:
        pmCode = "PM-SLICER-RESOURCE-0041";
        break;
    case MultiModelProductionErrorCode::Cancelled:
        pmCode = "PM-SLICER-CANCELLED-0070";
        break;
    case MultiModelProductionErrorCode::None:
        pmCode = "PM-SLICER-INTERNAL-0099";
        break;
    }

    std::string detail = std::string(
        MultiModelProductionErrorCodeName(code));
    if (result.error.has_value())
    {
        detail += "; field=" + result.error->field;
        detail += "; sceneId=" + result.error->sceneid;
        detail += "; modelId=" + result.error->modelid;
        detail += "; instanceId=" + result.error->instanceid;
    }
    return MakeError(
        std::move(pmCode),
        result.error.has_value()
            ? result.error->message
            : "scene production failed",
        std::move(detail));
}

api::ApiResult<api::SliceResult> RunExistingProductionEntry(
    const api::SliceRequest& sliceRequest,
    const api::ICancelToken& cancelToken,
    const api::ProgressSink& progressSink)
{
    MultiModelProductionRequest request;
    request.effectiveconfigpath = sliceRequest.scene_config_path;
    request.jobid = sliceRequest.job_id;
    request.attemptid =
        api::artifacts::MakePackageAttemptId(
            sliceRequest.correlation_id);
    request.canceltoken = &cancelToken;
    request.progresscallback =
        [&progressSink](const SliceRunProgress& progress)
        {
            if (!progressSink)
            {
                return;
            }
            api::ProgressEvent event;
            event.stage = progress.phase;
            event.percent = progress.percent;
            event.layers_done = progress.current;
            event.layers_total = progress.total;
            progressSink(event);
        };

    const MultiModelProductionResult produced =
        RunMultiModelProductionService(request);
    if (!produced.packagewritten
        || produced.error.has_value())
    {
        return api::ApiResult<api::SliceResult>::Failure(
            MapProductionError(produced));
    }

    try
    {
        std::ifstream manifestInput(
            produced.packagedir / "manifest.json",
            std::ios::binary);
        if (!manifestInput)
        {
            throw std::runtime_error(
                "published manifest is not readable");
        }
        const Json manifest = Json::parse(manifestInput);
        const Json& grid = manifest.at("grid");

        api::SliceResult result;
        result.package_dir = produced.packagedir;
        result.manifest_path =
            produced.packagedir / "manifest.json";
        result.layer_count = produced.layercount;
        result.grid_px = {
            grid.at("widthPx").as_int(),
            grid.at("heightPx").as_int()};
        result.engine_version = "legacy-scene-v1";
        result.elapsed_ms = static_cast<std::uint64_t>(
            std::llround(std::max(0.0, produced.profile.total_ms)));
        return api::ApiResult<api::SliceResult>::Success(
            std::move(result));
    }
    catch (const std::exception& exception)
    {
        return api::ApiResult<api::SliceResult>::Failure(
            MakeError(
                "PM-SLICER-CONTRACT-0060",
                "published package summary is invalid",
                exception.what()));
    }
}

}  // namespace

std::unique_ptr<api::SliceFacade> CreateProductionSliceFacade()
{
    return std::make_unique<SliceFacadeAdapter>(
        ResolveSubmissionContract,
        RunExistingProductionEntry);
}

}  // namespace slicer_core::engine
