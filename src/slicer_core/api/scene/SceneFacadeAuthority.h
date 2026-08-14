#pragma once

#include "slicer_core/api/ApiResult.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SceneDtos.h"
#include "slicer_core/api/scene/SceneFacadeService.h"
#include "slicer_core/layout/SceneCollisionService.h"

#include <string>
#include <vector>

namespace slicer_core::api::scene_facade_detail
{

struct AuthorityState
{
    SceneFacadeSeed seed;
    SceneSnapshot snapshot;
    CollisionReport collision_report;
    std::vector<std::string> warnings;
    std::vector<StructuredJsonObject> preflight_delta;
};

/**
 * @brief 构造并验证初始权威状态。
 * @param seed 场景标识、源几何和现有场景状态。
 * @return 已验证状态或稳定的 PM-SLICER 错误。
 */
[[nodiscard]] ApiResult<AuthorityState> BuildAuthorityState(
    SceneFacadeSeed seed) noexcept;

/**
 * @brief 将完整操作批次应用到隔离的候选状态。
 * @param current 为原子评估复制的当前权威状态。
 * @param request 来自 Commit 通道的有序操作批次。
 * @param cancelToken 协作式取消令牌。
 * @return 完整评估后的候选状态；失败时返回错误，且不修改 current。
 */
[[nodiscard]] ApiResult<AuthorityState> ApplyOperationBatch(
    const AuthorityState& current,
    const SceneOperationRequest& request,
    const ICancelToken& cancelToken) noexcept;

/**
 * @brief 将一个完整栅格排版应用到隔离的权威候选状态。
 * @param candidate 仅在完整验证后更新的权威候选状态。
 * @param layout 冻结的 11x2 行主序排版设置。
 * @return 已变更实例标识，或一个失败即拒绝的排版错误。
 */
[[nodiscard]] ApiResult<std::vector<std::string>> ApplyGridLayout(
    AuthorityState& candidate,
    const SceneLayout& layout) noexcept;

/**
 * @brief 计算操作重放使用的规范请求标识。
 * @param request 由当前内部 DTO 表示的 Commit 请求。
 * @return 请求规范表示的 SHA-256 标识。
 */
[[nodiscard]] std::string ComputeOperationFingerprint(
    const SceneOperationRequest& request);

}  // namespace slicer_core::api::scene_facade_detail
