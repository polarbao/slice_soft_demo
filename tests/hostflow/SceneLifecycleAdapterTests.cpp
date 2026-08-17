#include "contracts/print_module_spi.h"
#include "slicer_core/json_value.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

void Require(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "HOSTFLOW H-A-02: " << message << '\n';
        std::exit(1);
    }
}

std::string ReadBuffer(
    int (*reader)(char*, int, int*),
    const std::string& label)
{
    int required{0};
    Require(
        reader(nullptr, 0, &required) == PM_ERR_BUFFER_SMALL
            && required >= 0,
        label + " probe failed");
    std::vector<char> buffer(
        static_cast<std::size_t>(required) + 1U,
        '\0');
    Require(
        reader(buffer.data(), static_cast<int>(buffer.size()), nullptr)
            == required,
        label + " read failed");
    return {buffer.data()};
}

std::string RunJob(pm_module_t* module, const std::string& request)
{
    pm_job_t* const job = pm_submit(module, request.c_str());
    if (job == nullptr)
    {
        const std::string error = ReadBuffer(pm_last_error, "last error");
        Require(false, "pm_submit rejected request: " + error);
    }

    int required{0};
    Require(
        pm_result(job, nullptr, 0, &required) == PM_ERR_BUFFER_SMALL
            && required >= 0,
        "pm_result probe failed");
    std::vector<char> output(
        static_cast<std::size_t>(required) + 1U,
        '\0');
    Require(
        pm_result(
            job,
            output.data(),
            static_cast<int>(output.size()),
            nullptr) == required,
        "pm_result read failed");
    pm_release(job);
    return {output.data()};
}

slicer_core::Json Parse(const std::string& text)
{
    std::istringstream input{text};
    return slicer_core::Json::parse(input);
}

void RequireSuccess(
    const slicer_core::Json& result,
    const std::string& label)
{
    Require(
        result.is_object()
            && result.contains("ok")
            && result.at("ok").as_bool(),
        label + " should succeed: " + result.dump(0));
}

void RequireFailure(
    const slicer_core::Json& result,
    const std::string& code,
    const std::string& label)
{
    Require(
        result.is_object()
            && result.contains("ok")
            && !result.at("ok").as_bool()
            && result.at("code").as_string() == code,
        label + " should fail with " + code + ": " + result.dump(0));
}

std::string SceneContext(const double widthMm = 230.0)
{
    std::ostringstream context;
    context << "{\"resolvedProfileId\":\"profile-hostflow\","
            << "\"buildVolume\":{\"source\":\"device_profile\","
            << "\"widthMm\":" << widthMm
            << ",\"heightMm\":100,\"zLimitMm\":60,"
            << "\"origin\":\"lower_left\","
            << "\"xDirection\":\"positive\","
            << "\"yDirection\":\"positive\","
            << "\"isFixture\":false}}";
    return context.str();
}

std::string AddRequest(
    const std::string& operationId,
    const std::string& modelId,
    const std::string& context,
    const std::string& assignedId = {})
{
    std::ostringstream request;
    request << "{\"capability\":\"scene.apply_operation\","
            << "\"operationId\":\"" << operationId << "\","
            << "\"sceneContext\":" << context << ','
            << "\"currentSceneRevision\":0,"
            << "\"expectedSceneRevision\":0,"
            << "\"operations\":[{\"type\":\"addInstance\","
            << "\"modelId\":\"" << modelId << "\"";
    if (!assignedId.empty())
    {
        request << ",\"assignInstanceId\":\"" << assignedId << "\"";
    }
    request << ",\"initialTransform\":{\"translateXMm\":10,"
            << "\"translateYMm\":12,\"rotateXDeg\":2,"
            << "\"rotateYDeg\":-3,\"rotateZDeg\":5,"
            << "\"uniformScale\":1,\"mirrorX\":false,"
            << "\"mirrorY\":false}}]}";
    return request.str();
}

void VerifiesImplicitSceneLifecycle(
    pm_module_t* module,
    const std::filesystem::path& fixture)
{
    const std::string importRequest =
        "{\"capability\":\"model.import\",\"modelPath\":\""
        + fixture.generic_string()
        + "\",\"options\":{\"computeBBox\":true,"
          "\"extractMaterials\":true}}";
    const slicer_core::Json imported = Parse(RunJob(module, importRequest));
    RequireSuccess(imported, "model.import");
    const std::string modelId = imported.at("modelId").as_string();

    const std::string addRequest = AddRequest(
        "hostflow-add",
        modelId,
        SceneContext());
    const slicer_core::Json added = Parse(RunJob(module, addRequest));
    RequireSuccess(added, "implicit addInstance");
    Require(
        added.contains("sceneHandle")
            && added.at("newSceneRevision").as_int() == 1
            && added.at("instances").size() == 1U,
        "implicit addInstance must return handle, revision, and instance");
    const int sceneHandle = added.at("sceneHandle").as_int();
    const std::string instanceId =
        added.at("instances").at(0U).at("instanceId").as_string();

    const slicer_core::Json replay = Parse(RunJob(module, addRequest));
    RequireSuccess(replay, "implicit addInstance replay");
    Require(
        replay.at("sceneHandle").as_int() == sceneHandle
            && replay.at("newSceneRevision").as_int() == 1
            && replay.at("instances").size() == 1U,
        "exact replay must return the original handle without duplication");

    const slicer_core::Json changedContext = Parse(RunJob(
        module,
        AddRequest("hostflow-add", modelId, SceneContext(240.0))));
    RequireFailure(
        changedContext,
        "PM-SLICER-PROFILE-0031",
        "changed sceneContext replay");

    const std::string transformRequest =
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"hostflow-transform\","
        "\"sceneHandle\":" + std::to_string(sceneHandle)
        + ",\"currentSceneRevision\":1,\"expectedSceneRevision\":1,"
          "\"operations\":[{\"type\":\"translate\","
          "\"instanceId\":\"" + instanceId
        + "\",\"deltaMm\":[3,4,0]},{\"type\":\"rotateX\","
          "\"instanceId\":\"" + instanceId
        + "\",\"degrees\":8},{\"type\":\"rotateY\","
          "\"instanceId\":\"" + instanceId
        + "\",\"degrees\":-6},{\"type\":\"rotateZ\","
          "\"instanceId\":\"" + instanceId
        + "\",\"degrees\":15}]}";
    const slicer_core::Json transformed = Parse(RunJob(
        module,
        transformRequest));
    RequireSuccess(transformed, "same-session transform");
    Require(
        transformed.at("newSceneRevision").as_int() == 2,
        "transform must advance the implicit scene revision");

    const std::string removeRequest =
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"hostflow-remove\","
        "\"sceneHandle\":" + std::to_string(sceneHandle)
        + ",\"currentSceneRevision\":2,\"expectedSceneRevision\":2,"
          "\"operations\":[{\"type\":\"removeInstance\","
          "\"instanceId\":\"" + instanceId + "\"}]}";
    const slicer_core::Json removed = Parse(RunJob(module, removeRequest));
    RequireSuccess(removed, "removeInstance");
    Require(
        removed.at("newSceneRevision").as_int() == 3
            && removed.at("instances").size() == 0U,
        "removeInstance must retain the scene and remove only the instance");

    const std::string removeMissingRequest =
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"hostflow-remove-missing\","
        "\"sceneHandle\":" + std::to_string(sceneHandle)
        + ",\"currentSceneRevision\":3,\"expectedSceneRevision\":3,"
          "\"operations\":[{\"type\":\"removeInstance\","
          "\"instanceId\":\"missing\"}]}";
    RequireFailure(
        Parse(RunJob(module, removeMissingRequest)),
        "PM-SLICER-PROFILE-0031",
        "remove missing instance");

    const std::string addExistingRequest =
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"hostflow-add-existing\","
        "\"sceneHandle\":" + std::to_string(sceneHandle)
        + ",\"currentSceneRevision\":3,\"expectedSceneRevision\":3,"
          "\"operations\":[{\"type\":\"addInstance\","
          "\"modelId\":\"" + modelId
        + "\",\"assignInstanceId\":\"instance-again\"}]}";
    const slicer_core::Json addedAgain = Parse(RunJob(
        module,
        addExistingRequest));
    RequireSuccess(addedAgain, "add model after instance removal");
    Require(
        addedAgain.at("newSceneRevision").as_int() == 4
            && addedAgain.at("instances").size() == 1U,
        "removeInstance must not release the imported model");

    const std::string duplicateRequest =
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"hostflow-add-duplicate\","
        "\"sceneHandle\":" + std::to_string(sceneHandle)
        + ",\"currentSceneRevision\":4,\"expectedSceneRevision\":4,"
          "\"operations\":[{\"type\":\"addInstance\","
          "\"modelId\":\"" + modelId
        + "\",\"assignInstanceId\":\"instance-again\"}]}";
    RequireFailure(
        Parse(RunJob(module, duplicateRequest)),
        "PM-SLICER-PROFILE-0031",
        "duplicate assigned instance identity");

    const std::string missingContextRequest =
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"hostflow-missing-context\","
        "\"currentSceneRevision\":0,\"expectedSceneRevision\":0,"
        "\"operations\":[{\"type\":\"addInstance\","
        "\"modelId\":\"" + modelId + "\"}]}";
    RequireFailure(
        Parse(RunJob(module, missingContextRequest)),
        "PM-SLICER-INPUT-0002",
        "implicit create without sceneContext");

    const std::string handleWithContextRequest =
        "{\"capability\":\"scene.apply_operation\","
        "\"operationId\":\"hostflow-context-with-handle\","
        "\"sceneHandle\":" + std::to_string(sceneHandle)
        + ",\"sceneContext\":" + SceneContext()
        + ",\"currentSceneRevision\":4,\"expectedSceneRevision\":4,"
          "\"operations\":[{\"type\":\"translate\","
          "\"instanceId\":\"instance-again\","
          "\"deltaMm\":[1,0,0]}]}";
    RequireFailure(
        Parse(RunJob(module, handleWithContextRequest)),
        "PM-SLICER-INPUT-0002",
        "existing handle with sceneContext");
}

}  // namespace

int main(const int argc, char** argv)
{
    Require(argc == 2, "fixture path argument is required");
    pm_module_t* const module = pm_create(nullptr);
    Require(module != nullptr, "pm_create failed");
    VerifiesImplicitSceneLifecycle(
        module,
        std::filesystem::absolute(argv[1]).lexically_normal());
    pm_destroy(module);
    std::cout << "HOSTFLOW H-A-02 scene lifecycle tests: PASS\n";
    return 0;
}
