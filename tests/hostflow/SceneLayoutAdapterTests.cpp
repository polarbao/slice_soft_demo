#include "contracts/print_module_spi.h"
#include "slicer_core/json_value.h"

#include <cmath>
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
        std::cerr << "HOSTFLOW H-A-04: " << message << '\n';
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
        return ReadBuffer(
            pm_last_error,
            "last error");
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
            && (!result.contains("ok") || !result.at("ok").as_bool())
            && result.at("code").as_string() == code,
        label + " should fail with " + code + ": " + result.dump(0));
}

bool ApproximatelyEqual(const double left, const double right)
{
    return std::abs(left - right) <= 1.0e-9;
}

std::string ImportModel(
    pm_module_t* module,
    const std::filesystem::path& fixture)
{
    const slicer_core::Json imported = Parse(RunJob(
        module,
        "{\"capability\":\"model.import\",\"modelPath\":\""
            + fixture.generic_string()
            + "\",\"options\":{\"computeBBox\":true,"
              "\"extractMaterials\":true}}"));
    RequireSuccess(imported, "model.import");
    return imported.at("modelId").as_string();
}

slicer_core::Json AddTwentyTwoInstances(
    pm_module_t* module,
    const std::string& modelId)
{
    std::ostringstream request;
    request << "{\"capability\":\"scene.apply_operation\","
            << "\"operationId\":\"hostflow-layout-add\","
            << "\"sceneContext\":{"
            << "\"resolvedProfileId\":\"profile-hostflow-layout\","
            << "\"buildVolume\":{\"source\":\"device_profile\","
            << "\"widthMm\":230,\"heightMm\":100,\"zLimitMm\":60,"
            << "\"origin\":\"lower_left\","
            << "\"xDirection\":\"positive\","
            << "\"yDirection\":\"positive\",\"isFixture\":false}},"
            << "\"currentSceneRevision\":0,"
            << "\"expectedSceneRevision\":0,\"operations\":[";
    for (int index = 1; index <= 22; ++index)
    {
        if (index > 1)
        {
            request << ',';
        }
        request << "{\"type\":\"addInstance\",\"modelId\":\""
                << modelId << "\",\"assignInstanceId\":\"instance-"
                << index << "\"}";
    }
    request << "]}";
    return Parse(RunJob(module, request.str()));
}

std::string LayoutRequest(
    const int sceneHandle,
    const std::string& operationId,
    const int revision,
    const int columns,
    const int rows,
    const double columnGap,
    const double rowGap,
    const std::string& policy = "grid",
    const std::string& order = "row_major")
{
    std::ostringstream request;
    request << "{\"capability\":\"scene.apply_operation\","
            << "\"operationId\":\"" << operationId << "\","
            << "\"sceneHandle\":" << sceneHandle << ','
            << "\"currentSceneRevision\":" << revision << ','
            << "\"expectedSceneRevision\":" << revision
            << ",\"operations\":[{"
            << "\"type\":\"applyGridLayout\",\"layout\":{"
            << "\"policy\":\"" << policy << "\","
            << "\"maxColumns\":" << columns << ','
            << "\"maxRows\":" << rows << ','
            << "\"columnGapMm\":" << columnGap << ','
            << "\"rowGapMm\":" << rowGap << ','
            << "\"spacingMode\":\"edge_clearance\","
            << "\"order\":\"" << order << "\"}}]}";
    return request.str();
}

const slicer_core::Json& FindInstance(
    const slicer_core::Json& result,
    const std::string& instanceId)
{
    for (const slicer_core::Json& instance :
         result.at("instances").as_array())
    {
        if (instance.at("instanceId").as_string() == instanceId)
        {
            return instance;
        }
    }
    Require(false, "missing instance " + instanceId);
    return result;
}

double BoundsMinimum(
    const slicer_core::Json& instance,
    const std::size_t axis)
{
    return instance.at("effectiveBBoxMm").at("min").at(axis).as_double();
}

int VerifiesTwentyTwoInstanceLayoutAndReplay(
    pm_module_t* module,
    const std::filesystem::path& fixture)
{
    const std::string modelId = ImportModel(module, fixture);
    const slicer_core::Json added = AddTwentyTwoInstances(module, modelId);
    RequireSuccess(added, "22-instance add batch");
    Require(
        added.at("newSceneRevision").as_int() == 1
            && added.at("instances").size() == 22U,
        "add batch must create exactly 22 stable instances");
    const int sceneHandle = added.at("sceneHandle").as_int();
    const std::string request = LayoutRequest(
        sceneHandle,
        "hostflow-layout-commit",
        1,
        11,
        2,
        10.0,
        10.0);
    const slicer_core::Json laidOut = Parse(RunJob(module, request));
    RequireSuccess(laidOut, "22-instance grid layout");
    Require(
        laidOut.at("newSceneRevision").as_int() == 2
            && laidOut.at("instances").size() == 22U
            && laidOut.at("collisions").size() == 0U
            && laidOut.at("outOfBoundsInstances").size() == 0U,
        "layout must commit once without collision or out-of-bounds state");
    Require(
        ApproximatelyEqual(
            BoundsMinimum(FindInstance(laidOut, "instance-1"), 0U),
            0.0)
            && ApproximatelyEqual(
                BoundsMinimum(FindInstance(laidOut, "instance-11"), 0U),
                110.0)
            && ApproximatelyEqual(
                BoundsMinimum(FindInstance(laidOut, "instance-12"), 1U),
                11.0),
        "row-major placements must use 10 mm edge clearances");

    const slicer_core::Json replay = Parse(RunJob(module, request));
    RequireSuccess(replay, "exact layout replay");
    Require(
        replay.at("newSceneRevision").as_int() == 2
            && replay.at("sceneHash").as_string()
                == laidOut.at("sceneHash").as_string(),
        "exact replay must return the committed snapshot without mutation");
    RequireFailure(
        Parse(RunJob(module, LayoutRequest(
            sceneHandle,
            "hostflow-layout-commit",
            1,
            11,
            2,
            9.0,
            10.0))),
        "PM-SLICER-PROFILE-0031",
        "changed layout replay");
    return sceneHandle;
}

void VerifiesFailClosedRequests(pm_module_t* module, const int sceneHandle)
{
    RequireFailure(
        Parse(RunJob(module, LayoutRequest(
            sceneHandle,
            "hostflow-layout-capacity",
            2,
            1,
            1,
            10.0,
            10.0))),
        "PM-SLICER-LAYOUT-0023",
        "insufficient layout capacity");
    RequireFailure(
        Parse(RunJob(module, LayoutRequest(
            sceneHandle,
            "hostflow-layout-invalid-columns",
            2,
            12,
            2,
            10.0,
            10.0))),
        "PM-SLICER-PROFILE-0031",
        "invalid layout columns");
    RequireFailure(
        Parse(RunJob(module, LayoutRequest(
            sceneHandle,
            "hostflow-layout-invalid-gap",
            2,
            11,
            2,
            -0.01,
            10.0))),
        "PM-SLICER-PROFILE-0031",
        "negative layout gap");

    std::string mixed = LayoutRequest(
        sceneHandle,
        "hostflow-layout-mixed",
        2,
        11,
        2,
        10.0,
        10.0);
    mixed.replace(
        mixed.rfind("]}"),
        2U,
        ",{\"type\":\"translate\",\"instanceId\":\"instance-1\","
        "\"deltaMm\":[1,0,0]}]}");
    RequireFailure(
        Parse(RunJob(module, mixed)),
        "PM-SLICER-PROFILE-0031",
        "layout mixed with transform");

    std::string forbidden = LayoutRequest(
        sceneHandle,
        "hostflow-layout-forbidden-instance",
        2,
        11,
        2,
        10.0,
        10.0);
    const std::string marker = "\"type\":\"applyGridLayout\"";
    forbidden.insert(
        forbidden.find(marker) + marker.size(),
        ",\"instanceId\":\"instance-1\"");
    RequireFailure(
        Parse(RunJob(module, forbidden)),
        "PM-SLICER-INPUT-0002",
        "layout instance field");

    const slicer_core::Json snapshot = Parse(RunJob(
        module,
        "{\"capability\":\"scene.get_snapshot\",\"sceneHandle\":"
            + std::to_string(sceneHandle) + "}"));
    RequireSuccess(snapshot, "snapshot after rejected layouts");
    Require(
        snapshot.at("sceneRevision").as_int() == 2,
        "rejected layout requests must not mutate scene revision");
}

}  // namespace

int main(const int argc, char** argv)
{
    Require(argc == 2, "fixture path argument is required");
    pm_module_t* const module = pm_create(nullptr);
    Require(module != nullptr, "pm_create failed");
    const std::filesystem::path fixture =
        std::filesystem::absolute(argv[1]).lexically_normal();
    const int sceneHandle =
        VerifiesTwentyTwoInstanceLayoutAndReplay(module, fixture);
    VerifiesFailClosedRequests(module, sceneHandle);
    pm_destroy(module);
    std::cout << "HOSTFLOW H-A-04 grid layout tests: PASS\n";
    return 0;
}
