// MATVOL 宿主 Profile 往返门禁。
//
// 存在理由（2026-08-24）：MV-07A 曾在宿主模板里发射
// `materialProcessProfile.rgb.source = "material_volume"`，而切片侧的合法集合只有
// `texture_or_color` 与 `modelMaterial`，导致参考宿主一按「开始切片」即被
// PM-SLICER-PROFILE-0030 拒绝。该缺陷能够出厂，是因为当时唯一的证据是
// profileHash 闭合——它只比对宿主手写规范化文本与 Json::dump(0) 是否逐字节相同，
// 校验的是【序列化正确性】，不是【切片侧是否接受】。全仓此前没有任何测试把宿主
// 发射的 Profile 文本喂回 slicer_core 校验，本文件补上这道门。

#include "slicer_core/config.h"

extern "C"
{
#include "HostRequestBuilder.h"
}

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
bool ExpectTrue(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::filesystem::path RepoRoot()
{
#ifdef SLICESOFT_SOURCE_DIR
    return std::filesystem::path{SLICESOFT_SOURCE_DIR};
#else
    return std::filesystem::current_path();
#endif
}

/// @brief 复刻 HostProcessPresetCatalog 里 MATVOL 候选预设的有效设置。
hosteffectiveprofilesettings MakeMatvolSettings(
    const std::string& modelPath,
    const std::string& packageDirectory)
{
    hosteffectiveprofilesettings settings{};
    settings.modelpath = modelPath.c_str();
    settings.modelformat = "obj";
    settings.packagedirectory = packageDirectory.c_str();
    settings.profileid = "matvol-round-trip";
    settings.dpix = 600;
    settings.dpiy = 600;
    settings.layerthicknessmm = 0.038;
    settings.materialstrategy = HOST_MATERIAL_RGB_WHITE;
    settings.varnishtoplayers = 1;
    settings.texturetopsurfacelayers = 1;
    settings.supportenabled = 1;
    settings.supportmode = HOST_SUPPORT_BOTTOM_PROJECTION;
    settings.baseprojectionlayercount = 30;
    settings.geometrysamplingstrategy = HOST_GEOMETRY_SAMPLING_LEGACY_CENTER;
    settings.materialvolumeenabled = 1;
    settings.materialvolumeprimaryname = "01";
    settings.materialvolumeprimarypriority = 200;
    settings.materialvolumesecondaryname = "02";
    settings.materialvolumesecondarypriority = 100;
    return settings;
}

/// @brief 把宿主发射的 Profile 落盘后交给 slicer_core 解析与校验。
///        返回空字符串表示接受；否则返回 what() 文本。
std::string ValidateEmittedProfile(
    const hosteffectiveprofilesettings& settings,
    const std::filesystem::path& profilePath,
    bool* emitted)
{
    char profileHash[128]{};
    char* profile = HostBuildEffectiveProfile(
        &settings, profileHash, static_cast<unsigned long>(sizeof(profileHash)));
    *emitted = profile != nullptr;
    if (profile == nullptr)
    {
        return "host refused to emit a profile";
    }
    {
        std::ofstream output(profilePath, std::ios::binary | std::ios::trunc);
        output << profile;
    }
    std::free(profile);
    try
    {
        const slicer_core::SliceConfig config =
            slicer_core::load_slice_config(profilePath);
        slicer_core::validate_slice_config(config);
    }
    catch (const std::exception& error)
    {
        return error.what();
    }
    return {};
}

/// @brief MATVOL 候选预设发射的 Profile 必须被切片侧【契约层】接受。
///        生产执行另有 EnsureMaterialVolumeWiringImplemented 的 fail-closed，
///        与本用例无关：本用例只锁定宿主与切片侧的字段取值不发生漂移。
bool MatvolPresetProfileIsAcceptedByContract()
{
    const std::filesystem::path model =
        RepoRoot() / "model" / "obj" / "reality" / "finger_suoguo" / "03.obj";
    if (!std::filesystem::exists(model))
    {
        std::cout << "SKIP matvol_host_profile_round_trip asset not present: "
                  << model.string() << '\n';
        return true;
    }
    const std::filesystem::path outputDirectory =
        std::filesystem::temp_directory_path() / "matvol_round_trip";
    std::filesystem::create_directories(outputDirectory);
    const std::string modelPath = model.string();
    const std::string packageDirectory = (outputDirectory / "package").string();

    const hosteffectiveprofilesettings settings =
        MakeMatvolSettings(modelPath, packageDirectory);
    bool emitted{false};
    const std::string error = ValidateEmittedProfile(
        settings, outputDirectory / "matvol_profile.json", &emitted);

    bool passed{true};
    passed = ExpectTrue(emitted, "host emits a matvol profile") && passed;
    if (!error.empty())
    {
        std::cerr << "  slicer rejected the emitted profile: " << error << '\n';
    }
    passed = ExpectTrue(
                 error.empty(),
                 "slicer_core accepts the matvol profile emitted by the host")
        && passed;
    return passed;
}

/// @brief 关闭 MATVOL 时走既有模板，同样必须被接受；用于区分「模板本身坏了」
///        与「MATVOL 分支坏了」。
bool LegacyPresetProfileIsAcceptedByContract()
{
    const std::filesystem::path model =
        RepoRoot() / "model" / "obj" / "reality" / "finger_suoguo" / "03.obj";
    if (!std::filesystem::exists(model))
    {
        std::cout << "SKIP matvol_host_profile_round_trip_legacy asset not present\n";
        return true;
    }
    const std::filesystem::path outputDirectory =
        std::filesystem::temp_directory_path() / "matvol_round_trip";
    std::filesystem::create_directories(outputDirectory);
    const std::string modelPath = model.string();
    const std::string packageDirectory = (outputDirectory / "package_legacy").string();

    hosteffectiveprofilesettings settings =
        MakeMatvolSettings(modelPath, packageDirectory);
    settings.materialvolumeenabled = 0;
    settings.materialvolumeprimaryname = "";
    settings.materialvolumeprimarypriority = 0;
    settings.materialvolumesecondaryname = "";
    settings.materialvolumesecondarypriority = 0;

    bool emitted{false};
    const std::string error = ValidateEmittedProfile(
        settings, outputDirectory / "legacy_profile.json", &emitted);

    bool passed{true};
    passed = ExpectTrue(emitted, "host emits a legacy profile") && passed;
    if (!error.empty())
    {
        std::cerr << "  slicer rejected the legacy profile: " << error << '\n';
    }
    passed = ExpectTrue(
                 error.empty(),
                 "slicer_core accepts the legacy profile emitted by the host")
        && passed;
    return passed;
}
}

int main()
{
    struct TestCase
    {
        const char* name;
        bool (*run)();
    };
    const TestCase cases[] = {
        {"matvol_preset_profile_accepted", &MatvolPresetProfileIsAcceptedByContract},
        {"legacy_preset_profile_accepted", &LegacyPresetProfileIsAcceptedByContract},
    };

    int failures{0};
    for (const TestCase& testCase : cases)
    {
        if (!testCase.run())
        {
            std::cerr << "CASE FAILED: " << testCase.name << '\n';
            ++failures;
        }
    }
    if (failures != 0)
    {
        std::cerr << "FAIL MatvolHostProfileRoundTripTests " << failures << " case(s)\n";
        return 1;
    }
    std::cout << "PASS MatvolHostProfileRoundTripTests "
              << (sizeof(cases) / sizeof(cases[0])) << "/"
              << (sizeof(cases) / sizeof(cases[0])) << '\n';
    return 0;
}
