#pragma once

#include "slicer_core/api/scene/SceneFacadeService.h"
#include "slicer_core/json_value.h"

#include <cstdint>

namespace slicesoft::module
{

/** @brief 将权威 SceneFacade 结果序列化为冻结能力 DTO。 */
class SceneCapabilitySerializationAdapter final
{
public:
    /** @brief 序列化已提交的场景操作。 @param result Facade 返回结果。 @return 能力响应对象。 */
    [[nodiscard]] static slicer_core::Json SerializeCommit(
        const slicer_core::api::SceneCommitResult& result);

    /** @brief 序列化场景快照。 @param snapshot Facade 返回的快照。 @return 能力响应对象。 */
    [[nodiscard]] static slicer_core::Json SerializeSnapshot(
        const slicer_core::api::SceneSnapshot& snapshot);

    /** @brief 序列化碰撞报告。 @param revision 场景修订号。 @param report 报告。 @return 能力响应对象。 */
    [[nodiscard]] static slicer_core::Json SerializeCollision(
        std::uint64_t revision,
        const slicer_core::api::CollisionReport& report);

private:
    [[nodiscard]] static slicer_core::Json::Object SerializeSnapshotFields(
        const slicer_core::api::SceneSnapshot& snapshot);
    [[nodiscard]] static slicer_core::Json::Object SerializeCollisionFields(
        const slicer_core::api::CollisionReport& report);
};

}  // namespace slicesoft::module
