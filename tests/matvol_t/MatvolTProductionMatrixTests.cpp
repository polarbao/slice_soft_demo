#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/materials/transfer/TransferChannelError.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtPackageReader.h"
#include "slicer_core/rip_reader.h"
#include "slicer_core/slicer.h"
#include "slicer_core/model.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

namespace
{

bool Expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL " << message << '\n';
    }
    return condition;
}

std::filesystem::path SourceRoot()
{
#ifdef SLICESOFT_SOURCE_DIR
    return std::filesystem::path{SLICESOFT_SOURCE_DIR};
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path ModelPath(const char* fileName)
{
    return SourceRoot() / "model/obj/reality/finger_suoguo" / fileName;
}

slicer_core::Json MakeConfig(
    const std::filesystem::path& package,
    const char* modelName,
    const std::array<int, 3>& colour)
{
    return slicer_core::Json::object({
        {"input", slicer_core::Json::object({
            {"format", "obj"},
            {"modelPath", ModelPath(modelName).generic_string()}})},
        {"output", slicer_core::Json::object({
            {"packageDir", package.generic_string()},
            {"packageProtocol", "p0.rgbwsvt.1"},
            {"channelOrder", slicer_core::Json::array(
                {"R", "G", "B", "W", "S", "V", "T"})},
            {"dpiX", 80},
            {"dpiY", 80},
            {"layerThicknessMm", 0.30},
            {"storageMode", "stripped"},
            {"rowsPerStrip", 16}})},
        {"preview", slicer_core::Json::object({{"enabled", false}})},
        {"transferChannelPolicy", slicer_core::Json::object({
            {"enabled", true},
            {"matchSource", "material_diffuse_rgb"},
            {"materialDiffuseRgbValues", slicer_core::Json::array({
                slicer_core::Json::array(
                    {colour[0], colour[1], colour[2]})})},
            {"missingRegion", "allow_empty"},
            {"multipleMatches", "fail_closed"},
            {"value", 0},
            {"topology", slicer_core::Json::object({
                {"selfIntersectionPolicy", "tolerate_closed_self_intersection"},
                {"maxSelfIntersectionPairs", 64}})}})}});
}

std::filesystem::path WriteConfig(
    const std::filesystem::path& root,
    const std::string& name,
    const slicer_core::Json& config)
{
    const std::filesystem::path path = root / (name + ".json");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << config.dump(2) << '\n';
    if (!output.good())
    {
        throw std::runtime_error("failed to write matrix config");
    }
    return path;
}

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return slicer_core::Json::parse(input);
}

slicer_core::SliceRunOptions WriteOptions()
{
    slicer_core::SliceRunOptions options;
    options.write_preview_files = false;
    return options;
}

bool Reality03PassesStrictPackageReader(const std::filesystem::path& root)
{
    const std::filesystem::path package = root / "reality_03";
    (void)slicer_core::run_slicer(
        WriteConfig(
            root,
            "reality_03",
            MakeConfig(package, "03.obj", {255, 220, 198})),
        WriteOptions());
    const slicer_core::RgbwsvtPackageValidation validated =
        slicer_core::ValidateRgbwsvtPackage(package);
    return Expect(validated.schema == "p0.rgbwsvt.1", "03 strict schema")
        && Expect(
            validated.productionAcceptance
                == "rgbwsvt_candidate_unvalidated",
            "direct 03 run remains candidate")
        && Expect(validated.channelOrder.size() == 7U, "03 strict channel count")
        && Expect(
            validated.totalChannelStats[6U].print_pixels > 0U,
            "03 strict T pixels");
}

bool SceneProductionOptInWritesAdmitted(const std::filesystem::path& root)
{
    const std::filesystem::path package = root / "scene_opt_in";
    const std::filesystem::path configPath = WriteConfig(
        root,
        "scene_opt_in",
        MakeConfig(package, "03.obj", {255, 220, 198}));
    const slicer_core::SliceConfig config =
        slicer_core::load_slice_config(configPath);
    const slicer_core::ModelReport model =
        slicer_core::load_model_report(config, configPath.parent_path());
    slicer_core::ModelInstance instance;
    instance.instanceid = "matvol-t-scene-instance";
    instance.modelid = "matvol-t-scene-model";
    instance.sourcetransformidentity = ModelPath("03.obj").generic_string();
    instance.sourcebboxmm = model.bbox_mm;
    instance.effectivebboxmm = model.bbox_mm;

    slicer_core::SliceRunOptions options = WriteOptions();
    options.inputoverride = slicer_core::SliceRunInputOverride{
        ModelPath("03.obj"), "obj"};
    options.instanceoverride = instance;
    options.transfer_scene_production_opt_in = true;
    (void)slicer_core::run_slicer(configPath, options);

    const slicer_core::RgbwsvtPackageValidation validated =
        slicer_core::ValidateRgbwsvtPackage(package);
    return Expect(
        validated.productionAcceptance == "admitted",
        "qualified Scene RGBWSVT run is admitted");
}

bool UnqualifiedOptInRemainsCandidate(const std::filesystem::path& root)
{
    const std::filesystem::path package = root / "unqualified_opt_in";
    slicer_core::SliceRunOptions options = WriteOptions();
    options.transfer_scene_production_opt_in = true;
    (void)slicer_core::run_slicer(
        WriteConfig(
            root,
            "unqualified_opt_in",
            MakeConfig(package, "03.obj", {255, 220, 198})),
        options);
    return Expect(
        slicer_core::ValidateRgbwsvtPackage(package).productionAcceptance
            == "rgbwsvt_candidate_unvalidated",
        "unqualified Runner opt-in cannot admit a direct package");
}

bool InvalidAcceptanceFailsStrictReader(const std::filesystem::path& root)
{
    const std::filesystem::path package = root / "invalid_acceptance";
    (void)slicer_core::run_slicer(
        WriteConfig(
            root,
            "invalid_acceptance",
            MakeConfig(package, "03.obj", {255, 220, 198})),
        WriteOptions());
    slicer_core::Json::Object manifest =
        ReadJson(package / "manifest.json").as_object();
    manifest["productionAcceptance"] = "unknown";
    std::ofstream output(
        package / "manifest.json", std::ios::binary | std::ios::trunc);
    output << slicer_core::Json{std::move(manifest)}.dump(2) << '\n';
    output.close();
    try
    {
        (void)slicer_core::ValidateRgbwsvtPackage(package);
    }
    catch (const slicer_core::ValidationError& error)
    {
        return Expect(
            error.code() == slicer_core::ValidationErrorCode::SchemaUnsupported,
            "unknown production acceptance fails with stable code");
    }
    return Expect(false, "unknown production acceptance must fail strict validation");
}

bool OpenRealityFailsWithoutPackage(
    const std::filesystem::path& root,
    const char* modelName)
{
    const std::filesystem::path package = root / (std::string{"open_"} + modelName);
    try
    {
        (void)slicer_core::run_slicer(
            WriteConfig(
                root,
                std::string{"open_"} + modelName,
                MakeConfig(package, modelName, {255, 255, 0})),
            WriteOptions());
    }
    catch (const slicer_core::TransferChannelError& error)
    {
        return Expect(
                   error.Code()
                       == slicer_core::TransferChannelErrorCode::TopologyInvalid,
                   std::string{modelName} + " topology error")
            && Expect(
                !std::filesystem::exists(package),
                std::string{modelName} + " writes no package");
    }
    return Expect(false, std::string{modelName} + " must fail closed");
}

bool TamperedStatisticsFailStrictReader(const std::filesystem::path& root)
{
    const std::filesystem::path package = root / "tampered";
    (void)slicer_core::run_slicer(
        WriteConfig(
            root,
            "tampered",
            MakeConfig(package, "03.obj", {255, 220, 198})),
        WriteOptions());
    slicer_core::Json::Object manifest = ReadJson(package / "manifest.json").as_object();
    slicer_core::Json::Object tiff = manifest.at("tiff").as_object();
    slicer_core::Json::Object stats = tiff.at("channelStats").as_object();
    slicer_core::Json::Object transfer = stats.at("T").as_object();
    transfer["printPixels"] = transfer.at("printPixels").as_int() + 1;
    stats["T"] = slicer_core::Json{std::move(transfer)};
    tiff["channelStats"] = slicer_core::Json{std::move(stats)};
    manifest["tiff"] = slicer_core::Json{std::move(tiff)};
    std::ofstream output(package / "manifest.json", std::ios::binary | std::ios::trunc);
    output << slicer_core::Json{std::move(manifest)}.dump(2) << '\n';
    output.close();
    try
    {
        (void)slicer_core::ValidateRgbwsvtPackage(package);
    }
    catch (const slicer_core::ValidationError& error)
    {
        return Expect(
            error.code() == slicer_core::ValidationErrorCode::LayerStatisticsMismatch,
            "tampered T statistics fail with stable code");
    }
    return Expect(false, "tampered T statistics must fail strict validation");
}

bool CancellationRemovesReservedPackage(const std::filesystem::path& root)
{
    const std::filesystem::path package = root / "cancelled";
    bool packageReady{false};
    slicer_core::SliceRunOptions options = WriteOptions();
    options.progress_callback = [&packageReady](
        const slicer_core::SliceRunProgress& progress)
    {
        if (progress.phase == "support_generation")
        {
            packageReady = true;
        }
    };
    options.cancellation_requested = [&packageReady]() { return packageReady; };
    try
    {
        (void)slicer_core::run_slicer(
            WriteConfig(
                root,
                "cancelled",
                MakeConfig(package, "03.obj", {255, 220, 198})),
            options);
    }
    catch (const std::exception&)
    {
        return Expect(packageReady, "cancellation happens after package reservation")
            && Expect(
                !std::filesystem::exists(package),
                "cancelled RGBWSVT run removes the reserved package");
    }
    return Expect(false, "cancelled RGBWSVT run must throw");
}

}  // namespace

int main()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("slicesoft_matvol_t_matrix_" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    int failures{0};
    try
    {
        failures += Reality03PassesStrictPackageReader(root) ? 0 : 1;
        failures += SceneProductionOptInWritesAdmitted(root) ? 0 : 1;
        failures += UnqualifiedOptInRemainsCandidate(root) ? 0 : 1;
        failures += InvalidAcceptanceFailsStrictReader(root) ? 0 : 1;
        failures += OpenRealityFailsWithoutPackage(root, "08.obj") ? 0 : 1;
        failures += OpenRealityFailsWithoutPackage(root, "09.obj") ? 0 : 1;
        failures += TamperedStatisticsFailStrictReader(root) ? 0 : 1;
        failures += CancellationRemovesReservedPackage(root) ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL MatvolTProductionMatrixTests exception: "
                  << error.what() << '\n';
        ++failures;
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    if (failures != 0)
    {
        std::cerr << "FAIL MatvolTProductionMatrixTests " << failures
                  << " case(s)\n";
        return 1;
    }
    std::cout << "PASS MatvolTProductionMatrixTests 8/8\n";
    return 0;
}
