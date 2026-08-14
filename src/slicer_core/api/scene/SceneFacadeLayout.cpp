#include "slicer_core/api/scene/SceneFacadeAuthority.h"

#include "slicer_core/layout/GridLayoutPolicy.h"

#include <cstddef>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

// 文件职责：将冻结排版策略应用到隔离候选场景；
// 边界：只有整批排版验证通过后才允许候选状态进入提交阶段。
namespace slicer_core::api::scene_facade_detail
{
namespace
{

template <class T>
ApiResult<T> Failure(
    const std::string_view code,
    const GridLayoutError& error)
{
    return ApiResult<T>::Failure({
        std::string(code),
        error.message,
        !error.instanceid.empty() ? error.instanceid : error.field});
}

std::string_view ErrorCode(const GridLayoutErrorCode code)
{
    switch (code)
    {
    case GridLayoutErrorCode::InstanceCapacityExceeded:
        return "PM-SLICER-LAYOUT-0023";
    case GridLayoutErrorCode::SceneRevisionStale:
        return "PM-SLICER-LAYOUT-0022";
    case GridLayoutErrorCode::LockedInstanceConflict:
        return "PM-SLICER-LAYOUT-0020";
    case GridLayoutErrorCode::ParameterOutOfRange:
    case GridLayoutErrorCode::InstanceBoundsInvalid:
    case GridLayoutErrorCode::InstanceNotFound:
        return "PM-SLICER-PROFILE-0031";
    case GridLayoutErrorCode::None:
        break;
    }
    return "PM-SLICER-INTERNAL-0099";
}

}  // namespace

ApiResult<std::vector<std::string>> ApplyGridLayout(
    AuthorityState& candidate,
    const SceneLayout& layout) noexcept
{
    try
    {
        GridLayoutRequest request;
        request.layout = layout;
        request.currentscenerevision = candidate.seed.scene.scenerevision;
        request.expectedscenerevision = candidate.seed.scene.scenerevision;
        request.items.reserve(candidate.seed.scene.instances.size());
        for (const SceneModelInstance& instance :
             candidate.seed.scene.instances)
        {
            GridLayoutItem item;
            item.instance = instance.instance;
            item.requestedtransform = instance.requestedtransform;
            item.currentderivedlayouttransform =
                instance.derivedlayouttransform;
            request.items.push_back(std::move(item));
        }

        const GridLayoutResult result = ComputeGridLayout(request);
        if (!result.IsValid())
        {
            return Failure<std::vector<std::string>>(
                ErrorCode(result.error->code),
                *result.error);
        }

        std::vector<std::string> affectedInstances;
        affectedInstances.reserve(result.placements.size());
        for (std::size_t index = 0U;
             index < result.placements.size();
             ++index)
        {
            SceneModelInstance& instance =
                candidate.seed.scene.instances[index];
            const GridLayoutPlacement& placement =
                result.placements[index];
            instance.requestedtransform = placement.requestedtransform;
            instance.derivedlayouttransform =
                placement.derivedlayouttransform;
            instance.effectivetransform = placement.effectivetransform;
            affectedInstances.push_back(instance.instance.instanceid);
        }
        candidate.seed.scene.layout = layout;
        return ApiResult<std::vector<std::string>>::Success(
            std::move(affectedInstances));
    }
    catch (const std::exception& error)
    {
        return ApiResult<std::vector<std::string>>::Failure({
            "PM-SLICER-INTERNAL-0099",
            "failed to apply authoritative grid layout",
            error.what()});
    }
    catch (...)
    {
        return ApiResult<std::vector<std::string>>::Failure({
            "PM-SLICER-INTERNAL-0099",
            "failed to apply authoritative grid layout",
            "unknown exception"});
    }
}

}  // namespace slicer_core::api::scene_facade_detail
