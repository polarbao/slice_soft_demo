#include "slicer_module/SceneCapabilitySerializationAdapter.h"

#include "slicer_module/CapabilityJsonAdapter.h"

namespace slicesoft::module
{

slicer_core::Json SceneCapabilitySerializationAdapter::SerializeCommit(
    const slicer_core::api::SceneCommitResult& result)
{
    slicer_core::Json::Object fields = SerializeSnapshotFields(result.snapshot);
    fields.erase("scene");
    fields.erase("sceneRevision");
    fields.emplace("newSceneRevision", result.snapshot.scene_revision);
    const auto collision = SerializeCollisionFields(result.collision_report);
    fields.insert(collision.begin(), collision.end());
    fields.emplace("warnings", MakeStringArray(result.warnings));
    slicer_core::Json::Array delta;
    for (const auto& item : result.preflight_delta)
    {
        delta.emplace_back(ParseStructuredObject(item));
    }
    fields.emplace("preflightDelta", slicer_core::Json{std::move(delta)});
    fields.emplace("viewdataIdentity", result.viewdata_identity);
    return MakeSuccess(std::move(fields));
}

slicer_core::Json SceneCapabilitySerializationAdapter::SerializeSnapshot(
    const slicer_core::api::SceneSnapshot& snapshot)
{
    return MakeSuccess(SerializeSnapshotFields(snapshot));
}

slicer_core::Json::Object
SceneCapabilitySerializationAdapter::SerializeSnapshotFields(
    const slicer_core::api::SceneSnapshot& snapshot)
{
    slicer_core::Json::Array instances;
    for (const auto& state : snapshot.instances)
    {
        instances.emplace_back(slicer_core::Json::object({
            {"instanceId", state.instance.instance_id},
            {"modelId", std::to_string(state.instance.model_id)},
            {"canonicalTransform", slicer_core::Json::object({
                {"worldMatrix", MakeMatrix(state.instance.world_matrix)}})},
            {"effectiveBBoxMm", MakeBounds(state.effective_bounds_mm)},
            {"outOfBounds", state.out_of_bounds}}));
    }
    slicer_core::Json::Object buildVolume{
        {"widthMm", snapshot.build_volume.width_mm},
        {"heightMm", snapshot.build_volume.height_mm}};
    if (snapshot.build_volume.z_limit_mm)
    {
        buildVolume.emplace("zLimitMm", *snapshot.build_volume.z_limit_mm);
    }
    slicer_core::Json scene = slicer_core::Json::object({
        {"sceneId", std::to_string(snapshot.scene_id)},
        {"sceneRevision", snapshot.scene_revision},
        {"instances", slicer_core::Json{instances}}});
    return {
        {"scene", std::move(scene)},
        {"sceneRevision", snapshot.scene_revision},
        {"sceneHash", snapshot.scene_hash},
        {"instances", slicer_core::Json{std::move(instances)}},
        {"buildVolume", slicer_core::Json{std::move(buildVolume)}}};
}

slicer_core::Json SceneCapabilitySerializationAdapter::SerializeCollision(
    const std::uint64_t revision,
    const slicer_core::api::CollisionReport& report)
{
    auto fields = SerializeCollisionFields(report);
    fields.emplace("sceneRevision", revision);
    return MakeSuccess(std::move(fields));
}

slicer_core::Json::Object
SceneCapabilitySerializationAdapter::SerializeCollisionFields(
    const slicer_core::api::CollisionReport& report)
{
    slicer_core::Json::Array collisions;
    for (const auto& pair : report.collisions)
    {
        collisions.emplace_back(slicer_core::Json::object({
            {"a", pair.instance_a},
            {"b", pair.instance_b}}));
    }
    return {
        {"collisions", slicer_core::Json{std::move(collisions)}},
        {"outOfBoundsInstances", MakeStringArray(
            report.out_of_bounds_instances)}};
}

}  // namespace slicesoft::module
