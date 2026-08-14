#include "slicer_core/engine/SliceFacadeAdapter.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

// 文件职责：将生产场景切片入口适配为可取消、可观测的 SliceFacade；
// 边界：仅转发已验证合同，不持有材质、栅格、TIFF 或发布策略。
namespace slicer_core::engine
{
namespace
{

constexpr const char* kCancelledCode =
    "PM-SLICER-CANCELLED-0070";

class CooperativeCancellation final : public std::exception
{
public:
    const char* what() const noexcept override
    {
        return "cooperative slice cancellation requested";
    }
};

class ProgressObserverFailure final : public std::runtime_error
{
public:
    explicit ProgressObserverFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

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

api::ApiResult<api::SliceResult> CancelledResult(
    const std::string& detail)
{
    return api::ApiResult<api::SliceResult>::Failure(
        MakeError(
            kCancelledCode,
            "slice job was cancelled cooperatively",
            detail));
}

std::filesystem::path NormalizePath(
    const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(path, error);
    return (error ? path : absolute).lexically_normal();
}

std::string ComparablePathString(
    const std::filesystem::path& path)
{
    std::string value = NormalizePath(path).generic_string();
#ifdef _WIN32
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
#endif
    return value;
}

bool PathsMatch(
    const std::filesystem::path& left,
    const std::filesystem::path& right)
{
    return ComparablePathString(left) == ComparablePathString(right);
}

std::optional<api::ApiError> ValidateRequest(
    const api::SliceRequest& request)
{
    if (request.job_id.empty()
        || request.correlation_id.empty()
        || request.scene_hash.empty())
    {
        return MakeError(
            "PM-SLICER-PROFILE-0030",
            "slice request is missing a required identity",
            "jobId, correlationId, and sceneHash are required");
    }
    if (request.scene_config_path.empty()
        || request.package_dir.empty())
    {
        return MakeError(
            "PM-SLICER-PROFILE-0030",
            "slice request is missing a required path",
            "sceneConfigPath and packageDir are required");
    }
    return std::nullopt;
}

}  // namespace

SliceFacadeAdapter::SliceFacadeAdapter(
    SliceSubmissionContractResolver contractResolver,
    SliceProductionRunner productionRunner)
    : m_contractResolver(std::move(contractResolver)),
      m_productionRunner(
          [runner = std::move(productionRunner)](
              const api::SliceRequest& request,
              const api::ICancelToken&,
              const api::ProgressSink& progressSink)
          {
              return runner(request.scene_config_path, progressSink);
          })
{
}

SliceFacadeAdapter::SliceFacadeAdapter(
    SliceSubmissionContractResolver contractResolver,
    CancellableSliceProductionRunner productionRunner)
    : m_contractResolver(std::move(contractResolver)),
      m_productionRunner(std::move(productionRunner))
{
}

api::ApiResult<api::SliceResult> SliceFacadeAdapter::Run(
    const api::SliceRequest& request,
    const api::ICancelToken& cancelToken,
    const api::ProgressSink& progressSink) noexcept
{
    try
    {
        if (!m_contractResolver || !m_productionRunner)
        {
            return api::ApiResult<api::SliceResult>::Failure(
                MakeError(
                    "PM-SLICER-INTERNAL-0099",
                    "SliceFacade production binding is unavailable"));
        }
        if (const std::optional<api::ApiError> error =
                ValidateRequest(request);
            error.has_value())
        {
            return api::ApiResult<api::SliceResult>::Failure(*error);
        }
        if (cancelToken.IsCancelRequested())
        {
            return CancelledResult("before_contract_resolution");
        }

        const api::ApiResult<SliceSubmissionContract> resolved =
            m_contractResolver(request.scene_config_path);
        if (!resolved.IsOk())
        {
            return api::ApiResult<api::SliceResult>::Failure(
                resolved.Error() != nullptr
                    ? *resolved.Error()
                    : MakeError(
                          "PM-SLICER-INTERNAL-0099",
                          "slice contract resolution failed without an error"));
        }
        const SliceSubmissionContract* contract = resolved.Value();
        if (contract == nullptr)
        {
            return api::ApiResult<api::SliceResult>::Failure(
                MakeError(
                    "PM-SLICER-INTERNAL-0099",
                    "slice contract resolver returned no value"));
        }
        if (contract->scenehash != request.scene_hash)
        {
            return api::ApiResult<api::SliceResult>::Failure(
                MakeError(
                    "PM-SLICER-LAYOUT-0022",
                    "committed scene hash is stale",
                    "requested=" + request.scene_hash
                        + "; effective=" + contract->scenehash));
        }
        if (!PathsMatch(contract->packagedir, request.package_dir))
        {
            return api::ApiResult<api::SliceResult>::Failure(
                MakeError(
                    "PM-SLICER-PROFILE-0031",
                    "requested package path differs from the effective config",
                    "requested=" + NormalizePath(request.package_dir).generic_string()
                        + "; effective="
                        + NormalizePath(contract->packagedir).generic_string()));
        }
        if (cancelToken.IsCancelRequested())
        {
            return CancelledResult("before_production_run");
        }

        int lastPercent{0};
        std::string lastStage{"submission"};
        std::optional<api::ApiError> progressObserverError;
        const api::ProgressSink guardedProgress =
            [&cancelToken,
             &progressSink,
             &lastPercent,
             &lastStage,
             &progressObserverError](const api::ProgressEvent& event)
            {
                lastStage = event.stage;
                if (event.stage != "completed"
                    && cancelToken.IsCancelRequested())
                {
                    throw CooperativeCancellation{};
                }

                api::ProgressEvent forwarded = event;
                forwarded.percent = std::clamp(
                    std::max(lastPercent, event.percent),
                    0,
                    100);
                lastPercent = forwarded.percent;
                if (!progressSink)
                {
                    return;
                }
                try
                {
                    progressSink(forwarded);
                }
                catch (const std::exception& exception)
                {
                    progressObserverError = MakeError(
                        "PM-SLICER-INTERNAL-0099",
                        "slice progress observer failed",
                        exception.what());
                    throw ProgressObserverFailure(exception.what());
                }
                catch (...)
                {
                    progressObserverError = MakeError(
                        "PM-SLICER-INTERNAL-0099",
                        "slice progress observer failed",
                        "progress observer threw a non-standard exception");
                    throw ProgressObserverFailure(
                        "progress observer threw a non-standard exception");
                }
            };

        api::ApiResult<api::SliceResult> result =
            m_productionRunner(
                request,
                cancelToken,
                guardedProgress);
        if (!result.IsOk() && cancelToken.IsCancelRequested())
        {
            return CancelledResult(lastStage);
        }
        if (progressObserverError.has_value())
        {
            return api::ApiResult<api::SliceResult>::Failure(
                *progressObserverError);
        }
        if (result.IsOk())
        {
            const api::SliceResult* value = result.Value();
            if (value == nullptr
                || !PathsMatch(value->package_dir, request.package_dir))
            {
                return api::ApiResult<api::SliceResult>::Failure(
                    MakeError(
                        "PM-SLICER-CONTRACT-0060",
                        "production result package identity is invalid"));
            }
        }
        return result;
    }
    catch (const CooperativeCancellation&)
    {
        return CancelledResult("progress_boundary");
    }
    catch (const ProgressObserverFailure& exception)
    {
        return api::ApiResult<api::SliceResult>::Failure(
            MakeError(
                "PM-SLICER-INTERNAL-0099",
                "slice progress observer failed",
                exception.what()));
    }
    catch (const std::bad_alloc&)
    {
        return api::ApiResult<api::SliceResult>::Failure(
            MakeError(
                "PM-SLICER-RESOURCE-0040",
                "insufficient memory while running SliceFacade"));
    }
    catch (const std::exception& exception)
    {
        return api::ApiResult<api::SliceResult>::Failure(
            MakeError(
                "PM-SLICER-INTERNAL-0099",
                "unexpected SliceFacade failure",
                exception.what()));
    }
    catch (...)
    {
        return api::ApiResult<api::SliceResult>::Failure(
            MakeError(
                "PM-SLICER-INTERNAL-0099",
                "unknown SliceFacade failure"));
    }
}

}  // namespace slicer_core::engine
