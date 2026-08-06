#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/engine/ProductionRepairFacadeFactory.h"
#include "slicer_core/json_value.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

class NeverCancelled final : public slicer_core::api::ICancelToken
{
public:
    bool IsCancelRequested() const noexcept override
    {
        return false;
    }
};

class AlwaysCancelled final : public slicer_core::api::ICancelToken
{
public:
    bool IsCancelRequested() const noexcept override
    {
        return true;
    }
};

std::filesystem::path SourceRoot()
{
    return std::filesystem::path(SLICESOFT_SOURCE_DIR);
}

void WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return slicer_core::Json::parse(input);
}

bool Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

slicer_core::api::RepairRequest MakeRequest(
    const std::filesystem::path& root,
    const std::filesystem::path& source)
{
    slicer_core::api::RepairRequest request;
    request.job_id = "repair-job-001";
    request.correlation_id = "repair-correlation-001";
    request.model_id = "fixture";
    request.source_model_path = source;
    request.repaired_model_path = root / "job" / "repair" / "fixture.repaired.obj";
    request.profile_config_path = SourceRoot()
        / "samples" / "configs" / "material_process"
        / "obj_mtl_texture_rgb_white_ondemand.json";
    request.source_resource_root = root / "source";
    request.job_root_path = root / "job";
    request.profile_hash = slicer_core::api::ComputeProfileDocumentHash(
        ReadJson(request.profile_config_path));
    return request;
}

bool RepairsDuplicateAndReturnsStrictEvidence()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_stage14d08_r3_repair_facade";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root / "source");
    const std::filesystem::path source = root / "source" / "fixture.obj";
    WriteText(
        source,
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\nf 1 3 2\n");
    const std::string sourceBytes = ReadText(source);
    const auto facade = slicer_core::engine::CreateProductionRepairFacade();
    NeverCancelled cancelToken;
    const auto result = facade->Run(MakeRequest(root, source), cancelToken);
    bool pass = Expect(result.IsOk(), "conservative duplicate repair must succeed");
    if (result.IsOk())
    {
        const slicer_core::api::RepairResult& value = *result.Value();
        pass = Expect(std::filesystem::is_regular_file(value.repaired_model_path),
                      "repair must create a job-owned staged OBJ") && pass;
        pass = Expect(value.source_hash.starts_with("sha256:"),
                      "source digest must use sha256 prefix") && pass;
        pass = Expect(value.repaired_hash.starts_with("sha256:"),
                      "output digest must use sha256 prefix") && pass;
        const slicer_core::Json& asset = value.evidence.at("asset");
        pass = Expect(asset.at("assetWritten").as_bool(),
                      "assetWritten evidence must be true") && pass;
        pass = Expect(asset.at("assetReimported").as_bool(),
                      "assetReimported evidence must be true") && pass;
        pass = Expect(asset.at("strictComplete").as_bool(),
                      "strictComplete evidence must be true") && pass;
        pass = Expect(asset.at("strictPass").as_bool(),
                      "strictPass evidence must be true") && pass;
        pass = Expect(asset.at("attributesPreserved").as_bool(),
                      "attributesPreserved evidence must be true") && pass;
    }
    pass = Expect(ReadText(source) == sourceBytes,
                  "repair must not modify the source asset") && pass;
    std::filesystem::remove_all(root, ignored);
    return pass;
}

bool CancellationFailsWithoutOutput()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_stage14d08_r3_repair_cancel";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root / "source");
    const std::filesystem::path source = root / "source" / "fixture.obj";
    WriteText(
        source,
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\n");
    const auto facade = slicer_core::engine::CreateProductionRepairFacade();
    AlwaysCancelled cancelToken;
    const slicer_core::api::RepairRequest request = MakeRequest(root, source);
    const auto result = facade->Run(request, cancelToken);
    const bool pass = Expect(!result.IsOk(), "cancelled repair must fail")
        && Expect(result.Error() != nullptr
                      && result.Error()->code == "PM-SLICER-CANCELLED-0070",
                  "cancelled repair must use stable error code")
        && Expect(!std::filesystem::exists(request.repaired_model_path),
                  "cancelled repair must not publish output");
    std::filesystem::remove_all(root, ignored);
    return pass;
}

bool RejectsOutputScopeEscape()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path()
        / "slicesoft_stage14d08_r3_repair_scope";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root / "source");
    const std::filesystem::path source = root / "source" / "fixture.obj";
    WriteText(
        source,
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "f 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\n");
    slicer_core::api::RepairRequest request = MakeRequest(root, source);
    request.repaired_model_path = root / "escaped.obj";
    const auto facade = slicer_core::engine::CreateProductionRepairFacade();
    NeverCancelled cancelToken;
    const auto result = facade->Run(request, cancelToken);
    const bool pass = Expect(!result.IsOk(), "output scope escape must fail")
        && Expect(result.Error() != nullptr
                      && result.Error()->code == "PM-SLICER-INPUT-0001",
                  "scope escape must use stable input error");
    std::filesystem::remove_all(root, ignored);
    return pass;
}

}  // namespace

int main()
{
    const bool pass = RepairsDuplicateAndReturnsStrictEvidence()
        && CancellationFailsWithoutOutput()
        && RejectsOutputScopeEscape();
    if (!pass)
    {
        return 1;
    }
    std::cout << "stage14d08_r3_repair_facade_tests: PASS\n";
    return 0;
}
