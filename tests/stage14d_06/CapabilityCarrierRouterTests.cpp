#include "slicer_module/CapabilityCarrierRouter.h"

#include <iostream>
#include <string>

namespace
{

int g_failures{0};

void Check(const bool condition, const std::string& message)
{
    if (!condition)
    {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void TestLightAndModeDependentRouting()
{
    const auto light = slicesoft::module::CapabilityCarrierRouter::Route(
        R"({"capability":"model.import"})");
    Check(light.accepted, "light capability reaches the synchronous adapter");
    Check(light.carrier == slicesoft::module::CapabilityCarrier::InProcess,
        "light capability stays in-process");

    const auto fast = slicesoft::module::CapabilityCarrierRouter::Route(
        R"({"capability":"geometry.preflight","mode":"fast"})");
    Check(fast.accepted, "fast preflight is accepted");
    Check(fast.carrier == slicesoft::module::CapabilityCarrier::InProcess,
        "fast preflight stays in-process");

    const auto full = slicesoft::module::CapabilityCarrierRouter::Route(
        R"({"capability":"geometry.preflight","mode":"full","scene":{},"sceneHash":"sha256:12345678","expectedSceneRevision":1,"profile":{},"profileHash":"sha256:12345678","targetMode":"legacy"})");
    Check(full.accepted, "full preflight is accepted by the Worker route");
    Check(full.carrier == slicesoft::module::CapabilityCarrier::Worker,
        "full preflight uses the Worker");
    Check(full.workerCapability == "geometry.preflight.full",
        "full preflight maps to the private Worker capability");
    Check(full.workerPayload.at("input").at("mode").as_string() == "full",
        "full preflight fields move into the Worker input branch");
}

void TestWorkerOnlyBackend()
{
    const std::string prefix =
        R"({"capability":"slice.rgbwsv","jobId":"job-1","correlationId":"correlation-1","sceneHash":"sha256:12345678","scene":{},"profile":{},"output":{"contract":"p0.rgbwsv.2","packageDir":"C:/package"})";
    const auto implicit = slicesoft::module::CapabilityCarrierRouter::Route(
        prefix + "}");
    Check(implicit.accepted, "missing backend defaults to Worker");
    Check(implicit.workerPayload.at("options").at("backend").as_string()
            == "worker",
        "implicit backend is normalized to worker");

    const auto explicitWorker =
        slicesoft::module::CapabilityCarrierRouter::Route(
            prefix + R"(,"options":{"backend":"worker"}})");
    Check(explicitWorker.accepted, "explicit Worker backend is accepted");

    for (const std::string backend : {"inprocess", "auto", "legacy", "Worker"})
    {
        const auto rejected = slicesoft::module::CapabilityCarrierRouter::Route(
            prefix + R"(,"options":{"backend":")" + backend + R"("}})");
        Check(!rejected.accepted, "non-Worker backend is rejected: " + backend);
        Check(rejected.errorCode == "PM-SLICER-PROFILE-0031",
            "non-Worker backend has the frozen profile error");
    }

    const auto wrongType = slicesoft::module::CapabilityCarrierRouter::Route(
        prefix + R"(,"options":{"backend":1}})");
    Check(!wrongType.accepted, "non-string backend is rejected");
    Check(wrongType.errorCode == "PM-SLICER-INPUT-0002",
        "non-string backend has the frozen input error");
}

void TestRepairMappingAndInvalidInput()
{
    const auto repair = slicesoft::module::CapabilityCarrierRouter::Route(
        R"({"capability":"geometry.repair","jobId":"repair-1","correlationId":"correlation-1","modelPath":"C:/model.obj","modelFormat":"obj","outputPath":"C:/fixed.obj","profile":{},"profileHash":"sha256:12345678","sourceResourceScope":{},"repairOutputFormat":"obj","policy":"conservative","requireStrictPass":true})");
    Check(repair.accepted, "repair request is accepted");
    Check(repair.carrier == slicesoft::module::CapabilityCarrier::Worker,
        "repair is Worker-only");
    Check(repair.workerPayload.at("input").at("modelPath").as_string()
            == "C:/model.obj",
        "repair fields move into the Worker input branch");

    const auto invalid = slicesoft::module::CapabilityCarrierRouter::Route("{}");
    Check(!invalid.accepted, "missing capability is rejected");
    Check(invalid.errorCode == "PM-SLICER-INPUT-0002",
        "invalid routing input has a stable error");
}

}  // namespace

int main()
{
    TestLightAndModeDependentRouting();
    TestWorkerOnlyBackend();
    TestRepairMappingAndInvalidInput();
    if (g_failures != 0)
    {
        std::cerr << g_failures << " Stage 14D-06 router checks failed\n";
        return 1;
    }
    std::cout << "Stage 14D-06 capability carrier router: PASS\n";
    return 0;
}
