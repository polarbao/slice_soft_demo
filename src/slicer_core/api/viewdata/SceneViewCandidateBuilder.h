#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneDtos.h"
#include "slicer_core/api/SceneViewDtos.h"
#include "slicer_core/api/viewdata/SceneViewResolvedAsset.h"
#include "slicer_core/model.h"

#include <memory>
#include <string>
#include <vector>

namespace slicer_core::api::viewdata_detail
{

/** @brief 已导入模型及其按失败即拒绝规则解析的外观。 */
struct PreparedViewModel
{
    std::shared_ptr<const SceneModel> model;
    ResolvedViewAppearance appearance;
};

/** @brief 绑定到一个已准备模型的场景实例。 */
struct PreparedViewInstance
{
    SceneInstanceState state;
    std::shared_ptr<const PreparedViewModel> model;
};

/** @brief ViewData 预算策略请求的一个有界候选项。 */
struct ViewCandidateOptions
{
    int preview_dimension{0};
    ViewLod lod{ViewLod::Lod0};
    std::size_t max_texture_edge_px{0U};
    bool degraded{false};
    std::string degradation_reason;
};

/**
 * @brief 构造闭合的纹理 ViewData 候选项。
 * @param request 已验证的 ViewData 请求。
 * @param snapshot 权威场景快照。
 * @param prepared 已选择并准备的场景实例。
 * @param options 此候选项的分辨率和降级选择。
 * @param cancelToken 协作式取消令牌。
 * @return 闭合候选项或稳定的 PM-SLICER 错误。
 */
[[nodiscard]] ApiResult<SceneViewData> BuildViewCandidate(
    const SceneViewDataRequest& request,
    const SceneSnapshot& snapshot,
    const std::vector<PreparedViewInstance>& prepared,
    const ViewCandidateOptions& options,
    const ICancelToken& cancelToken) noexcept;

}  // namespace slicer_core::api::viewdata_detail
