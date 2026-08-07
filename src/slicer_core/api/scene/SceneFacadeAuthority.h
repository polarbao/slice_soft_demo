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
 * @brief Builds and validates the initial authoritative state.
 * @param seed Scene identity, source geometry, and existing scene state.
 * @return Validated state or a stable PM-SLICER error.
 */
[[nodiscard]] ApiResult<AuthorityState> BuildAuthorityState(
    SceneFacadeSeed seed) noexcept;

/**
 * @brief Applies a complete operation batch to an isolated candidate state.
 * @param current Current authoritative state copied for atomic evaluation.
 * @param request Ordered operation batch from the Commit lane.
 * @param cancelToken Cooperative cancellation token.
 * @return Fully evaluated candidate or an error without changing current.
 */
[[nodiscard]] ApiResult<AuthorityState> ApplyOperationBatch(
    const AuthorityState& current,
    const SceneOperationRequest& request,
    const ICancelToken& cancelToken) noexcept;

/**
 * @brief Applies one complete grid layout to an isolated authority candidate.
 * @param candidate Candidate authority to update only after full validation.
 * @param layout Frozen 11x2 row-major layout settings.
 * @return Changed instance identities or one fail-closed layout error.
 */
[[nodiscard]] ApiResult<std::vector<std::string>> ApplyGridLayout(
    AuthorityState& candidate,
    const SceneLayout& layout) noexcept;

/**
 * @brief Computes the canonical request identity used by operation replay.
 * @param request Commit request represented by the current internal DTO.
 * @return SHA-256 identity of its canonical request representation.
 */
[[nodiscard]] std::string ComputeOperationFingerprint(
    const SceneOperationRequest& request);

}  // namespace slicer_core::api::scene_facade_detail
