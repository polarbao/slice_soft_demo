// MATVOL MV-08B：生产接线的端到端证明。
//
// 本用例的价值在于第二条断言，而不是第一条。仅仅「输出里出现两种材质色」不足以区分
// 真 3D 与旧的 2.5D relief_heightfield——后者每列只有一个 top_triangle_index，
// 同样能让不同列呈现不同颜色。只有【同一 XY 列在不同层归属不同材质】才是纵深的证明，
// 也是整个 MATVOL 专项的立项理由。
//
// 分辨率取中等值以保证 CTest 时长可控：BuildMaterialVolumePlan 逐列遍历该材质全部
// 三角面，生产分辨率下约 16 万列 × 2.7 万面，属分钟级，不适合放进回归套件。
// 生产分辨率的实测另行单独执行并记入报告。

#include "HostRequestBuilder.h"

#include "slicer_core/config.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackage.h"
#include "slicer_core/slicer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr std::array<std::uint8_t, 3> kGreen{63U, 190U, 126U};
constexpr std::array<std::uint8_t, 3> kPeach{255U, 220U, 198U};

bool ExpectTrue(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::filesystem::path RepositoryRoot()
{
#ifdef SLICESOFT_SOURCE_DIR
    return std::filesystem::path{SLICESOFT_SOURCE_DIR};
#else
    return std::filesystem::current_path();
#endif
}

hosteffectiveprofilesettings MakeSettings(
    const std::string& modelPath,
    const std::string& packageDirectory,
    const int dpi)
{
    hosteffectiveprofilesettings settings{};
    settings.modelpath = modelPath.c_str();
    settings.modelformat = "obj";
    settings.packagedirectory = packageDirectory.c_str();
    settings.profileid = "matvol-production-wiring";
    settings.dpix = dpi;
    settings.dpiy = dpi;
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

bool WriteHostProfile(
    const hosteffectiveprofilesettings& settings,
    const std::filesystem::path& profilePath)
{
    char profileHash[128]{};
    char* profile = HostBuildEffectiveProfile(
        &settings, profileHash, static_cast<unsigned long>(sizeof(profileHash)));
    if (profile == nullptr)
    {
        return false;
    }
    {
        std::ofstream output(profilePath, std::ios::binary | std::ios::trunc);
        output << profile;
    }
    std::free(profile);
    return true;
}

struct ColumnObservation
{
    bool sawGreen{false};
    bool sawPeach{false};
};

/// @brief 跑通生产路径并统计逐列材质观察。
bool ProductionWiringEmitsPerLayerMaterialOwnership()
{
    const std::filesystem::path model =
        RepositoryRoot() / "model" / "obj" / "reality" / "finger_suoguo" / "03.obj";
    if (!std::filesystem::exists(model))
    {
        std::cout << "SKIP production_wiring asset not present: " << model.string() << '\n';
        return true;
    }

    const std::filesystem::path workDirectory =
        std::filesystem::temp_directory_path() / "slicesoft_matvol_wiring";
    std::error_code cleanupError;
    std::filesystem::remove_all(workDirectory, cleanupError);
    std::filesystem::create_directories(workDirectory);
    const std::filesystem::path packageDirectory = workDirectory / "package";
    std::filesystem::create_directories(packageDirectory);
    const std::filesystem::path profilePath = workDirectory / "profile.json";

    const std::string modelPath = model.generic_string();
    const std::string packagePath = packageDirectory.generic_string();
    // 200 dpi：约 93 x 184 列，足以覆盖两层壳且保持秒级。
    const hosteffectiveprofilesettings settings = MakeSettings(modelPath, packagePath, 200);
    if (!ExpectTrue(WriteHostProfile(settings, profilePath), "host emits a matvol profile"))
    {
        return false;
    }

    std::vector<ColumnObservation> columns;
    std::size_t greenPixels{0};
    std::size_t peachPixels{0};

    slicer_core::SliceRunOptions options;
    // 只验证像素语义，不写盘：避免把 TIFF 与预览的耗时算进回归。
    options.write_tiff_layers = false;
    options.write_preview_files = false;
    options.write_reports = false;
    options.layercallback =
        [&columns, &greenPixels, &peachPixels](
            const slicer_core::RgbwsvProductionLayer& layer,
            const slicer_core::MaterialClosureSemanticLayerInput&) {
            const std::size_t columnCount =
                static_cast<std::size_t>(layer.widthPx) * static_cast<std::size_t>(layer.heightPx);
            if (columns.size() != columnCount)
            {
                columns.assign(columnCount, ColumnObservation{});
            }
            for (std::size_t column{0}; column < columnCount; ++column)
            {
                const std::size_t base = column * 6U;
                if (base + 2U >= layer.channels.size())
                {
                    break;
                }
                const std::uint8_t r = layer.channels[base + 0U];
                const std::uint8_t g = layer.channels[base + 1U];
                const std::uint8_t b = layer.channels[base + 2U];
                if (r == kGreen[0] && g == kGreen[1] && b == kGreen[2])
                {
                    columns[column].sawGreen = true;
                    ++greenPixels;
                }
                else if (r == kPeach[0] && g == kPeach[1] && b == kPeach[2])
                {
                    columns[column].sawPeach = true;
                    ++peachPixels;
                }
            }
        };

    try
    {
        const slicer_core::SliceRunResult result = slicer_core::run_slicer(profilePath, options);
        (void)result;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: production run threw: " << error.what() << '\n';
        return false;
    }

    std::size_t mixedColumns{0};
    for (const ColumnObservation& observation : columns)
    {
        if (observation.sawGreen && observation.sawPeach)
        {
            ++mixedColumns;
        }
    }

    std::cout << "  wiring: greenPixels=" << greenPixels
              << " peachPixels=" << peachPixels
              << " columns=" << columns.size()
              << " mixedColumns=" << mixedColumns << '\n';

    bool passed{true};
    passed = ExpectTrue(greenPixels > 0U, "material 01 green reaches the output") && passed;
    passed = ExpectTrue(peachPixels > 0U, "material 02 peach reaches the output") && passed;
    // 决定性断言：同一 XY 列在不同层归属不同材质。
    // 旧的 relief_heightfield 每列只有一个 top_triangle_index，做不到这一点。
    passed = ExpectTrue(
                 mixedColumns > 0U,
                 "at least one column carries different materials at different layers")
        && passed;

    std::filesystem::remove_all(workDirectory, cleanupError);
    return passed;
}
// 变异检验：关闭 materialVolumePolicy 跑同一模型，同列多材质必须消失。
// 没有这条断言，无法排除「两种颜色与同列差异来自别的路径」这一可能，
// 也就无法证明 MV-08B 的接线确实承重。
bool DisablingMaterialVolumeRemovesPerLayerOwnership()
{
    const std::filesystem::path model =
        RepositoryRoot() / "model" / "obj" / "reality" / "finger_suoguo" / "03.obj";
    if (!std::filesystem::exists(model))
    {
        std::cout << "SKIP disabled_policy asset not present" << '\n';
        return true;
    }
    const std::filesystem::path workDirectory =
        std::filesystem::temp_directory_path() / "slicesoft_matvol_disabled";
    std::error_code cleanupError;
    std::filesystem::remove_all(workDirectory, cleanupError);
    std::filesystem::create_directories(workDirectory);
    const std::filesystem::path packageDirectory = workDirectory / "package";
    std::filesystem::create_directories(packageDirectory);
    const std::filesystem::path profilePath = workDirectory / "profile.json";

    const std::string modelPath = model.generic_string();
    const std::string packagePath = packageDirectory.generic_string();
    hosteffectiveprofilesettings settings = MakeSettings(modelPath, packagePath, 200);
    settings.materialvolumeenabled = 0;
    if (!ExpectTrue(WriteHostProfile(settings, profilePath), "host emits a legacy profile"))
    {
        return false;
    }

    std::vector<ColumnObservation> columns;
    slicer_core::SliceRunOptions options;
    options.write_tiff_layers = false;
    options.write_preview_files = false;
    options.write_reports = false;
    options.layercallback =
        [&columns](
            const slicer_core::RgbwsvProductionLayer& layer,
            const slicer_core::MaterialClosureSemanticLayerInput&) {
            const std::size_t columnCount =
                static_cast<std::size_t>(layer.widthPx) * static_cast<std::size_t>(layer.heightPx);
            if (columns.size() != columnCount)
            {
                columns.assign(columnCount, ColumnObservation{});
            }
            for (std::size_t column{0}; column < columnCount; ++column)
            {
                const std::size_t base = column * 6U;
                if (base + 2U >= layer.channels.size())
                {
                    break;
                }
                const std::uint8_t r = layer.channels[base + 0U];
                const std::uint8_t g = layer.channels[base + 1U];
                const std::uint8_t b = layer.channels[base + 2U];
                if (r == kGreen[0] && g == kGreen[1] && b == kGreen[2])
                {
                    columns[column].sawGreen = true;
                }
                else if (r == kPeach[0] && g == kPeach[1] && b == kPeach[2])
                {
                    columns[column].sawPeach = true;
                }
            }
        };

    try
    {
        const slicer_core::SliceRunResult result = slicer_core::run_slicer(profilePath, options);
        (void)result;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: legacy run threw: " << error.what() << '\n';
        return false;
    }

    std::size_t mixedColumns{0};
    for (const ColumnObservation& observation : columns)
    {
        if (observation.sawGreen && observation.sawPeach)
        {
            ++mixedColumns;
        }
    }
    std::cout << "  legacy: columns=" << columns.size()
              << " mixedColumns=" << mixedColumns << '\n';
    std::filesystem::remove_all(workDirectory, cleanupError);
    return ExpectTrue(
        mixedColumns == 0U,
        "disabling materialVolumePolicy removes per-layer material ownership");
}

}  // namespace

int main()
{
    int failures{0};
    if (!ProductionWiringEmitsPerLayerMaterialOwnership())
    {
        std::cerr << "CASE FAILED: production_wiring" << '\n';
        ++failures;
    }
    if (!DisablingMaterialVolumeRemovesPerLayerOwnership())
    {
        std::cerr << "CASE FAILED: disabled_policy" << '\n';
        ++failures;
    }
    if (failures != 0)
    {
        std::cerr << "FAIL MatvolProductionWiringTests" << '\n';
        return 1;
    }
    std::cout << "PASS MatvolProductionWiringTests 2/2" << '\n';
    return 0;
}
