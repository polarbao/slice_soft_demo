#pragma once

#include "slicer_core/api/scene/SceneFacadeService.h"
#include "slicer_core/json_value.h"

#include <cstdint>

namespace slicesoft::module
{

/** @brief Serializes authoritative SceneFacade results to frozen capability DTOs. */
class SceneCapabilitySerializationAdapter final
{
public:
    /** @brief Serializes a committed scene operation. @param result Facade result. @return Capability envelope. */
    [[nodiscard]] static slicer_core::Json SerializeCommit(
        const slicer_core::api::SceneCommitResult& result);

    /** @brief Serializes a scene snapshot. @param snapshot Facade snapshot. @return Capability envelope. */
    [[nodiscard]] static slicer_core::Json SerializeSnapshot(
        const slicer_core::api::SceneSnapshot& snapshot);

    /** @brief Serializes a collision report. @param revision Scene revision. @param report Report. @return Capability envelope. */
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
