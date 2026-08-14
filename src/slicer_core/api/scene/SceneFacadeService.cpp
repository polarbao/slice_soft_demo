#include "slicer_core/api/scene/SceneFacadeService.h"

#include "slicer_core/api/scene/SceneFacadeAuthority.h"

#include <map>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <utility>

// 文件职责：持有权威场景并协调 Commit、快照、碰撞和 ViewData 三条通道；
// 边界：几何与 ViewData 由下层服务提供，本文件不在宿主侧重复计算策略。
namespace slicer_core::api
{
namespace
{

template <class T>
ApiResult<T> Failure(
    const std::string_view code,
    const std::string_view message,
    const std::string_view detail)
{
    return ApiResult<T>::Failure(
        {std::string(code), std::string(message), std::string(detail)});
}

ApiResult<SceneCommitResult> Cancelled(const std::string_view detail)
{
    return Failure<SceneCommitResult>(
        "PM-SLICER-CANCELLED-0070",
        "scene operation was cancelled without mutation",
        detail);
}

std::string BuildCommitViewDataIdentity(const SceneSnapshot& snapshot)
{
    return "vd:" + std::to_string(snapshot.scene_revision)
        + ":scene:auto:" + snapshot.scene_hash.substr(0U, 8U);
}

SceneCommitResult BuildCommitResult(
    const scene_facade_detail::AuthorityState& authority)
{
    SceneCommitResult result;
    result.snapshot = authority.snapshot;
    result.collision_report = authority.collision_report;
    result.warnings = authority.warnings;
    result.preflight_delta = authority.preflight_delta;
    result.viewdata_identity = BuildCommitViewDataIdentity(
        authority.snapshot);
    return result;
}

ApiResult<CollisionReport> CancelledCollision(
    const std::string_view detail)
{
    return Failure<CollisionReport>(
        "PM-SLICER-CANCELLED-0070",
        "scene collision evaluation was cancelled",
        detail);
}

ApiResult<SceneViewData> CancelledView(const std::string_view detail)
{
    return Failure<SceneViewData>(
        "PM-SLICER-CANCELLED-0070",
        "scene view request was cancelled",
        detail);
}

}  // namespace

class SceneFacadeService::Impl
{
public:
    struct ReplayRecord
    {
        std::string fingerprint;
        ApiResult<SceneCommitResult> result;
    };

    explicit Impl(
        scene_facade_detail::AuthorityState state,
        std::shared_ptr<const ITexturedSceneViewDataProvider> provider)
        : authority(std::move(state)), viewDataProvider(std::move(provider))
    {
    }

    mutable std::mutex mutex;
    scene_facade_detail::AuthorityState authority;
    std::map<std::string, ReplayRecord> replayRecords;
    std::shared_ptr<const ITexturedSceneViewDataProvider> viewDataProvider;
};

SceneFacadeService::SceneFacadeService(
    std::unique_ptr<Impl> implementation) noexcept
    : m_implementation(std::move(implementation))
{
}

SceneFacadeService::~SceneFacadeService() = default;

ApiResult<void> SceneFacadeService::RegisterModel(
    SceneFacadeModelRegistration registration) noexcept
{
    try
    {
        if (registration.api_model_id == 0U
            || registration.scene_model_id.empty()
            || registration.source.modelid != registration.scene_model_id
            || registration.source.sourcepath.empty()
            || registration.source.format.empty()
            || registration.source.resourcescopeid.empty()
            || registration.source.sourcehash.empty()
            || registration.source.resourcehash.empty()
            || registration.scope.resourcescopeid
                != registration.source.resourcescopeid
            || !registration.model)
        {
            return Failure<void>(
                "PM-SLICER-INPUT-0002",
                "scene model registration is incomplete",
                std::to_string(registration.api_model_id));
        }

        std::scoped_lock lock(m_implementation->mutex);
        const auto existing =
            m_implementation->authority.seed.registered_models.find(
                registration.api_model_id);
        if (existing !=
            m_implementation->authority.seed.registered_models.end())
        {
            const SceneFacadeModelRegistration& current = existing->second;
            const bool equivalent =
                current.scene_model_id == registration.scene_model_id
                && current.source.sourcepath.lexically_normal()
                    == registration.source.sourcepath.lexically_normal()
                && current.source.sourcehash == registration.source.sourcehash
                && current.source.resourcehash
                    == registration.source.resourcehash
                && current.scope.resourcescopeid
                    == registration.scope.resourcescopeid;
            return equivalent
                ? ApiResult<void>::Success()
                : Failure<void>(
                    "PM-SLICER-PROFILE-0031",
                    "modelId was registered with conflicting scene resources",
                    std::to_string(registration.api_model_id));
        }

        m_implementation->authority.seed.registered_models.emplace(
            registration.api_model_id,
            std::move(registration));
        return ApiResult<void>::Success();
    }
    catch (const std::exception& error)
    {
        return Failure<void>(
            "PM-SLICER-INTERNAL-0099",
            "failed to register a SceneFacade model resource",
            error.what());
    }
    catch (...)
    {
        return Failure<void>(
            "PM-SLICER-INTERNAL-0099",
            "failed to register a SceneFacade model resource",
            "unknown exception");
    }
}

ApiResult<std::shared_ptr<SceneFacadeService>> SceneFacadeService::Create(
    SceneFacadeSeed seed,
    std::shared_ptr<const ITexturedSceneViewDataProvider> viewDataProvider) noexcept
{
    try
    {
        ApiResult<scene_facade_detail::AuthorityState> state =
            scene_facade_detail::BuildAuthorityState(std::move(seed));
        if (!state.IsOk())
        {
            return Failure<std::shared_ptr<SceneFacadeService>>(
                state.Error()->code,
                state.Error()->message,
                state.Error()->detail);
        }
        auto implementation = std::make_unique<Impl>(
            *state.Value(),
            std::move(viewDataProvider));
        return ApiResult<std::shared_ptr<SceneFacadeService>>::Success(
            std::shared_ptr<SceneFacadeService>(
                new SceneFacadeService(std::move(implementation))));
    }
    catch (const std::exception& error)
    {
        return Failure<std::shared_ptr<SceneFacadeService>>(
            "PM-SLICER-INTERNAL-0099",
            "failed to create SceneFacade",
            error.what());
    }
    catch (...)
    {
        return Failure<std::shared_ptr<SceneFacadeService>>(
            "PM-SLICER-INTERNAL-0099",
            "failed to create SceneFacade",
            "unknown exception");
    }
}

ApiResult<SceneCommitResult> SceneFacadeService::ApplyOperation(
    const SceneOperationRequest& request,
    const ICancelToken& cancelToken) noexcept
{
    try
    {
        std::scoped_lock lock(m_implementation->mutex);
        if (request.scene_id != m_implementation->authority.seed.scene_id)
        {
            return Failure<SceneCommitResult>(
                "PM-SLICER-PROFILE-0031",
                "scene operation references an unknown scene",
                std::to_string(request.scene_id));
        }
        if (request.operation_id.empty())
        {
            return Failure<SceneCommitResult>(
                "PM-SLICER-PROFILE-0031",
                "operationId must not be empty",
                "operation_id");
        }
        if (request.current_scene_revision
            != request.expected_scene_revision)
        {
            return Failure<SceneCommitResult>(
                "PM-SLICER-PROFILE-0031",
                "currentSceneRevision must equal expectedSceneRevision",
                request.operation_id);
        }

        const std::string fingerprint =
            scene_facade_detail::ComputeOperationFingerprint(request);
        const auto replay = m_implementation->replayRecords.find(
            request.operation_id);
        if (replay != m_implementation->replayRecords.end())
        {
            if (replay->second.fingerprint != fingerprint)
            {
                return Failure<SceneCommitResult>(
                    "PM-SLICER-PROFILE-0031",
                    "operationId was reused with a different canonical payload",
                    request.operation_id);
            }
            return replay->second.result;
        }

        ApiResult<SceneCommitResult> result = Cancelled(
            request.operation_id);
        if (!cancelToken.IsCancelRequested())
        {
            const std::optional<SceneValidationError> stale =
                ValidateSceneRevision(
                    m_implementation->authority.seed.scene,
                    request.expected_scene_revision);
            if (stale.has_value())
            {
                result = Failure<SceneCommitResult>(
                    "PM-SLICER-LAYOUT-0022",
                    stale->message,
                    request.operation_id);
            }
            else
            {
                ApiResult<scene_facade_detail::AuthorityState> candidate =
                    scene_facade_detail::ApplyOperationBatch(
                        m_implementation->authority,
                        request,
                        cancelToken);
                if (!candidate.IsOk())
                {
                    result = Failure<SceneCommitResult>(
                        candidate.Error()->code,
                        candidate.Error()->message,
                        candidate.Error()->detail);
                }
                else
                {
                    m_implementation->authority = *candidate.Value();
                    result = ApiResult<SceneCommitResult>::Success(
                        BuildCommitResult(m_implementation->authority));
                }
            }
        }

        m_implementation->replayRecords.emplace(
            request.operation_id,
            Impl::ReplayRecord{fingerprint, result});
        return result;
    }
    catch (const std::exception& error)
    {
        return Failure<SceneCommitResult>(
            "PM-SLICER-INTERNAL-0099",
            "failed to process SceneFacade operation",
            error.what());
    }
    catch (...)
    {
        return Failure<SceneCommitResult>(
            "PM-SLICER-INTERNAL-0099",
            "failed to process SceneFacade operation",
            "unknown exception");
    }
}

ApiResult<SceneSnapshot> SceneFacadeService::GetSnapshot(
    const SceneId sceneId) const noexcept
{
    try
    {
        std::scoped_lock lock(m_implementation->mutex);
        if (sceneId != m_implementation->authority.seed.scene_id)
        {
            return Failure<SceneSnapshot>(
                "PM-SLICER-INPUT-0001",
                "scene snapshot references an unknown scene",
                std::to_string(sceneId));
        }
        return ApiResult<SceneSnapshot>::Success(
            m_implementation->authority.snapshot);
    }
    catch (const std::exception& error)
    {
        return Failure<SceneSnapshot>(
            "PM-SLICER-INTERNAL-0099",
            "failed to read SceneFacade snapshot",
            error.what());
    }
    catch (...)
    {
        return Failure<SceneSnapshot>(
            "PM-SLICER-INTERNAL-0099",
            "failed to read SceneFacade snapshot",
            "unknown exception");
    }
}

ApiResult<SceneViewData> SceneFacadeService::GetViewData(
    const SceneViewDataRequest& request,
    const ICancelToken& cancelToken) const noexcept
{
    try
    {
        if (cancelToken.IsCancelRequested())
        {
            return CancelledView(std::to_string(request.scene_id));
        }
        if (request.view_mode == ViewMode::ThreeD
            && request.lod == ViewLod::OutlineOnly)
        {
            return Failure<SceneViewData>(
                "PM-SLICER-PROFILE-0031",
                "three_d ViewData cannot use outline_only",
                "lod");
        }

        SceneSnapshot snapshot;
        std::shared_ptr<const ITexturedSceneViewDataProvider> provider;
        {
            std::scoped_lock lock(m_implementation->mutex);
            if (request.scene_id != m_implementation->authority.seed.scene_id)
            {
                return Failure<SceneViewData>(
                    "PM-SLICER-INPUT-0001",
                    "ViewData request references an unknown scene",
                    std::to_string(request.scene_id));
            }
            if (request.expected_scene_revision
                != m_implementation->authority.snapshot.scene_revision)
            {
                return Failure<SceneViewData>(
                    "PM-SLICER-VIEWDATA-STALE",
                    "ViewData request revision is stale",
                    std::to_string(request.expected_scene_revision));
            }
            provider = m_implementation->viewDataProvider;
            snapshot = m_implementation->authority.snapshot;
        }

        if (!provider)
        {
            return Failure<SceneViewData>(
                "PM-SLICER-INTERNAL-0099",
                "textured Scene ViewData provider is not installed",
                "Stage 14B-03A TexturedSceneViewDataProvider is required");
        }

        ApiResult<SceneViewData> provided = provider->GetViewData(
            request,
            snapshot,
            cancelToken);
        if (!provided.IsOk())
        {
            return provided;
        }

        std::scoped_lock lock(m_implementation->mutex);
        if (m_implementation->authority.snapshot.scene_revision
                != snapshot.scene_revision
            || provided.Value()->scene_revision != snapshot.scene_revision
            || provided.Value()->view_mode != request.view_mode)
        {
            return Failure<SceneViewData>(
                "PM-SLICER-VIEWDATA-STALE",
                "ViewData provider result no longer matches the authoritative scene",
                std::to_string(snapshot.scene_revision));
        }
        return provided;
    }
    catch (const std::exception& error)
    {
        return Failure<SceneViewData>(
            "PM-SLICER-INTERNAL-0099",
            "failed to process SceneFacade ViewData request",
            error.what());
    }
    catch (...)
    {
        return Failure<SceneViewData>(
            "PM-SLICER-INTERNAL-0099",
            "failed to process SceneFacade ViewData request",
            "unknown exception");
    }
}

ApiResult<CollisionReport> SceneFacadeService::CheckCollision(
    const SceneSnapshot& snapshot,
    const ICancelToken& cancelToken) const noexcept
{
    try
    {
        if (cancelToken.IsCancelRequested())
        {
            return CancelledCollision(std::to_string(snapshot.scene_id));
        }
        std::scoped_lock lock(m_implementation->mutex);
        const SceneSnapshot& authoritative =
            m_implementation->authority.snapshot;
        if (snapshot.scene_id != authoritative.scene_id
            || snapshot.scene_revision != authoritative.scene_revision
            || snapshot.scene_hash != authoritative.scene_hash)
        {
            return Failure<CollisionReport>(
                "PM-SLICER-LAYOUT-0022",
                "collision request snapshot is stale",
                std::to_string(snapshot.scene_revision));
        }
        return ApiResult<CollisionReport>::Success(
            m_implementation->authority.collision_report);
    }
    catch (const std::exception& error)
    {
        return Failure<CollisionReport>(
            "PM-SLICER-INTERNAL-0099",
            "failed to read authoritative collision report",
            error.what());
    }
    catch (...)
    {
        return Failure<CollisionReport>(
            "PM-SLICER-INTERNAL-0099",
            "failed to read authoritative collision report",
            "unknown exception");
    }
}

}  // namespace slicer_core::api
