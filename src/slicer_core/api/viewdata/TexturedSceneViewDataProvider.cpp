#include "slicer_core/api/viewdata/TexturedSceneViewDataProvider.h"

#include "slicer_core/api/viewdata/SceneSurfacePreviewBuilder.h"
#include "slicer_core/api/viewdata/SceneViewBudget.h"
#include "slicer_core/api/viewdata/SceneViewClosureValidator.h"
#include "slicer_core/api/viewdata/SceneViewIdentity.h"
#include "slicer_core/api/viewdata/SceneViewMeshBuilder.h"
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

Bounds3d LocalBounds(const SceneModel& model)
{
    Bounds3d bounds;
    bounds.min_mm = {
        model.bbox_mm.min.x,
        model.bbox_mm.min.y,
        model.bbox_mm.min.z};
    bounds.max_mm = {
        model.bbox_mm.max.x,
        model.bbox_mm.max.y,
        model.bbox_mm.max.z};
    return bounds;
}

struct PreparedModel
{
    std::shared_ptr<const SceneModel> model;
    viewdata_detail::ResolvedViewAppearance appearance;
};

struct PreparedInstance
{
    SceneInstanceState state;
    std::shared_ptr<const PreparedModel> model;
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

            ApiResult<std::vector<PreparedInstance>> prepared = Prepare(
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
                constexpr std::array<int, 7> dimensions{
                    768, 512, 384, 256, 128, 64, 32};
                for (const int dimension : dimensions)
                {
                    ApiResult<SceneViewData> candidate = BuildCandidate(
                        request,
                        snapshot,
                        *prepared.Value(),
                        dimension,
                        ViewLod::Lod0,
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
                    ApiResult<SceneViewData> candidate = BuildCandidate(
                        request,
                        snapshot,
                        *prepared.Value(),
                        0,
                        lod,
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

    ApiResult<std::vector<PreparedInstance>> Prepare(
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
                    return Failure<std::vector<PreparedInstance>>(
                        "PM-SLICER-PROFILE-0031",
                        "ViewData instance filter contains a duplicate",
                        instanceId);
                }
                const auto state = states.find(instanceId);
                if (state == states.end())
                {
                    return Failure<std::vector<PreparedInstance>>(
                        "PM-SLICER-INPUT-0001",
                        "ViewData instance is not present in the scene",
                        instanceId);
                }
                selected.push_back(state->second);
            }
        }
        if (selected.empty())
        {
            return Failure<std::vector<PreparedInstance>>(
                "PM-SLICER-PROFILE-0031",
                "ViewData request selected no instances",
                "instance_ids");
        }

        std::map<ModelId, std::shared_ptr<const PreparedModel>> modelCache;
        std::vector<PreparedInstance> result;
        result.reserve(selected.size());
        for (const SceneInstanceState* state : selected)
        {
            if (cancelToken.IsCancelRequested())
            {
                return Failure<std::vector<PreparedInstance>>(
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
                    return Failure<std::vector<PreparedInstance>>(
                        model.Error()->code,
                        model.Error()->message,
                        model.Error()->detail);
                }
                auto prepared = std::make_shared<PreparedModel>();
                prepared->model = *model.Value();
                ApiResult<viewdata_detail::ResolvedViewAppearance> appearance =
                    viewdata_detail::ResolveViewAppearance(
                        *prepared->model,
                        *m_textures,
                        cancelToken);
                if (!appearance.IsOk())
                {
                    return Failure<std::vector<PreparedInstance>>(
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
        return ApiResult<std::vector<PreparedInstance>>::Success(
            std::move(result));
    }

    static ApiResult<SceneViewData> BuildCandidate(
        const SceneViewDataRequest& request,
        const SceneSnapshot& snapshot,
        const std::vector<PreparedInstance>& prepared,
        const int previewDimension,
        const ViewLod lod,
        const ICancelToken& cancelToken)
    {
        SceneViewData result;
        result.view_mode = request.view_mode;
        result.scene_revision = snapshot.scene_revision;
        std::map<std::string, std::size_t> appearances;

        for (const PreparedInstance& preparedInstance : prepared)
        {
            if (cancelToken.IsCancelRequested())
            {
                return Cancelled(snapshot.scene_id);
            }
            const PreparedModel& preparedModel = *preparedInstance.model;
            const std::string& appearanceIdentity =
                preparedModel.appearance.appearance.appearance_identity;
            if (!appearances.contains(appearanceIdentity))
            {
                appearances.emplace(
                    appearanceIdentity,
                    result.appearances.size());
                result.appearances.push_back(
                    preparedModel.appearance.appearance);
            }

            ViewInstance instance;
            instance.instance_id =
                preparedInstance.state.instance.instance_id;
            instance.model_id = preparedInstance.state.instance.model_id;
            instance.world_matrix =
                preparedInstance.state.instance.world_matrix;
            instance.local_bounds_mm = LocalBounds(*preparedModel.model);
            instance.texture_status = preparedModel.appearance.has_texture
                ? TextureStatus::Available
                : TextureStatus::NotProvided;
            instance.appearance_identity = appearanceIdentity;

            if (request.view_mode == ViewMode::Top)
            {
                ApiResult<SurfacePreview> preview =
                    viewdata_detail::BuildSurfacePreview(
                        *preparedModel.model,
                        preparedModel.appearance,
                        previewDimension,
                        cancelToken);
                if (!preview.IsOk())
                {
                    return Failure<SceneViewData>(
                        preview.Error()->code,
                        preview.Error()->message,
                        preview.Error()->detail);
                }
                instance.preview_identity =
                    preview.Value()->preview_identity;
                instance.surface_preview = std::move(*preview.Value());
            }
            else
            {
                ApiResult<ViewMesh> mesh = viewdata_detail::BuildViewMesh(
                    *preparedModel.model,
                    preparedModel.appearance,
                    instance.world_matrix,
                    lod,
                    request.mesh_transform,
                    cancelToken);
                if (!mesh.IsOk())
                {
                    return Failure<SceneViewData>(
                        mesh.Error()->code,
                        mesh.Error()->message,
                        mesh.Error()->detail);
                }
                instance.mesh_identity = mesh.Value()->mesh_identity;
                instance.mesh = std::move(*mesh.Value());
            }
            result.instances.push_back(std::move(instance));
        }

        const ApiResult<void> closure =
            viewdata_detail::ValidateViewDataClosure(result);
        if (!closure.IsOk())
        {
            return Failure<SceneViewData>(
                closure.Error()->code,
                closure.Error()->message,
                closure.Error()->detail);
        }
        result.viewdata_identity =
            viewdata_detail::ComputeViewDataIdentity(result);
        return ApiResult<SceneViewData>::Success(std::move(result));
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
