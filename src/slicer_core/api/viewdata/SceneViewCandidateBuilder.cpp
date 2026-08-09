#include "slicer_core/api/viewdata/SceneViewCandidateBuilder.h"

#include "slicer_core/api/viewdata/SceneSurfacePreviewBuilder.h"
#include "slicer_core/api/viewdata/SceneViewAppearanceBudget.h"
#include "slicer_core/api/viewdata/SceneViewClosureValidator.h"
#include "slicer_core/api/viewdata/SceneViewIdentity.h"
#include "slicer_core/api/viewdata/SceneViewMeshBuilder.h"
#include "slicer_core/api/viewdata/SceneViewOutlineBuilder.h"

#include <exception>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace slicer_core::api::viewdata_detail
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

ApiResult<SceneViewData> Cancelled(const SceneId sceneId)
{
    return Failure<SceneViewData>(
        "PM-SLICER-CANCELLED-0070",
        "textured ViewData request was cancelled",
        std::to_string(sceneId));
}

ResolvedViewAppearance BudgetAppearance(
    const ResolvedViewAppearance& source,
    const std::size_t maxTextureEdgePx)
{
    return maxTextureEdgePx == 0U
        ? source
        : DownsampleAppearanceTextures(source, maxTextureEdgePx);
}

ApiResult<SurfacePreview> BuildOutlinePreview(
    const SceneModel& model,
    const ResolvedViewAppearance& appearance,
    const int requestedDimension,
    const ICancelToken& cancelToken)
{
    constexpr int outlineDimension{256};
    return BuildSurfacePreview(
        model,
        appearance,
        requestedDimension > 0 ? requestedDimension : outlineDimension,
        cancelToken);
}

void MarkTextureDegradation(
    const ResolvedViewAppearance& source,
    const ResolvedViewAppearance& candidate,
    bool& degraded,
    std::string& reason)
{
    if (source.appearance.appearance_identity
        == candidate.appearance.appearance_identity)
    {
        return;
    }
    degraded = true;
    if (!reason.empty())
    {
        reason += ";";
    }
    reason += "texture_resolution_reduced_for_max_bytes";
}

}  // namespace

ApiResult<SceneViewData> BuildViewCandidate(
    const SceneViewDataRequest& request,
    const SceneSnapshot& snapshot,
    const std::vector<PreparedViewInstance>& prepared,
    const ViewCandidateOptions& options,
    const ICancelToken& cancelToken) noexcept
{
    try
    {
        SceneViewData result;
        result.view_mode = request.view_mode;
        result.scene_revision = snapshot.scene_revision;
        result.truncated = options.degraded;
        result.truncation_reason = options.degradation_reason;

        std::map<ModelId, ResolvedViewAppearance> budgetedAppearances;
        std::map<std::string, std::size_t> appearanceIndices;
        std::map<ModelId, std::size_t> localMeshIndices;
        std::map<std::string, std::size_t> meshIndices;
        for (const PreparedViewInstance& preparedInstance : prepared)
        {
            if (cancelToken.IsCancelRequested())
            {
                return Cancelled(snapshot.scene_id);
            }

            const PreparedViewModel& preparedModel = *preparedInstance.model;
            auto budgeted = budgetedAppearances.find(
                preparedInstance.state.instance.model_id);
            if (budgeted == budgetedAppearances.end())
            {
                ResolvedViewAppearance appearance = BudgetAppearance(
                    preparedModel.appearance,
                    options.max_texture_edge_px);
                MarkTextureDegradation(
                    preparedModel.appearance,
                    appearance,
                    result.truncated,
                    result.truncation_reason);
                budgeted = budgetedAppearances.emplace(
                    preparedInstance.state.instance.model_id,
                    std::move(appearance)).first;
            }
            const ResolvedViewAppearance& appearance = budgeted->second;
            const std::string& appearanceIdentity =
                appearance.appearance.appearance_identity;
            if (!appearanceIndices.contains(appearanceIdentity))
            {
                appearanceIndices.emplace(
                    appearanceIdentity,
                    result.appearances.size());
                result.appearances.push_back(appearance.appearance);
            }

            ViewInstance instance;
            instance.instance_id =
                preparedInstance.state.instance.instance_id;
            instance.model_id = preparedInstance.state.instance.model_id;
            const Matrix4d sourceWorldMatrix =
                preparedInstance.state.instance.world_matrix;
            instance.world_matrix = sourceWorldMatrix;
            instance.local_bounds_mm = LocalBounds(*preparedModel.model);
            instance.texture_status = appearance.has_texture
                ? TextureStatus::Available
                : TextureStatus::NotProvided;
            instance.appearance_identity = appearanceIdentity;

            ApiResult<SurfacePreview> outlinePreview = BuildOutlinePreview(
                *preparedModel.model,
                appearance,
                options.preview_dimension,
                cancelToken);
            if (!outlinePreview.IsOk())
            {
                return Failure<SceneViewData>(
                    outlinePreview.Error()->code,
                    outlinePreview.Error()->message,
                    outlinePreview.Error()->detail);
            }
            ApiResult<std::vector<ViewOutline>> outlines =
                BuildViewOutlines(*outlinePreview.Value());
            if (!outlines.IsOk())
            {
                return Failure<SceneViewData>(
                    outlines.Error()->code,
                    outlines.Error()->message,
                    outlines.Error()->detail);
            }
            instance.outlines = std::move(*outlines.Value());

            if (request.view_mode == ViewMode::Top)
            {
                instance.preview_identity =
                    outlinePreview.Value()->preview_identity;
                instance.surface_preview =
                    std::move(*outlinePreview.Value());
            }
            else
            {
                const ViewMesh* resolvedMesh{nullptr};
                if (request.mesh_transform == MeshTransform::Local)
                {
                    const auto cached = localMeshIndices.find(
                        preparedInstance.state.instance.model_id);
                    if (cached != localMeshIndices.end())
                    {
                        resolvedMesh = &result.meshes.at(cached->second);
                    }
                }
                if (resolvedMesh == nullptr)
                {
                    ApiResult<ViewMesh> mesh = BuildViewMesh(
                        *preparedModel.model,
                        appearance,
                        sourceWorldMatrix,
                        options.lod,
                        request.mesh_transform,
                        cancelToken);
                    if (!mesh.IsOk())
                    {
                        return Failure<SceneViewData>(
                            mesh.Error()->code,
                            mesh.Error()->message,
                            mesh.Error()->detail);
                    }
                    auto cached = meshIndices.find(
                        mesh.Value()->mesh_identity);
                    if (cached == meshIndices.end())
                    {
                        const std::size_t meshIndex = result.meshes.size();
                        meshIndices.emplace(
                            mesh.Value()->mesh_identity,
                            meshIndex);
                        result.meshes.push_back(std::move(*mesh.Value()));
                        cached = meshIndices.find(
                            result.meshes.back().mesh_identity);
                    }
                    resolvedMesh = &result.meshes.at(cached->second);
                    if (request.mesh_transform == MeshTransform::Local)
                    {
                        localMeshIndices.emplace(
                            preparedInstance.state.instance.model_id,
                            cached->second);
                    }
                }
                instance.mesh_identity = resolvedMesh->mesh_identity;
                if (request.mesh_transform == MeshTransform::World)
                {
                    instance.world_matrix = Matrix4d{};
                }
            }
            result.instances.push_back(std::move(instance));
        }

        const ApiResult<void> closure = ValidateViewDataClosure(result);
        if (!closure.IsOk())
        {
            return Failure<SceneViewData>(
                closure.Error()->code,
                closure.Error()->message,
                closure.Error()->detail);
        }
        result.viewdata_identity = ComputeViewDataIdentity(result);
        return ApiResult<SceneViewData>::Success(std::move(result));
    }
    catch (const std::exception& error)
    {
        return Failure<SceneViewData>(
            "PM-SLICER-INTERNAL-0099",
            "failed to build textured Scene ViewData candidate",
            error.what());
    }
    catch (...)
    {
        return Failure<SceneViewData>(
            "PM-SLICER-INTERNAL-0099",
            "failed to build textured Scene ViewData candidate",
            "unknown exception");
    }
}

}  // namespace slicer_core::api::viewdata_detail
