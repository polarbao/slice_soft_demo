#pragma once

#include "slicer_core/api/CommonDtos.h"
#include "slicer_core/scene/ModelTransform.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace slicer_core::api {

enum class SceneOperationType
{
    AddInstance,
    RemoveInstance,
    ApplyGridLayout,
    Translate,
    RotateX,
    RotateY,
    RotateZ,
    UniformScale,
    MirrorX,
    MirrorY,
    LandOnBuildPlate
};

/** @brief 在 Commit 通道提交的一个规范场景编辑。 */
struct SceneOperation
{
    SceneOperationType type{SceneOperationType::Translate};
    std::string instance_id;
    ModelId model_id{0};
    ModelTransform initial_transform;
    SceneLayout layout;
    double value_x{0.0};
    double value_y{0.0};
    double value_z{0.0};
};

/** @brief 带乐观并发控制的原子场景操作请求。 */
struct SceneOperationRequest
{
    SceneId scene_id{0};
    std::string operation_id;
    std::uint64_t current_scene_revision{0};
    std::uint64_t expected_scene_revision{0};
    std::string scene_context_identity;
    std::vector<SceneOperation> operations;
};

/** @brief 随权威状态返回的已解析构建体积尺寸。 */
struct SceneBuildVolumeDescriptor
{
    double width_mm{0.0};
    double height_mm{0.0};
    std::optional<double> z_limit_mm;
};

/** @brief 一个实例的权威已提交状态。 */
struct SceneInstanceState
{
    InstanceReference instance;
    Bounds3d effective_bounds_mm;
    bool out_of_bounds{false};
};

/** @brief 基础服务返回的权威场景快照。 */
struct SceneSnapshot
{
    SceneId scene_id{0};
    std::uint64_t scene_revision{0};
    std::string scene_hash;
    MultiModelScene scene;
    SceneBuildVolumeDescriptor build_volume;
    std::vector<SceneInstanceState> instances;
};

/** @brief 权威碰撞对。 */
struct CollisionPair
{
    std::string instance_a;
    std::string instance_b;
};

/** @brief 碰撞及构建体积评估结果。 */
struct CollisionReport
{
    std::vector<CollisionPair> collisions;
    std::vector<std::string> out_of_bounds_instances;
};

/** @brief 无需后续查询快照的完整 Commit 响应。 */
struct SceneCommitResult
{
    SceneSnapshot snapshot;
    CollisionReport collision_report;
    std::vector<std::string> warnings;
    std::vector<StructuredJsonObject> preflight_delta;
    std::string viewdata_identity;
};

}  // namespace slicer_core::api
