#include "slicer_core/api/viewdata/TexturedSceneViewDataProvider.h"

#include "slicer_core/api/viewdata/SceneViewBudget.h"
#include "slicer_core/api/viewdata/SceneViewCandidateBuilder.h"
#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"

#include <array>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace slicer_core::api
{
namespace
{

template <class T>
ApiResult<T> Failure(
    const std::string_view code,
    const std::string_view message,
    const std::string& detail)
{
    return ApiResult<T>::Failure(
        {std::string(code), std::string(message), detail});
}

struct TopBudgetAttempt
{
    int preview_dimension{0};
    std::size_t max_texture_edge_px{0U};
};

class TexturedSceneViewDataProvider final
    : public ITexturedSceneViewDataProvider
{
public:
    TexturedSceneViewDataProvider(
        std::shared_ptr<const ISceneViewModelRepository> models,
        std::shared_ptr<const ISceneViewTextureSource> textures)
        : m_models(std::move(models)), m_textures(std::move(textures))
    {
    }

    [[nodiscard]] ApiResult<SceneViewData> GetViewData(
        const SceneViewDataRequest& request,
        const SceneSnapshot& snapshot,
        const ICancelToken& cancelToken) const noexcept override
    {
        try
        {
            const ApiResult<void> requestValidation = ValidateRequest(
                request,
                snapshot);
            if (!requestValidation.IsOk())
            {
                return Failure<SceneViewData>(
                    requestValidation.Error()->code,
                    requestValidation.Error()->message,
                    requestValidation.Error()->detail);
            }
            if (cancelToken.IsCancelRequested())
            {
                return Cancelled(snapshot.scene_id);
            }

            ApiResult<std::vector<viewdata_detail::PreparedViewInstance>>
                prepared = Prepare(
                request,
                snapshot,
                cancelToken);
            if (!prepared.IsOk())
            {
                return Failure<SceneViewData>(
                    prepared.Error()->code,
                    prepared.Error()->message,
                    prepared.Error()->detail);
            }

            if (request.view_mode == ViewMode::Top)
            {
                constexpr std::array<TopBudgetAttempt, 14> attempts{{
                    {768, 0U},
                    {512, 0U},
                    {384, 0U},
                    {256, 0U},
                    {128, 0U},
                    {64, 0U},
                    {32, 0U},
                    {256, 2048U},
                    {256, 1024U},
                    {128, 512U},
                    {128, 256U},
                    {64, 128U},
                    {32, 64U},
                    {32, 32U}}};
                for (const TopBudgetAttempt& attempt : attempts)
                {
                    viewdata_detail::ViewCandidateOptions options;
                    options.preview_dimension = attempt.preview_dimension;
                    options.lod = ViewLod::Lod0;
                    options.max_texture_edge_px =
                        attempt.max_texture_edge_px;
                    options.degraded = attempt.preview_dimension < 768;
                    if (options.degraded)
                    {
                        options.degradation_reason =
                            "top_preview_resolution_reduced_for_max_bytes";
                    }
                    ApiResult<SceneViewData> candidate =
                        viewdata_detail::BuildViewCandidate(
                        request,
                        snapshot,
                        *prepared.Value(),
                        options,
                        cancelToken);
                    if (!candidate.IsOk())
                    {
                        return candidate;
                    }
                    if (viewdata_detail::EstimateViewDataBytes(
                            *candidate.Value()) <= request.max_bytes)
                    {
                        return candidate;
                    }
                }
            }
            else
            {
                const std::vector<ViewLod> lods = request.lod == ViewLod::Auto
                    ? std::vector<ViewLod>{
                        ViewLod::Lod0, ViewLod::Lod1, ViewLod::Lod2}
                    : std::vector<ViewLod>{request.lod};
                for (const ViewLod lod : lods)
                {
                    viewdata_detail::ViewCandidateOptions options;
                    options.lod = lod;
                    options.degraded = request.lod == ViewLod::Auto
                        && lod != ViewLod::Lod0;
                    if (options.degraded)
                    {
                        options.degradation_reason =
                            "mesh_lod_reduced_for_max_bytes";
                    }
                    ApiResult<SceneViewData> candidate =
                        viewdata_detail::BuildViewCandidate(
                        request,
                        snapshot,
                        *prepared.Value(),
                        options,
                        cancelToken);
                    if (!candidate.IsOk())
                    {
                        return candidate;
                    }
                    if (viewdata_detail::EstimateViewDataBytes(
                            *candidate.Value()) <= request.max_bytes)
                    {
                        return candidate;
                    }
                }

                constexpr std::array<std::size_t, 7> textureEdges{
                    2048U, 1024U, 512U, 256U, 128U, 64U, 32U};
                const ViewLod textureBudgetLod = request.lod == ViewLod::Auto
                    ? ViewLod::Lod2
                    : request.lod;
                for (const std::size_t edge : textureEdges)
                {
                    viewdata_detail::ViewCandidateOptions options;
                    options.lod = textureBudgetLod;
                    options.max_texture_edge_px = edge;
                    options.degraded = request.lod == ViewLod::Auto
                        && textureBudgetLod != ViewLod::Lod0;
                    if (options.degraded)
                    {
                        options.degradation_reason =
                            "mesh_lod_reduced_for_max_bytes";
                    }
                    ApiResult<SceneViewData> candidate =
                        viewdata_detail::BuildViewCandidate(
                            request,
                            snapshot,
                            *prepared.Value(),
                            options,
                            cancelToken);
                    if (!candidate.IsOk())
                    {
                        return candidate;
                    }
                    if (viewdata_detail::EstimateViewDataBytes(
                            *candidate.Value()) <= request.max_bytes)
                    {
                        return candidate;
                    }
                }
            }

            return Failure<SceneViewData>(
                "PM-SLICER-VIEWDATA-BUDGET",
                "ViewData cannot retain required texture content within budget",
                std::to_string(request.max_bytes));
        }
        catch (const std::exception& error)
        {
            return Failure<SceneViewData>(
                "PM-SLICER-INTERNAL-0099",
                "failed to build textured Scene ViewData",
                error.what());
        }
        catch (...)
        {
            return Failure<SceneViewData>(
                "PM-SLICER-INTERNAL-0099",
                "failed to build textured Scene ViewData",
                "unknown exception");
        }
    }

private:
    static ApiResult<SceneViewData> Cancelled(const SceneId sceneId)
    {
        return Failure<SceneViewData>(
            "PM-SLICER-CANCELLED-0070",
            "textured ViewData request was cancelled",
            std::to_string(sceneId));
    }

    static ApiResult<void> ValidateRequest(
        const SceneViewDataRequest& request,
        const SceneSnapshot& snapshot)
    {
        if (request.scene_id == 0U || request.scene_id != snapshot.scene_id)
        {
            return Failure<void>(
                "PM-SLICER-INPUT-0001",
                "ViewData request references an unknown scene",
                std::to_string(request.scene_id));
        }
        if (request.expected_scene_revision != snapshot.scene_revision)
        {
            return Failure<void>(
                "PM-SLICER-VIEWDATA-STALE",
                "ViewData request revision is stale",
                std::to_string(request.expected_scene_revision));
        }
        if (request.max_bytes == 0U)
        {
            return Failure<void>(
                "PM-SLICER-PROFILE-0031",
                "ViewData maxBytes must be non-zero",
                "max_bytes");
        }
        if (request.content.empty())
        {
            return Failure<void>(
                "PM-SLICER-PROFILE-0031",
                "ViewData content must not be empty",
                "content");
        }
        std::set<ViewContent> content;
        for (const ViewContent item : request.content)
        {
            if (!content.emplace(item).second)
            {
                return Failure<void>(
                    "PM-SLICER-PROFILE-0031",
                    "ViewData content contains a duplicate",
                    "content");
            }
        }
        const std::array<ViewContent, 3> commonContent{
            ViewContent::Bbox,
            ViewContent::Outline,
            ViewContent::Appearance};
        for (const ViewContent required : commonContent)
        {
            if (!content.contains(required))
            {
                return Failure<void>(
                    "PM-SLICER-PROFILE-0031",
                    "ViewData content omits a required item",
                    "content");
            }
        }
        const ViewContent modeContent = request.view_mode == ViewMode::Top
            ? ViewContent::SurfacePreview
            : ViewContent::Mesh;
        if (!content.contains(modeContent))
        {
            return Failure<void>(
                "PM-SLICER-PROFILE-0031",
                "ViewData content omits the required view payload",
                "content");
        }
        if (request.view_mode == ViewMode::ThreeD
            && request.lod == ViewLod::OutlineOnly)
        {
            return Failure<void>(
                "PM-SLICER-PROFILE-0031",
                "three_d ViewData cannot use outline_only",
                "lod");
        }
        return ApiResult<void>::Success();
    }

    ApiResult<std::vector<viewdata_detail::PreparedViewInstance>> Prepare(
        const SceneViewDataRequest& request,
        const SceneSnapshot& snapshot,
        const ICancelToken& cancelToken) const
    {
        std::map<std::string, const SceneInstanceState*> states;
        for (const SceneInstanceState& state : snapshot.instances)
        {
            states.emplace(state.instance.instance_id, &state);
        }

        std::vector<const SceneInstanceState*> selected;
        if (request.instance_ids.empty())
        {
            for (const SceneInstanceState& state : snapshot.instances)
            {
                selected.push_back(&state);
            }
        }
        else
        {
            std::set<std::string> uniqueIds;
            for (const std::string& instanceId : request.instance_ids)
            {
                if (!uniqueIds.emplace(instanceId).second)
                {
                    return Failure<std::vector<
                        viewdata_detail::PreparedViewInstance>>(
                        "PM-SLICER-PROFILE-0031",
                        "ViewData instance filter contains a duplicate",
                        instanceId);
                }
                const auto state = states.find(instanceId);
                if (state == states.end())
                {
                    return Failure<std::vector<
                        viewdata_detail::PreparedViewInstance>>(
                        "PM-SLICER-INPUT-0001",
                        "ViewData instance is not present in the scene",
                        instanceId);
                }
                selected.push_back(state->second);
            }
        }
        if (selected.empty())
        {
            return Failure<std::vector<
                viewdata_detail::PreparedViewInstance>>(
                "PM-SLICER-PROFILE-0031",
                "ViewData request selected no instances",
                "instance_ids");
        }

        std::map<ModelId, std::shared_ptr<const
            viewdata_detail::PreparedViewModel>> modelCache;
        std::vector<viewdata_detail::PreparedViewInstance> result;
        result.reserve(selected.size());
        for (const SceneInstanceState* state : selected)
        {
            if (cancelToken.IsCancelRequested())
            {
                return Failure<std::vector<
                    viewdata_detail::PreparedViewInstance>>(
                    "PM-SLICER-CANCELLED-0070",
                    "ViewData resource preparation was cancelled",
                    state->instance.instance_id);
            }
            auto preparedModel = modelCache.find(state->instance.model_id);
            if (preparedModel == modelCache.end())
            {
                ApiResult<std::shared_ptr<const SceneModel>> model =
                    m_models->GetModel(state->instance.model_id);
                if (!model.IsOk())
                {
                    return Failure<std::vector<
                        viewdata_detail::PreparedViewInstance>>(
                        model.Error()->code,
                        model.Error()->message,
                        model.Error()->detail);
                }
                auto prepared = std::make_shared<
                    viewdata_detail::PreparedViewModel>();
                prepared->model = *model.Value();
                ApiResult<viewdata_detail::ResolvedViewAppearance> appearance =
                    viewdata_detail::ResolveViewAppearance(
                        *prepared->model,
                        *m_textures,
                        cancelToken);
                if (!appearance.IsOk())
                {
                    return Failure<std::vector<
                        viewdata_detail::PreparedViewInstance>>(
                        appearance.Error()->code,
                        appearance.Error()->message,
                        appearance.Error()->detail);
                }
                prepared->appearance = std::move(*appearance.Value());
                preparedModel = modelCache.emplace(
                    state->instance.model_id,
                    std::move(prepared)).first;
            }
            result.push_back({*state, preparedModel->second});
        }
        return ApiResult<std::vector<
            viewdata_detail::PreparedViewInstance>>::Success(
            std::move(result));
    }

    const std::shared_ptr<const ISceneViewModelRepository> m_models;
    const std::shared_ptr<const ISceneViewTextureSource> m_textures;
};

}  // namespace

ApiResult<std::shared_ptr<const ITexturedSceneViewDataProvider>>
CreateTexturedSceneViewDataProvider(
    std::shared_ptr<const ISceneViewModelRepository> models,
    std::shared_ptr<const ISceneViewTextureSource> textures) noexcept
{
    if (!models || !textures)
    {
        return Failure<
            std::shared_ptr<const ITexturedSceneViewDataProvider>>(
                "PM-SLICER-PROFILE-0031",
                "textured ViewData provider requires model and texture sources",
                "dependencies");
    }
    return ApiResult<
        std::shared_ptr<const ITexturedSceneViewDataProvider>>::Success(
            std::make_shared<TexturedSceneViewDataProvider>(
                std::move(models),
                std::move(textures)));
}

}  // namespace slicer_core::api
