#include "slicer_core/json_value.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"
#include "slicer_core/slicer.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

namespace
{

bool ExpectTrue(const bool condition, const std::string& message)
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

slicer_core::Json ReadJson(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return slicer_core::Json::parse(input);
}

std::filesystem::path WriteProfileVariant(
    const std::filesystem::path& root,
    const std::string& profileName,
    const std::string& suffix,
    const std::filesystem::path& package)
{
    const std::filesystem::path source = SourceRoot()
        / "samples/configs/matvol_t/process_profiles" / (profileName + ".json");
    slicer_core::Json::Object config = ReadJson(source).as_object();
    slicer_core::Json::Object input = config.at("input").as_object();
    input["modelPath"] = (SourceRoot()
        / "model/obj/reality/finger_suoguo/03.obj").generic_string();
    config["input"] = slicer_core::Json{std::move(input)};
    slicer_core::Json::Object output = config.at("output").as_object();
    output["packageDir"] = package.generic_string();
    output["dpiX"] = 80;
    output["dpiY"] = 80;
    output["layerThicknessMm"] = 0.30;
    config["output"] = slicer_core::Json{std::move(output)};
    slicer_core::Json::Object preview = config.at("preview").as_object();
    preview["enabled"] = false;
    preview["outputPolicy"] = "tiff_native";
    config["preview"] = slicer_core::Json{std::move(preview)};
    const std::filesystem::path path = root / (profileName + "_" + suffix + ".json");
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << slicer_core::Json{std::move(config)}.dump(2) << '\n';
    return path;
}

bool ProfileProducesCandidate(
    const std::filesystem::path& root,
    const std::string& profileName,
    const std::string& expectedMaterial,
    const std::string& suffix)
{
    const std::filesystem::path package = root / (profileName + "_" + suffix);
    const std::filesystem::path config = WriteProfileVariant(
        root, profileName, suffix, package);
    (void)slicer_core::run_slicer(config);
    const slicer_core::Json manifest = ReadJson(package / "manifest.json");
    const slicer_core::Json process = ReadJson(
        package / "reports/material_process_report.json");
    const slicer_core::Json transfer = ReadJson(
        package / "reports/transfer_channel_report.json");
    return ExpectTrue(
               manifest.at("schema").as_string() == "p0.rgbwsvt.1",
               profileName + " writes RGBWSVT")
        && ExpectTrue(
            manifest.at("productionAcceptance").as_string()
                == "rgbwsvt_candidate_unvalidated",
            profileName + " remains candidate-only")
        && ExpectTrue(
            process.at(expectedMaterial).at("printPixels").as_int() > 0,
            profileName + " writes its configured nail material")
        && ExpectTrue(
            transfer.at("totals").at("transferPrintPixels").as_int() > 0,
            profileName + " writes T")
        && ExpectTrue(
            transfer.at("totals").at("unexpectedOverlapPixels").as_int() == 0,
            profileName + " keeps T exclusive");
}

bool RgbProfileIsByteRepeatable(const std::filesystem::path& root)
{
    const std::string profile = "obj_mtl_texture_rgb_only_rgbwsvt";
    const std::filesystem::path first = root / "repeat_first";
    const std::filesystem::path second = root / "repeat_second";
    (void)slicer_core::run_slicer(WriteProfileVariant(root, profile, "first", first));
    (void)slicer_core::run_slicer(WriteProfileVariant(root, profile, "second", second));
    const slicer_core::Json firstManifest = ReadJson(first / "manifest.json");
    const slicer_core::Json secondManifest = ReadJson(second / "manifest.json");
    bool equal = firstManifest.at("layers").size() == secondManifest.at("layers").size();
    for (std::size_t index = 0U; equal && index < firstManifest.at("layers").size(); ++index)
    {
        const auto firstLayer = slicer_core::ReadRgbwsvtTiff(
            first / firstManifest.at("layers").at(index).at("path").as_string());
        const auto secondLayer = slicer_core::ReadRgbwsvtTiff(
            second / secondManifest.at("layers").at(index).at("path").as_string());
        equal = firstLayer.pixels == secondLayer.pixels;
    }
    return ExpectTrue(equal, "RGB candidate TIFF bytes are repeatable");
}

}  // namespace

int main()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("slicesoft_rgbwsvt_cli_" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    int failures = 0;
    try
    {
        failures += ProfileProducesCandidate(
            root, "obj_mtl_texture_rgb_only_rgbwsvt", "rgb", "rgb") ? 0 : 1;
        failures += ProfileProducesCandidate(
            root, "nail_white_underbase_only_rgbwsvt", "white", "white") ? 0 : 1;
        failures += ProfileProducesCandidate(
            root, "nail_varnish_only_rgbwsvt", "varnish", "varnish") ? 0 : 1;
        failures += RgbProfileIsByteRepeatable(root) ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL LegacyRgbwsvtCliCandidateTests exception: " << error.what() << '\n';
        ++failures;
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    if (failures != 0)
    {
        std::cerr << "FAIL LegacyRgbwsvtCliCandidateTests " << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS LegacyRgbwsvtCliCandidateTests 4/4\n";
    return 0;
}
