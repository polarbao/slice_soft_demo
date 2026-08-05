#pragma once

#include "slicer_core/api/CommonDtos.h"

#include <cstdint>
#include <string>
#include <vector>

namespace slicer_core::api {

enum class SceneOperationType
{
    Translate,
    RotateZ,
    UniformScale,
    MirrorX,
    MirrorY
};

/** @brief One canonical scene edit submitted on the Commit lane. */
struct SceneOperation
{
    SceneOperationType type{SceneOperationType::Translate};
    std::string instance_id;
    double value_x{0.0};
    double value_y{0.0};
    double value_z{0.0};
};

/** @brief Atomic scene operation request with optimistic concurrency. */
struct SceneOperationRequest
{
    SceneId scene_id{0};
    std::string operation_id;
    std::uint64_t expected_scene_revision{0};
    std::vector<SceneOperation> operations;
};

/** @brief Authoritative committed state for one instance. */
struct SceneInstanceState
{
    InstanceReference instance;
    Bounds3d effective_bounds_mm;
    bool out_of_bounds{false};
};

/** @brief Authoritative scene snapshot returned by base services. */
struct SceneSnapshot
{
    SceneId scene_id{0};
    std::uint64_t scene_revision{0};
    std::string scene_hash;
    std::vector<SceneInstanceState> instances;
};

/** @brief Authoritative collision pair. */
struct CollisionPair
{
    std::string instance_a;
    std::string instance_b;
};

/** @brief Collision and build-volume evaluation result. */
struct CollisionReport
{
    std::vector<CollisionPair> collisions;
    std::vector<std::string> out_of_bounds_instances;
};

}  // namespace slicer_core::api
