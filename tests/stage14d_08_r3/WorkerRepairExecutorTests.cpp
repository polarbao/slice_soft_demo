#include "slicer_worker/repair/WorkerRepairExecutor.h"

#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/engine/ProductionRepairFacadeFactory.h"
#include "slicer_core/json_value.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace
{

namespace api = slicer_core::api;
namespace worker = slicesoft::worker;

int g_failures{0};

class TestCancelToken final : public api::ICancelToken
{
public:
    explicit TestCancelToken(const bool cancelled = false)
        : m_cancelled(cancelled)
    {
    }

    /** @brief 返回确定性的取消状态。 */
    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return m_cancelled;
    }

private:
    bool m_cancelled{false};
};

void Check(const bool condition, const std::string& message)
{
    if (!condition)
    {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return slicer_core::Json::parse(input);
}

std::filesystem::path SourceRoot()
{
    return std::filesystem::path(SLICESOFT_SOURCE_DIR);
}

worker::WorkerRequestEnvelope MakeRequest(
    const std::filesystem::path& jobRoot,
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& outputPath,
    slicer_core::Json inputOverride = nullptr)
{
    const std::filesystem::path profilePath = SourceRoot()
        / "samples/configs/material_process/obj_mtl_texture_rgb_white_ondemand.json";
    const slicer_core::Json profile = ReadJson(profilePath);
    slicer_core::Json::Object input = slicer_core::Json::object({
        {"modelId", "repair-fixture"},
        {"modelPath", sourcePath.generic_string()},
        {"modelFormat", "obj"},
        {"outputPath", outputPath.generic_string()},
        {"profileHash", api::ComputeProfileDocumentHash(profile)},
        {"sourceResourceScope", slicer_core::Json::object({
            {"rootPath", sourcePath.parent_path().generic_string()},
        })},
        {"repairOutputFormat", "obj"},
        {"policy", "conservative"},
        {"requireStrictPass", true},
    }).as_object();
    if (inputOverride.is_object())
    {
        for (const auto& [key, value] : inputOverride.as_object())
        {
            input.insert_or_assign(key, value);
        }
    }
    return worker::WorkerRequestEnvelope(
        worker::WorkerJobIdentity(
            jobRoot.filename().generic_string(),
            "repair-correlation",
            "geometry.repair",
            jobRoot / "request.json"),
        1U,
        0U,
        std::chrono::milliseconds(30000),
        std::nullopt,
        nullptr,
        profile,
        slicer_core::Json{std::move(input)},
        nullptr);
}

void TestProductionRepairAndPublication(const std::filesystem::path& root)
{
    const std::filesystem::path jobRoot = root / "job-success";
    const std::filesystem::path source = root / "source" / "fixture.obj";
    const std::filesystem::path output =
        jobRoot / "repair" / "fixture.repaired.obj";
    WriteText(
        source,
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\nf 1 3 2\n");
    std::filesystem::create_directories(jobRoot);
    worker::WorkerRepairExecutor executor(
        slicer_core::engine::CreateProductionRepairFacade());
    const auto result = executor.Execute(
        MakeRequest(jobRoot, source, output), TestCancelToken{});
    Check(result.Ok(), "production Worker repair succeeds");
    if (result.Ok())
    {
        Check(result.Output().at("outputPath").as_string()
                == output.generic_string(),
            "Worker returns the requested job-owned output path");
        const slicer_core::Json& evidence = result.Output().at("evidence");
        Check(evidence.at("assetWritten").as_bool()
                && evidence.at("assetReimported").as_bool()
                && evidence.at("strictComplete").as_bool()
                && evidence.at("strictPass").as_bool()
                && evidence.at("attributesPreserved").as_bool(),
            "Worker flattens all required strict asset evidence");
    }
    Check(std::filesystem::is_regular_file(output),
        "Worker publishes the repaired OBJ inside the job repair directory");
    Check(std::filesystem::is_regular_file(
            std::filesystem::path(output.generic_string() + ".evidence.json")),
        "Worker publishes the repair evidence beside the OBJ");
    Check(!std::filesystem::exists(
            output.parent_path() / ".job-success.repair.staging"),
        "Worker removes the exact repair staging directory");
}

void TestCancellationAndScopeFailure(const std::filesystem::path& root)
{
    const std::filesystem::path source = root / "source-cancel" / "fixture.obj";
    WriteText(
        source,
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\n");
    const std::filesystem::path cancelRoot = root / "job-cancel";
    std::filesystem::create_directories(cancelRoot);
    const std::filesystem::path cancelOutput =
        cancelRoot / "repair" / "fixture.repaired.obj";
    worker::WorkerRepairExecutor executor(
        slicer_core::engine::CreateProductionRepairFacade());
    const auto cancelled = executor.Execute(
        MakeRequest(cancelRoot, source, cancelOutput), TestCancelToken{true});
    Check(!cancelled.Ok() && cancelled.Code() == "PM-SLICER-CANCELLED-0070",
        "cancelled repair uses the stable cancellation code");
    Check(cancelled.Cleanup().has_value()
            && cancelled.Cleanup()->StagingRemoved()
            && !cancelled.Cleanup()->Published(),
        "cancelled repair reports the frozen cleanup evidence");
    Check(!std::filesystem::exists(cancelOutput),
        "cancelled repair publishes no output");

    const std::filesystem::path escapeRoot = root / "job-escape";
    std::filesystem::create_directories(escapeRoot);
    const auto escaped = executor.Execute(
        MakeRequest(escapeRoot, source, root / "escaped.obj"),
        TestCancelToken{});
    Check(!escaped.Ok() && escaped.Code() == "PM-SLICER-INPUT-0001",
        "repair output outside job/repair fails closed");
}

}  // namespace

int main()
{
    const auto suffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("slicesoft_stage14d08_worker_repair_" + std::to_string(suffix));
    std::filesystem::create_directories(root);
    try
    {
        TestProductionRepairAndPublication(root);
        TestCancellationAndScopeFailure(root);
    }
    catch (const std::exception& error)
    {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    if (g_failures != 0)
    {
        std::cerr << g_failures << " Worker repair test(s) failed\n";
        return 1;
    }
    std::cout << "stage14d08 R3 Worker repair tests passed\n";
    return 0;
}
