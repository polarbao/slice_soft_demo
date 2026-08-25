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
#include "slicer_core/rip_reader.h"
#include "slicer_core/system/ProcessMemoryStats.h"
#include "slicer_core/slicer.h"

#include <array>
#include <chrono>
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

std::size_t g_uncancelledMs{0};

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

    const auto uncancelledStart = std::chrono::steady_clock::now();
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

    g_uncancelledMs = static_cast<std::size_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - uncancelledStart).count());
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

// MV-08C：体积报告必须真正落盘，且内容不得退化。
// 两条断言各自对应一个曾经存在的缺口：
//   - topology 不得为 invalid —— plan 内部算出的拓扑事实若不外传，报告会全体退化，
//     schema 仍然合法但事实错误，属最难发现的一类缺陷；
//   - warnings 必须列出被放行的自交材质 —— MQ-05 明令不得静默吞掉，
//     否则产出物上完全看不出这一版是在放宽策略下切出来的。
bool VolumeReportIsEmittedWithFacts()
{
    const std::filesystem::path model =
        RepositoryRoot() / "model" / "obj" / "reality" / "finger_suoguo" / "03.obj";
    if (!std::filesystem::exists(model))
    {
        std::cout << "SKIP volume_report asset not present" << '\n';
        return true;
    }
    const std::filesystem::path workDirectory =
        std::filesystem::temp_directory_path() / "slicesoft_matvol_report";
    std::error_code cleanupError;
    std::filesystem::remove_all(workDirectory, cleanupError);
    std::filesystem::create_directories(workDirectory);
    const std::filesystem::path packageDirectory = workDirectory / "package";
    std::filesystem::create_directories(packageDirectory);
    const std::filesystem::path profilePath = workDirectory / "profile.json";

    const std::string modelPath = model.generic_string();
    const std::string packagePath = packageDirectory.generic_string();
    const hosteffectiveprofilesettings settings = MakeSettings(modelPath, packagePath, 120);
    if (!ExpectTrue(WriteHostProfile(settings, profilePath), "host emits a matvol profile"))
    {
        return false;
    }

    slicer_core::SliceRunOptions options;
    // 严格 RIP 读取器要求 TIFF 层文件真实存在，且 manifest.json 只在 write_reports
    // 下才落盘，故两者都必须打开；仅开报告会以 E_LAYER_MISSING 失败。
    options.write_tiff_layers = true;
    options.write_preview_files = false;
    options.write_reports = true;
    slicer_core::SliceRunResult result;
    try
    {
        result = slicer_core::run_slicer(profilePath, options);
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: report run threw: " << error.what() << '\n';
        return false;
    }

    const std::filesystem::path reportPath =
        packageDirectory / "reports" / "material_volume_report.json";
    bool passed = ExpectTrue(
        std::filesystem::exists(reportPath), "volume report is written into the package");
    if (!passed)
    {
        return false;
    }
    std::ifstream input(reportPath, std::ios::binary);
    const std::string report(
        (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();

    passed = ExpectTrue(
                 report.find("slicesoft.material_volume_report.1") != std::string::npos,
                 "report declares its schema")
        && passed;
    passed = ExpectTrue(
                 report.find("closed_orientable") != std::string::npos,
                 "topology facts reach the report instead of degrading to invalid")
        && passed;
    passed = ExpectTrue(
                 report.find("self_intersecting") != std::string::npos,
                 "the self-intersecting material is reported as such")
        && passed;
    passed = ExpectTrue(
                 report.find("tolerate_closed_self_intersection") != std::string::npos,
                 "warnings disclose the tolerated self-intersecting material")
        && passed;
    std::cout << "  report: bytes=" << report.size() << '\n';

    // MV-09：Package / RIP strict。严格读取器不读 manifest.reports，故 MV-08C 新增的
    // materialVolume 键对它不可见；本断言锁定的是「MATVOL 产出的包本身通过严格校验」。
    // 外层捕获 std::exception 而非仅 ValidationError：rip_reader 对 manifest.tiff 用的是
    // 无检查的 .at()，缺失时抛的是 std::out_of_range，只接前者会让回归以崩溃而非失败呈现。
    try
    {
        const slicer_core::RipValidationResult validation =
            slicer_core::validate_slice_package(result.package_dir);
        passed = ExpectTrue(
                     validation.layer_count == result.layer_count,
                     "strict RIP layer count agrees with the run")
            && passed;
        passed = ExpectTrue(validation.bit_depth == 8, "strict RIP sees 8-bit channels")
            && passed;
        std::cout << "  rip strict: schema=" << validation.schema
                  << " grid=" << validation.width_px << "x" << validation.height_px
                  << "x" << validation.layer_count
                  << " storage=" << validation.storage_mode
                  << " compression=" << validation.compression << '\n';
    }
    catch (const slicer_core::ValidationError& error)
    {
        std::cerr << "FAILED: strict RIP rejected the MATVOL package: "
                  << slicer_core::validation_error_code_string(error.code())
                  << " " << error.what() << '\n';
        passed = false;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: strict RIP validation threw: " << error.what() << '\n';
        passed = false;
    }

    // MQ-04 回签要求：MV-04 起每张卡都必须把实测耗时与峰值内存记入报告。
    // 此处只【记录】不【判定】——设备 SLA 仍为 INPUT OPEN，只卡 MV-10。
    // 内存由调用方自行 bracket：run_slicer 不采集，SliceRunProfile 里也没有内存字段。
    const slicer_core::ProcessMemoryStats memoryAfter =
        slicer_core::CaptureProcessMemoryStats();
    std::cout << "  perf: totalMs=" << result.profile.total_ms
              << " maskSamplingMs=" << result.profile.mask_sampling_ms
              << " layerComputeMs=" << result.profile.layer_compute_ms
              << " peakWorkingSetBytes="
              << (memoryAfter.available ? memoryAfter.peak_working_set_bytes : 0U)
              << '\n';


    std::filesystem::remove_all(workDirectory, cleanupError);
    return passed;
}

// MV-09 cancel：生产路径的取消必须在 plan 构建窗口内生效。
// 该窗口是本路径最长的不可中断段（逐列遍历全部三角面），且发生在 gridcallback 之前，
// 因此仅靠既有的回调取消完全覆盖不到它——这正是此前该项被判定「受阻」的原因。
bool ProductionCancellationStopsPlanBuild()
{
    const std::filesystem::path model =
        RepositoryRoot() / "model" / "obj" / "reality" / "finger_suoguo" / "03.obj";
    if (!std::filesystem::exists(model))
    {
        std::cout << "SKIP cancellation asset not present" << '\n';
        return true;
    }
    const std::filesystem::path workDirectory =
        std::filesystem::temp_directory_path() / "slicesoft_matvol_cancel";
    std::error_code cleanupError;
    std::filesystem::remove_all(workDirectory, cleanupError);
    std::filesystem::create_directories(workDirectory);
    const std::filesystem::path packageDirectory = workDirectory / "package";
    std::filesystem::create_directories(packageDirectory);
    const std::filesystem::path profilePath = workDirectory / "profile.json";

    const std::string modelPath = model.generic_string();
    const std::string packagePath = packageDirectory.generic_string();
    const hosteffectiveprofilesettings settings = MakeSettings(modelPath, packagePath, 200);
    if (!ExpectTrue(WriteHostProfile(settings, profilePath), "host emits a matvol profile"))
    {
        return false;
    }

    slicer_core::SliceRunOptions options;
    options.write_tiff_layers = false;
    options.write_preview_files = false;
    options.write_reports = false;
    // 立即取消：plan 构建在每条光栅行开头检查一次，故第一行即命中。
    options.cancellationRequested = []() { return true; };

    bool passed{true};
    const auto start = std::chrono::steady_clock::now();
    try
    {
        const slicer_core::SliceRunResult result = slicer_core::run_slicer(profilePath, options);
        (void)result;
        passed = ExpectTrue(false, "cancelled run must not complete");
    }
    catch (const std::exception& error)
    {
        const std::string message = error.what();
        // 取消复用 E_MATVOL_BUDGET_EXCEEDED：错误码枚举属 DEV_MATVOL 冻结清单，
        // 新增 Cancelled 值是合同变更而非代码变更，故此处按既有约定断言而不去改枚举。
        passed = ExpectTrue(
            message.find("E_MATVOL_BUDGET_EXCEEDED") != std::string::npos,
            "cancellation fails closed with the frozen budget code");
    }
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    // 仅「抛出取消」不足以证明是早期取消——ThrowIfCancelled 在列循环之后还有一次调用，
    // 跑完整个构建再报也会抛。真正区分二者的是耗时，故必须有时间断言。
    // 上界不拍数字，而以同一测试内【未取消】那次的实测耗时自校准：
    // 取消路径应显著短于完整路径。基准不可用时退回一个宽松固定值。
    const std::size_t budgetMs = g_uncancelledMs > 0U
        ? (g_uncancelledMs * 3U) / 4U
        : 30000U;
    passed = ExpectTrue(
                 static_cast<std::size_t>(elapsedMs) < budgetMs,
                 "cancellation takes effect during the plan build, not after it")
        && passed;
    std::cout << "  cancel budget: uncancelledMs=" << g_uncancelledMs
              << " budgetMs=" << budgetMs << '\n';
    std::cout << "  cancel: elapsedMs=" << elapsedMs << '\n';
    std::filesystem::remove_all(workDirectory, cleanupError);
    return passed;
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
    if (!VolumeReportIsEmittedWithFacts())
    {
        std::cerr << "CASE FAILED: volume_report" << '\n';
        ++failures;
    }
    if (!ProductionCancellationStopsPlanBuild())
    {
        std::cerr << "CASE FAILED: cancellation" << '\n';
        ++failures;
    }
    if (failures != 0)
    {
        std::cerr << "FAIL MatvolProductionWiringTests" << '\n';
        return 1;
    }
    std::cout << "PASS MatvolProductionWiringTests 4/4" << '\n';
    return 0;
}
