#include "slicer_core/config.h"
#include "slicer_core/materials/transfer/TransferChannelError.h"
#include "slicer_core/materials/transfer/TransferMaterialResolver.h"
#include "slicer_core/model.h"
#include "slicer_core/model/ModelLoadConfig.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

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

std::filesystem::path RealityAsset(const std::string& fileName)
{
#ifdef SLICESOFT_SOURCE_DIR
    return std::filesystem::path{SLICESOFT_SOURCE_DIR} / "model" / "obj" / "reality"
        / "finger_suoguo" / fileName;
#else
    return std::filesystem::path{"model"} / "obj" / "reality" / "finger_suoguo"
        / fileName;
#endif
}

slicer_core::ModelReport LoadModel(const std::filesystem::path& path)
{
    slicer_core::ModelLoadConfig config;
    config.input.model_path = path;
    config.input.format = "obj";
    config.auto_orient.enabled = false;
    return slicer_core::load_model_report(config, path.parent_path());
}

slicer_core::TransferChannelPolicyConfig PolicyFor(
    const std::array<std::uint8_t, 3>& rgb)
{
    slicer_core::TransferChannelPolicyConfig policy;
    policy.enabled = true;
    policy.material_diffuse_rgb_values.push_back(rgb);
    return policy;
}

bool Reality03ResolvesMaterial02AsTransfer()
{
    const std::filesystem::path path = RealityAsset("03.obj");
    if (!std::filesystem::exists(path))
    {
        std::cout << "SKIP 03.obj not present\n";
        return true;
    }
    const slicer_core::ModelReport model = LoadModel(path);
    const slicer_core::TransferMaterialMatch match = slicer_core::ResolveTransferMaterial(
        PolicyFor({255U, 220U, 198U}), model.material_infos);
    return ExpectTrue(match.present, "03 transfer material is present")
        && ExpectTrue(match.materialName == "02", "03 material 02 is transfer, not material 01")
        && ExpectTrue(
            match.diffuseRgb == std::array<std::uint8_t, 3>{255U, 220U, 198U},
            "03 transfer colour comes from MTL Kd");
}

bool Reality08And09UseConfiguredYellowWithoutHardcoding()
{
    bool passed{true};
    for (const char* fileName : {"08.obj", "09.obj"})
    {
        const std::filesystem::path path = RealityAsset(fileName);
        if (!std::filesystem::exists(path))
        {
            std::cout << "SKIP " << fileName << " not present\n";
            continue;
        }
        const slicer_core::ModelReport model = LoadModel(path);
        const slicer_core::TransferMaterialMatch match = slicer_core::ResolveTransferMaterial(
            PolicyFor({255U, 255U, 0U}), model.material_infos);
        passed = ExpectTrue(match.present, std::string{fileName} + " transfer material is present")
            && passed;
        passed = ExpectTrue(
                     match.materialName == "02",
                     std::string{fileName} + " material 02 is transfer")
            && passed;
    }
    return passed;
}

bool MissingRegionCanKeepLegacyModelSemantics()
{
    const std::vector<slicer_core::MaterialInfo> infos{
        slicer_core::MaterialInfo{"01", {63U, 190U, 126U}, true}};
    const slicer_core::TransferMaterialMatch match = slicer_core::ResolveTransferMaterial(
        PolicyFor({255U, 220U, 198U}), infos);
    return ExpectTrue(!match.present, "zero colour matches produces an empty T region");
}

bool MissingRequiredRegionFailsClosed()
{
    slicer_core::TransferChannelPolicyConfig policy = PolicyFor({255U, 220U, 198U});
    policy.missing_region = "fail_closed";
    const std::vector<slicer_core::MaterialInfo> infos{
        slicer_core::MaterialInfo{"01", {63U, 190U, 126U}, true}};
    try
    {
        (void)slicer_core::ResolveTransferMaterial(policy, infos);
    }
    catch (const slicer_core::TransferChannelError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::TransferChannelErrorCode::RegionMissing,
            "required missing region has a stable error code");
    }
    return ExpectTrue(false, "required missing region must fail closed");
}

bool MultipleMatchesFailClosed()
{
    const std::vector<slicer_core::MaterialInfo> infos{
        slicer_core::MaterialInfo{"02", {255U, 220U, 198U}, true},
        slicer_core::MaterialInfo{"transfer_copy", {255U, 220U, 198U}, true}};
    try
    {
        (void)slicer_core::ResolveTransferMaterial(
            PolicyFor({255U, 220U, 198U}), infos);
    }
    catch (const slicer_core::TransferChannelError& error)
    {
        return ExpectTrue(
            error.Code() == slicer_core::TransferChannelErrorCode::MatchAmbiguous,
            "multiple matching materials have a stable error code");
    }
    return ExpectTrue(false, "multiple matching materials must fail closed");
}

bool DisabledPolicyNeverInfersFromMaterialName()
{
    slicer_core::TransferChannelPolicyConfig policy;
    const std::vector<slicer_core::MaterialInfo> infos{
        slicer_core::MaterialInfo{"02", {255U, 220U, 198U}, true}};
    const slicer_core::TransferMaterialMatch match =
        slicer_core::ResolveTransferMaterial(policy, infos);
    return ExpectTrue(!match.present, "disabled policy does not infer transfer from name 02");
}

bool NewProtocolConfigurationParsesExactColours()
{
#ifdef SLICESOFT_SOURCE_DIR
    const std::filesystem::path path = std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "samples" / "configs" / "matvol_t" / "transfer_rgbwsvt_prototype.json";
#else
    const std::filesystem::path path = std::filesystem::path{"samples"} / "configs"
        / "matvol_t" / "transfer_rgbwsvt_prototype.json";
#endif
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
    return ExpectTrue(config.output.package_protocol == "p0.rgbwsvt.1", "new protocol parses")
        && ExpectTrue(config.transfer_channel_policy.enabled, "T policy parses enabled")
        && ExpectTrue(
            config.transfer_channel_policy.material_diffuse_rgb_values
                == std::vector<std::array<std::uint8_t, 3>>{{255U, 220U, 198U}},
            "configured Kd colour parses without software constants");
}

bool OldProcessConfigurationKeepsSixChannelProtocol()
{
#ifdef SLICESOFT_SOURCE_DIR
    const std::filesystem::path path = std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "samples" / "configs" / "material_process"
        / "obj_mtl_texture_rgb_only.json";
#else
    const std::filesystem::path path = std::filesystem::path{"samples"} / "configs"
        / "material_process" / "obj_mtl_texture_rgb_only.json";
#endif
    const slicer_core::SliceConfig config = slicer_core::load_slice_config(path);
    return ExpectTrue(config.output.package_protocol == "p0.rgbwsv.2", "old protocol remains default")
        && ExpectTrue(!config.transfer_channel_policy.enabled, "old process does not emit T")
        && ExpectTrue(config.output.channel_order.size() == 6U, "old process keeps six channels");
}

bool NewCraftProcessCopiesUseSevenChannelProtocol()
{
#ifdef SLICESOFT_SOURCE_DIR
    const std::filesystem::path directory = std::filesystem::path{SLICESOFT_SOURCE_DIR}
        / "samples" / "configs" / "matvol_t" / "process_profiles";
#else
    const std::filesystem::path directory = std::filesystem::path{"samples"} / "configs"
        / "matvol_t" / "process_profiles";
#endif
    bool passed{true};
    for (const char* fileName : {
             "nail_rgb_white_varnish_top1_rgbwsvt.json",
             "nail_rgb_white_varnish_top2_rgbwsvt.json",
             "nail_rgb_white_varnish_top3_rgbwsvt.json",
             "nail_varnish_only_rgbwsvt.json",
             "nail_white_underbase_only_rgbwsvt.json",
             "obj_mtl_texture_rgb_only_rgbwsvt.json",
             "obj_mtl_texture_rgb_varnish_rgbwsvt.json",
             "obj_mtl_texture_rgb_white_ondemand_rgbwsvt.json",
             "obj_mtl_texture_rgb_white_varnish_rgbwsvt.json",
             "three_mf_texture_rgb_white_varnish_rgbwsvt.json"})
    {
        const slicer_core::SliceConfig config =
            slicer_core::load_slice_config(directory / fileName);
        passed = ExpectTrue(
                     config.output.package_protocol == "p0.rgbwsvt.1",
                     std::string{fileName} + " opts into the new protocol")
            && passed;
        passed = ExpectTrue(
                     config.transfer_channel_policy.enabled,
                     std::string{fileName} + " enables configured T routing")
            && passed;
    }
    return passed;
}

}  // namespace

int main()
{
    int failures{0};
    const auto run = [&failures](const bool passed, const char* name)
    {
        if (!passed)
        {
            std::cerr << "CASE FAILED " << name << '\n';
            ++failures;
        }
    };
    run(Reality03ResolvesMaterial02AsTransfer(), "reality_03_role");
    run(Reality08And09UseConfiguredYellowWithoutHardcoding(), "reality_08_09_configured_colour");
    run(MissingRegionCanKeepLegacyModelSemantics(), "missing_optional");
    run(MissingRequiredRegionFailsClosed(), "missing_required");
    run(MultipleMatchesFailClosed(), "multiple_matches");
    run(DisabledPolicyNeverInfersFromMaterialName(), "disabled_no_name_inference");
    run(NewProtocolConfigurationParsesExactColours(), "new_protocol_config");
    run(OldProcessConfigurationKeepsSixChannelProtocol(), "old_protocol_config");
    run(NewCraftProcessCopiesUseSevenChannelProtocol(), "new_craft_process_copies");
    if (failures != 0)
    {
        std::cerr << "FAIL MatvolTransferResolverTests " << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS MatvolTransferResolverTests 9/9\n";
    return 0;
}
