#include "contracts/print_module_spi.h"
#include "slicer_core/json_value.h"
#include "slicer_core/api/ProfileIdentity.h"
#include "slicer_core/system/Utf8Path.h"
#include "slicer_core/system/Utf8CommandLine.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/model/AssetReferencePath.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
using slicer_core::Json;
void Require(bool ok, const std::string& message)
{
    if (!ok) throw std::runtime_error(message);
}
std::string Utf8(const std::filesystem::path& path)
{
    const auto bytes = path.generic_u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
Json Run(pm_module_t* module, const Json& request)
{
    std::unique_ptr<pm_job_t, decltype(&pm_release)> job(
        pm_submit(module, request.dump(0).c_str()), pm_release);
    if (!job)
    {
        char error[4096]{};
        (void)pm_last_error(error, sizeof(error), nullptr);
        throw std::runtime_error(std::string("submit failed: ") + error);
    }
    int required = 0;
    int status = PM_ERR_INVALID_STATE;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(2);
    while ((status = pm_result(job.get(), nullptr, 0, &required)) == PM_ERR_INVALID_STATE
        && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(status == PM_ERR_BUFFER_SMALL, "result unavailable or timed out");
    std::vector<char> bytes(static_cast<std::size_t>(required) + 1);
    Require(pm_result(job.get(), bytes.data(), static_cast<int>(bytes.size()), nullptr) == required,
        "result read failed");
    std::istringstream stream{bytes.data()};
    auto result = Json::parse(stream);
    Require(result.at("ok").as_bool(), result.dump(0));
    return result;
}
Json Import(pm_module_t* module, const std::filesystem::path& path)
{
    return Run(module, Json::object({{"capability", "model.import"}, {"modelPath", Utf8(path)},
        {"options", Json::object({{"computeBBox", true}, {"extractMaterials", true},
            {"autoOrient", false}})}}));
}
std::string Read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "fixture read failed");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void Write(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary);
    output << text;
    Require(output.good(), "fixture write failed");
}
void CheckPathBoundaries()
{
    bool rejected = false;
    try { (void)slicer_core::PathFromUtf8(std::string("bad\0path", 8)); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "embedded NUL must be rejected");
#ifdef _WIN32
    rejected = false;
    try { (void)slicer_core::PathFromUtf8("\xb2\xc4\xd6\xca.mtl"); }
    catch (const std::system_error&) { rejected = true; }
    Require(rejected, "SPI paths must not fall back to ANSI");
    if (GetACP() == 936)
        Require(slicer_core::model_detail::AssetReferencePath("\xb2\xc4\xd6\xca.mtl")
            == std::filesystem::path(u8"\u6750\u8d28.mtl"), "legacy ANSI reference compatibility failed");
#endif
}
Json MakeScene(pm_module_t* module, const Json& imported, const std::filesystem::path& model)
{
    const auto added = Run(module, Json::object({{"capability", "scene.apply_operation"},
        {"operationId", "unicode-add"}, {"currentSceneRevision", 0}, {"expectedSceneRevision", 0},
        {"sceneContext", Json::object({{"resolvedProfileId", "golden_material_process_top2_fixture"},
            {"buildVolume", Json::object({{"source", "device_profile"}, {"widthMm", 230},
                {"heightMm", 100}, {"zLimitMm", 60}, {"origin", "lower_left"},
                {"xDirection", "positive"}, {"yDirection", "positive"}, {"isFixture", false}})}})},
        {"operations", Json::array({Json::object({{"type", "addInstance"},
            {"modelId", imported.at("modelId")}})})}}));
    const auto snapshot = Run(module, Json::object({{"capability", "scene.get_snapshot"},
        {"sceneHandle", added.at("sceneHandle")}}));
    const auto& source = snapshot.at("scene").at("models").at(0U);
    Require(source.at("sourcePath").as_string() == Utf8(model), "scene source path is not UTF-8");
    Require(source.at("displayName").as_string() == Utf8(model.stem()), "display name is not UTF-8");
    const auto decoded = slicer_core::DeserializeMultiModelScene(snapshot.at("scene"));
    Require(decoded.IsValid() && decoded.scene.models.front().sourcepath == model,
        "scene native path roundtrip failed");
    Require(slicer_core::SerializeMultiModelScene(decoded.scene).dump(0) == snapshot.at("scene").dump(0),
        "scene serialization identity drift");
    return snapshot;
}
void Slice(pm_module_t* module, const Json& snapshot, const std::filesystem::path& repository,
    const std::filesystem::path& model, const std::filesystem::path& output)
{
    std::filesystem::create_directories(output.parent_path());
    std::istringstream input{Read(repository / "samples/configs/golden/material_process_top2_fixture.json")};
    auto profile = Json::parse(input).as_object();
    profile["input"] = Json::object({{"modelPath", Utf8(model)}, {"format", "auto"}});
    auto settings = profile.at("output").as_object();
    settings["packageDir"] = Utf8(output);
    profile["output"] = Json{std::move(settings)};
    auto texture = profile.at("texture").as_object();
    texture["missingTexturePolicy"] = "fail_fast";
    profile["texture"] = Json{std::move(texture)};
    profile["autoOrient"] = Json::object({{"enabled", false}});
    profile["profileVersion"] = "unipath-1";
    profile["profileHash"] = slicer_core::api::ComputeProfileDocumentHash(Json{profile});
    auto hash = snapshot.at("sceneHash").as_string();
    if (!hash.starts_with("sha256:")) hash = "sha256:" + hash;
    const auto sliced = Run(module, Json::object({{"capability", "slice.rgbwsv"},
        {"jobId", "unicode-slice"}, {"correlationId", "unicode-correlation"},
        {"scene", snapshot.at("scene")}, {"sceneHash", hash}, {"profile", Json{std::move(profile)}},
        {"output", Json::object({{"contract", "p0.rgbwsv.2"}, {"packageDir", Utf8(output)}})},
        {"options", Json::object({{"backend", "worker"}})}}));
    Require(sliced.at("packageDir").as_string() == Utf8(output), "Worker result path drift");
    const auto verified = Run(module, Json::object({{"capability", "package.verify"},
        {"packageDir", Utf8(output)}}));
    Require(verified.at("valid").as_bool(), "produced package is invalid");
    std::vector<std::filesystem::path> layers;
    for (const auto& entry : std::filesystem::directory_iterator(output / "layers"))
        if (entry.is_regular_file()) layers.push_back(entry.path());
    std::sort(layers.begin(), layers.end());
    Require(layers.size() == static_cast<std::size_t>(verified.at("layerCount").as_int()),
        "TIFF count mismatch");
    std::string layerDigests;
    for (const auto& layer : layers) layerDigests += slicer_core::ComputeSha256(Read(layer));
    std::cout << "TIFF_CHECKSUMS=" << slicer_core::ComputeSha256(layerDigests) << '\n';
    const auto report = Run(module, Json::object({{"capability", "package.read_report"},
        {"packageDir", Utf8(output)}, {"reportName", "scene"}}));
    Require(report.dump(0).find(Utf8(model)) != std::string::npos, "scene report lost UTF-8 model path");
    const auto previewPath = output.parent_path() / std::filesystem::path(u8"\u9884\u89c8.png");
    const auto preview = Run(module, Json::object({{"capability", "package.render_layer_preview"},
        {"packageDir", Utf8(output)}, {"layerIndex", 0}, {"mode", "single_channel"},
        {"channels", Json::array({"R"})}, {"maxWidthPx", 256}, {"outputPath", Utf8(previewPath)}}));
    Require(preview.at("outputPath").as_string() == Utf8(previewPath)
        && std::filesystem::file_size(previewPath) > 0, "Chinese preview path failed");
    std::cout << "UNICODE_WORKER_SLICE_PASS layers=" << verified.at("layerCount").as_int() << '\n';
}
}

static int RunTests(int argc, char** argv)
{
    try
    {
        Require(argc == 2 || argc == 3, "repository root and optional UTF-8 model path required");
        CheckPathBoundaries();
#ifdef _WIN32
        std::cout << "ACTIVE_CODE_PAGE=" << GetACP() << std::endl;
#endif
        const auto root = std::filesystem::temp_directory_path() / ("slicesoft_unicode_"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto modelDirectory = root / std::filesystem::path(u8"\u4e2d\u6587 \u76ee\u5f55");
        std::filesystem::create_directories(modelDirectory);
        std::filesystem::current_path(root); // Keep 3MF extraction caches inside test evidence.
        const auto repository = slicer_core::PathFromUtf8(argv[1]);
        const auto fixture = repository / "samples/models/textured/fixtures/policy_textured_small.obj";
        const auto modelPath = modelDirectory / std::filesystem::path(u8"\u9ec4\u6668\u6668ND002.obj");
        const auto materialPath = modelDirectory / std::filesystem::path(u8"\u6750\u8d28.mtl");
        const auto texturePath = modelDirectory / std::filesystem::path(u8"\u8d34\u56fe \u989c\u8272.png");
        auto modelText = Read(fixture);
        modelText.replace(0, modelText.find('\n'), "mtllib " + Utf8(materialPath.filename()));
        Write(modelPath, modelText);
        Write(materialPath, "newmtl policy_tex\nKd 1 1 1\nmap_Kd " + Utf8(texturePath.filename()) + "\n");
        std::filesystem::copy_file(repository / "samples/models/textured/textures/gradient.png", texturePath);
        std::unique_ptr<pm_module_t, decltype(&pm_destroy)> module(pm_create(nullptr), pm_destroy);
        Require(module != nullptr, "module create failed");
        const auto imported = Import(module.get(), modelPath);
        Require(imported.at("triangleCount").as_int() > 0, "empty import");
        Require(!imported.at("singleMaterialOnly").as_bool(), "Chinese MTL/texture was silently degraded");
        Require(imported.at("materials").at(0U).at("texturePath").as_string() == Utf8(texturePath),
            "material texture path is not UTF-8");
        const auto snapshot = MakeScene(module.get(), imported, modelPath);
        Slice(module.get(), snapshot, repository, modelPath,
            root / std::filesystem::path(u8"\u5207\u7247 \u8f93\u51fa")
                / std::filesystem::path(u8"\u6210\u54c1\u5305"));
        for (const auto& source : {"samples/models/sample.stl", "samples/models/3mf/texture2d_checker_cube.3mf"})
        {
            const auto target = modelDirectory / (std::filesystem::path(u8"\u6a21\u578b") += std::filesystem::path(source).extension());
            std::filesystem::copy_file(repository / source, target);
            Require(Import(module.get(), target).at("triangleCount").as_int() > 0, "STL/3MF import failed");
        }
        if (argc == 3)
        {
            const auto real = Import(module.get(), slicer_core::PathFromUtf8(argv[2]));
            std::cout << "REAL_MODEL_IMPORT_PASS triangles=" << real.at("triangleCount").as_int() << '\n';
        }
        std::cout << "UNICODE_PATH_IMPORT_PASS root=" << Utf8(root) << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) { return slicer_core::RunUtf8Main(argc, argv, RunTests); }
#else
int main(int argc, char** argv) { return RunTests(argc, argv); }
#endif
