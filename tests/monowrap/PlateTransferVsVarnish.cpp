/**
 * @file
 * @brief MW3-10：整版缩裹与单材料光油的**逐像素几何对拍**。
 *
 * 用户 2026-09-20 裁定：整版缩裹「仿照单材料光油 / 白墨的方式处理」。
 * 于是判据不是数字对账，而是直接对拍——**同一版、同一批实例，
 * 分别用单材料光油与整模缩裹各切一次，打印像素的逐像素几何必须完全相同**，
 * 只是一个落在 V 通道、另一个落在 T 通道。
 *
 * 为什么不比「T 像素总数 == 各件模型像素之和」：那只是数字相等。
 * 总数对而位置错位的 bug（偏移算错、实例配错层）它一个都抓不到。
 *
 * 为什么不比通道校验和：光油的打印值取自配置、T 恒为 0，
 * 值不同而几何可能相同，直接比校验和会误报。故比的是「哪些像素被打印」。
 *
 * 网格刻意取粗（低 DPI、厚层）：本条不变量说的是**通道落点**，与分辨率无关，
 * 粗网格一样能证，且秒级跑完。产线分辨率的验证由字节级基线负责。
 */
#include "../support/MonowrapPlateScene.h"
#include "slicer_core/TiffReadApi.h"
#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/model.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtProtocol.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"
#include "slicer_core/api/Cancellation.h"
#include "slicer_core/api/SliceFacade.h"
#include "slicer_core/engine/ProductionSliceFacadeFactory.h"
#include "slicer_core/pipeline/MultiModelProductionService.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"
#include "slicer_core/scene/SceneModel.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

/// 与 CMake 的 SKIP_RETURN_CODE 对应，改动时必须两处同步。
constexpr int kSkipReturnCode{111};
using monowrap_test::kDpi;
using monowrap_test::kInstanceCount;
using monowrap_test::kLayerHeightMm;
using monowrap_test::kOriginOffsetMm;
using monowrap_test::BuildPlateScene;
using monowrap_test::SourceRoot;
using monowrap_test::WriteProfileVariant;
constexpr std::size_t kRgbwsvChannels{6U};
constexpr std::size_t kRgbwsvtChannels{7U};
constexpr std::size_t kVarnishChannel{5U};
constexpr std::size_t kTransferChannel{6U};
constexpr std::uint8_t kEmptyValue{255U};


struct PlateRun
{
    bool valid{false};
    std::string error;
    std::filesystem::path packageDir;
    /// Worker 证据检查要的两个维度。见下方 RunPlate 里的说明。
    int gridWidthPx{0};
    int gridHeightPx{0};
    int layerCount{0};
    std::filesystem::path manifestPath;
};

/// 不取消的取消源：本测试不验取消路径。
class NeverCancelled final : public slicer_core::api::ICancelToken
{
public:
    [[nodiscard]] bool IsCancelRequested() const noexcept override
    {
        return false;
    }
};

PlateRun RunPlate(
    const std::filesystem::path& baseProfile,
    const std::vector<std::filesystem::path>& modelPaths,
    const std::filesystem::path& root,
    const bool transferChannel)
{
    std::vector<std::filesystem::path> profiles;
    for (std::size_t index{0U}; index < modelPaths.size(); ++index)
    {
        profiles.push_back(WriteProfileVariant(
            baseProfile,
            root / ("profile_" + std::to_string(index) + ".json"),
            modelPaths.at(index)));
    }

    slicer_core::SceneEffectiveConfigRequest effectiveRequest;
    effectiveRequest.scene = BuildPlateScene(profiles);
    effectiveRequest.sourcescenepath = root / "scene_config.draft.json";
    effectiveRequest.generatedconfigpath = root / "scene_config.effective.json";
    effectiveRequest.sourceprofileid = effectiveRequest.scene.resolvedprofileid;
    effectiveRequest.sourceprofileconfigpath = profiles.front();
    effectiveRequest.outputpackagedir = root / "package";
    effectiveRequest.generatedatutc = "2026-09-20T00:00:00.000Z";
    effectiveRequest.dpix = kDpi;
    effectiveRequest.dpiy = kDpi;
    effectiveRequest.layerheightmm = kLayerHeightMm;
    effectiveRequest.slicepipelinemode = "legacy";
    // 产生【生产准入】的包：不置位时准入写成 functional_fixture_admitted，
    // 而七通道回读自检只认 admitted / rgbwsvt_candidate_unvalidated。
    // 这是本测试自己的取值，不是产品限制。
    effectiveRequest.production = true;

    PlateRun run;
    const slicer_core::SceneEffectiveConfigResult effective =
        slicer_core::WriteSceneEffectiveConfig(effectiveRequest);
    if (!effective.IsValid())
    {
        // 带上字段与原因：只说「invalid」等于把排错成本转嫁给下一个人。
        run.error = "effective config invalid";
        if (effective.error.has_value())
        {
            run.error += " (field=" + effective.error->field
                + ", message=" + effective.error->message + ")";
        }
        return run;
    }

    // 走【生产门面】而不是直接调多模型服务——那是 Worker 实际用的边界。
    //
    // 本测试最初直接调 RunMultiModelProductionService，包写得对，
    // 但返回给调用方的 SliceResult 少填了 grid_px，于是实机以
    // PM-SLICER-CONTRACT-0060「incomplete package evidence」失败，
    // 而测试全绿。在比真实调用方低一层的地方验证，就是这个后果。
    const std::unique_ptr<slicer_core::api::SliceFacade> facade =
        slicer_core::engine::CreateProductionSliceFacade();
    slicer_core::api::SliceRequest sliceRequest;
    sliceRequest.job_id = "monowrap-plate";
    sliceRequest.correlation_id = "monowrap-plate-corr";
    sliceRequest.scene_config_path = effectiveRequest.generatedconfigpath;
    // 门面要求三项身份齐全（job / correlation / sceneHash），缺一即
    // PM-SLICER-PROFILE-0030。sceneHash 取场景本身的哈希，与 Worker 一致。
    // 有效配置里存的是【不带 sha256: 前缀】的裸哈希，与 Worker 那侧的
    // externalSceneHash（带前缀）不是同一种写法，别混用。
    sliceRequest.scene_hash =
        slicer_core::ComputeMultiModelSceneHash(effectiveRequest.scene);
    sliceRequest.package_dir = effectiveRequest.outputpackagedir;
    sliceRequest.output_contract = transferChannel
        ? std::string{slicer_core::CurrentRgbwsvtProtocol().schema}
        : std::string{"p0.rgbwsv.2"};

    const NeverCancelled cancelToken;
    const slicer_core::api::ApiResult<slicer_core::api::SliceResult> produced =
        facade->Run(sliceRequest, cancelToken, {});
    if (!produced.IsOk() || produced.Value() == nullptr)
    {
        run.error = produced.Error() != nullptr
            ? (produced.Error()->code + ": " + produced.Error()->message)
            : "slice facade returned no result";
        return run;
    }
    const slicer_core::api::SliceResult& result = *produced.Value();
    run.valid = true;
    run.packageDir = result.package_dir;
    run.manifestPath = result.manifest_path;
    run.layerCount = result.layer_count;
    run.gridWidthPx = result.grid_px[0];
    run.gridHeightPx = result.grid_px[1];
    return run;
}

/// 原样抄 Worker 的产出证据判据（WorkerSliceExecutor.cpp:443-450）。
/// 抄而不是引用，是因为那是 Worker 侧的私有逻辑；抄过来的代价是要跟着它改，
/// 收益是本测试能在核心侧就挡住「摘要少填字段」这一类错误。
bool HasCompletePackageEvidence(const PlateRun& run, std::string& reason)
{
    if (run.packageDir.empty())
    {
        reason = "package_dir 为空";
        return false;
    }
    if (run.manifestPath != run.packageDir / "manifest.json"
        || !std::filesystem::is_regular_file(run.manifestPath))
    {
        reason = "manifest_path 不指向已发布的清单";
        return false;
    }
    if (run.layerCount <= 0)
    {
        reason = "layer_count <= 0";
        return false;
    }
    if (run.gridWidthPx <= 0 || run.gridHeightPx <= 0)
    {
        reason = "grid_px 有维度 <= 0（实机报 CONTRACT-0060 的正是这一条）";
        return false;
    }
    return true;
}

std::vector<std::filesystem::path> LayerPaths(const std::filesystem::path& packageDir)
{
    std::vector<std::filesystem::path> paths;
    const std::filesystem::path layers = packageDir / "layers";
    if (!std::filesystem::exists(layers))
    {
        return paths;
    }
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(layers))
    {
        if (entry.is_regular_file())
        {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

/// 把一个通道平面折成「该像素是否被打印」的布尔序列。
/// 比的是几何而非取值：光油打印值来自配置、T 恒为 0，值不同不代表几何不同。
std::vector<std::uint8_t> PrintedPlane(
    const std::vector<std::uint8_t>& pixels,
    const std::size_t channelCount,
    const std::size_t channel)
{
    std::vector<std::uint8_t> printed;
    printed.reserve(pixels.size() / channelCount);
    for (std::size_t base{0U}; base + channelCount <= pixels.size();
         base += channelCount)
    {
        printed.push_back(pixels.at(base + channel) != kEmptyValue ? 1U : 0U);
    }
    return printed;
}

}  // namespace

int main()
{
    try
    {
        const std::filesystem::path assets = SourceRoot() / "model" / "obj"
            / "alg_suoguo" / "20260908-HuangChenC";
        if (!std::filesystem::exists(assets))
        {
            std::cout << "MW3-10 SKIP: 资产目录不存在 "
                      << assets.generic_string() << '\n';
            return kSkipReturnCode;
        }
        std::vector<std::filesystem::path> modelPaths;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::directory_iterator(assets))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".obj")
            {
                modelPaths.push_back(entry.path());
            }
        }
        std::sort(modelPaths.begin(), modelPaths.end());
        if (modelPaths.empty())
        {
            std::cout << "MW3-10 SKIP: 资产目录里没有 obj\n";
            return kSkipReturnCode;
        }

        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / "slicesoft_monowrap_plate";
        std::error_code cleanup;
        std::filesystem::remove_all(root, cleanup);

        const std::filesystem::path samples = SourceRoot() / "samples" / "configs";
        // 对照组取 nail_varnish_only：缩裹工艺的 materialProcessProfile 名叫
        // nail_varnish_only_rgbwsvt，二者本就同一血统，一个写 V、一个写 T，
        // 正是「仿照单材料光油」这句话在仓库里的字面对应物。
        //（varnish_only_all_model 虽也是单材料光油，但它没有
        //  materialProcessProfile，resolvedprofileid 为空会被场景校验拒绝。）
        const auto varnishStart = std::chrono::steady_clock::now();
        const PlateRun varnish = RunPlate(
            samples / "material_process" / "nail_varnish_only.json",
            modelPaths, root / "varnish", false);
        const double varnishMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - varnishStart).count();
        if (!varnish.valid)
        {
            std::cerr << "FAILED: 单材料光油整版切片失败：" << varnish.error << '\n';
            return 1;
        }
        const auto wrapStart = std::chrono::steady_clock::now();
        const PlateRun wrap = RunPlate(
            samples / "matvol_t" / "monowrap_whole_model_rgbwsvt.json",
            modelPaths, root / "wrap", true);
        const double wrapMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - wrapStart).count();
        // 耗时只作【报告】不作断言：本机单次波动可达 47%，
        // 把它写成门禁等于把噪声变成红灯。要比就多次取最小值，另行基准。
        std::cout << "TIMING varnishMs=" << static_cast<long long>(varnishMs)
                  << " wrapMs=" << static_cast<long long>(wrapMs)
                  << " ratio=" << (wrapMs / std::max(1.0, varnishMs))
                  << '\n';
        if (!wrap.valid)
        {
            std::cerr << "FAILED: 整模缩裹整版切片失败：" << wrap.error << '\n';
            return 1;
        }
        // 两版都要满足 Worker 的证据判据，否则实机会以 CONTRACT-0060 失败
        // 而本测试仍然全绿——这正是本条断言存在的理由。
        std::string reason;
        if (!HasCompletePackageEvidence(varnish, reason))
        {
            std::cerr << "FAILED: 光油整版产出证据不完整：" << reason << '\n';
            return 1;
        }
        if (!HasCompletePackageEvidence(wrap, reason))
        {
            std::cerr << "FAILED: 缩裹整版产出证据不完整：" << reason << '\n';
            return 1;
        }

        const std::vector<std::filesystem::path> varnishLayers =
            LayerPaths(varnish.packageDir);
        const std::vector<std::filesystem::path> wrapLayers =
            LayerPaths(wrap.packageDir);
        if (varnishLayers.size() != wrapLayers.size() || varnishLayers.empty())
        {
            std::cerr << "FAILED: 两版层数不同（光油 " << varnishLayers.size()
                      << " ／ 缩裹 " << wrapLayers.size() << "）\n";
            return 1;
        }

        std::uint64_t printedPixels{0U};
        for (std::size_t index{0U}; index < varnishLayers.size(); ++index)
        {
            const slicer_core::TiffReadResult v =
                slicer_core::read_rgbwsv_tiff(varnishLayers.at(index));
            const slicer_core::RgbwsvtTiffReadResult t =
                slicer_core::ReadRgbwsvtTiff(wrapLayers.at(index));
            if (v.spec.width != t.spec.width || v.spec.height != t.spec.height)
            {
                std::cerr << "FAILED: 第 " << index << " 层幅面不同\n";
                return 1;
            }
            const std::vector<std::uint8_t> varnishPlane =
                PrintedPlane(v.pixels, kRgbwsvChannels, kVarnishChannel);
            const std::vector<std::uint8_t> transferPlane =
                PrintedPlane(t.pixels, kRgbwsvtChannels, kTransferChannel);
            if (varnishPlane != transferPlane)
            {
                std::size_t mismatches{0U};
                for (std::size_t p{0U}; p < varnishPlane.size(); ++p)
                {
                    mismatches += varnishPlane.at(p) != transferPlane.at(p) ? 1U : 0U;
                }
                std::cerr << "FAILED: 第 " << index << " 层几何不同，"
                          << mismatches << " 个像素不一致\n";
                return 1;
            }
            printedPixels += static_cast<std::uint64_t>(
                std::count(transferPlane.begin(), transferPlane.end(),
                           static_cast<std::uint8_t>(1U)));
            // 整模缩裹只写 T：前六通道在缩裹像素上必须全空。
            for (std::size_t p{0U}; p < transferPlane.size(); ++p)
            {
                if (transferPlane.at(p) == 0U)
                {
                    continue;
                }
                for (std::size_t channel{0U}; channel < kTransferChannel; ++channel)
                {
                    if (t.pixels.at(p * kRgbwsvtChannels + channel) != kEmptyValue)
                    {
                        std::cerr << "FAILED: 第 " << index
                                  << " 层缩裹像素的通道 " << channel << " 非空\n";
                        return 1;
                    }
                }
            }
        }

        std::cout << "GRID varnish=" << varnish.gridWidthPx << "x"
                  << varnish.gridHeightPx << " wrap=" << wrap.gridWidthPx
                  << "x" << wrap.gridHeightPx
                  << " (补白已开)\n";
        std::cout << "PASS: plate_transfer_matches_varnish_geometry 实例 "
                  << kInstanceCount << " 件、层 " << varnishLayers.size()
                  << "、打印像素 " << printedPixels
                  << "，光油 V 与缩裹 T 逐像素几何完全相同且 T 像素前六通道全空\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        // 未捕获异常会让整个二进制 abort、缓冲未刷新连 stdout 都看不到。本仓踩过。
        std::cerr << "FAILED: 抛出异常 " << error.what() << '\n';
        return 1;
    }
}
