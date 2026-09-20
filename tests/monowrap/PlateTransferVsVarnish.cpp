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
#include "slicer_core/TiffReadApi.h"
#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/model.h"
#include "slicer_core/output/rgbwsvt/RgbwsvtTiffIo.h"
#include "slicer_core/pipeline/MultiModelProductionService.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"
#include "slicer_core/scene/SceneModel.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
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
constexpr int kInstanceCount{12};
constexpr int kDpi{150};
constexpr double kLayerHeightMm{0.2};
constexpr std::size_t kRgbwsvChannels{6U};
constexpr std::size_t kRgbwsvtChannels{7U};
constexpr std::size_t kVarnishChannel{5U};
constexpr std::size_t kTransferChannel{6U};
constexpr std::uint8_t kEmptyValue{255U};

std::filesystem::path SourceRoot()
{
#ifdef SLICESOFT_SOURCE_DIR
    return std::filesystem::path{SLICESOFT_SOURCE_DIR};
#else
    return std::filesystem::current_path();
#endif
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        throw std::runtime_error("failed to read " + path.generic_string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

/// 复制基准工艺并只改叶子键：模型路径、DPI、层厚。
/// 整块替换 input / output 会把该工艺其余设置一并抹掉，本仓踩过这个坑。
std::filesystem::path WriteProfileVariant(
    const std::filesystem::path& basePath,
    const std::filesystem::path& destination,
    const std::filesystem::path& modelPath)
{
    std::ifstream source{basePath, std::ios::binary};
    if (!source)
    {
        throw std::runtime_error(
            "failed to read base profile " + basePath.generic_string());
    }
    slicer_core::Json::Object root = slicer_core::Json::parse(source).as_object();
    slicer_core::Json::Object input = root.at("input").as_object();
    input["modelPath"] =
        std::filesystem::absolute(modelPath).lexically_normal().generic_string();
    root["input"] = slicer_core::Json{std::move(input)};
    slicer_core::Json::Object output = root.at("output").as_object();
    // 场景准入要求有效配置与显式工艺的 DPI / 层厚一致，否则 SCENE_PROFILE_MISMATCH。
    output["dpiX"] = static_cast<double>(kDpi);
    output["dpiY"] = static_cast<double>(kDpi);
    output["layerThicknessMm"] = kLayerHeightMm;
    root["output"] = slicer_core::Json{std::move(output)};
    std::filesystem::create_directories(destination.parent_path());
    std::ofstream out{destination, std::ios::binary};
    out << slicer_core::Json{std::move(root)}.dump();
    return destination;
}

slicer_core::MultiModelScene BuildPlateScene(
    const std::vector<std::filesystem::path>& profileConfigPaths)
{
    std::vector<slicer_core::SceneModel> models;
    std::vector<std::filesystem::path> modelPaths;
    slicer_core::SliceConfig firstProfile;
    for (std::size_t index{0U}; index < profileConfigPaths.size(); ++index)
    {
        slicer_core::SliceConfig profile =
            slicer_core::load_slice_config(profileConfigPaths.at(index));
        slicer_core::SceneModel model = slicer_core::load_model_report(
            profile, profileConfigPaths.at(index).parent_path());
        modelPaths.push_back(
            std::filesystem::absolute(model.model_path).lexically_normal());
        if (index == 0U)
        {
            firstProfile = profile;
        }
        models.push_back(std::move(model));
    }

    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-monowrap-plate";
    scene.scenerevision = 1U;
    scene.resolvedprofileid = firstProfile.material_process_profile.name;

    double modelWidth{0.0};
    double modelHeight{0.0};
    for (const slicer_core::SceneModel& item : models)
    {
        modelWidth = std::max(modelWidth, item.bbox_mm.max.x - item.bbox_mm.min.x);
        modelHeight = std::max(modelHeight, item.bbox_mm.max.y - item.bbox_mm.min.y);
    }
    constexpr double marginMm{2.0};
    constexpr double gapMm{2.0};
    // 生产准入要求构建体积来自设备档案且非 fixture，否则
    // BuildVolumeFixtureNotProduction。本测试要的正是生产口径的包。
    scene.buildvolume.source = slicer_core::BuildVolumeSource::DeviceProfile;
    scene.buildvolume.widthmm = marginMm * 2.0 + modelWidth * kInstanceCount
        + gapMm * (kInstanceCount - 1);
    scene.buildvolume.heightmm = marginMm * 2.0 + modelHeight;
    scene.buildvolume.origin = slicer_core::BuildVolumeOrigin::LowerLeft;
    scene.buildvolume.xdirection = slicer_core::BuildVolumeAxisDirection::Positive;
    scene.buildvolume.ydirection = slicer_core::BuildVolumeAxisDirection::Positive;
    scene.buildvolume.isfixture = false;

    for (std::size_t index{0U}; index < modelPaths.size(); ++index)
    {
        slicer_core::ResourceScope scope;
        scope.resourcescopeid = "scope-plate-" + std::to_string(index);
        scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
        scope.rootpath = modelPaths.at(index).parent_path();
        scene.resourcescopes.push_back(scope);

        slicer_core::ModelSource source;
        source.modelid = "model-plate-" + std::to_string(index);
        source.sourcepath = modelPaths.at(index);
        source.format = "obj";
        source.resourcescopeid = scope.resourcescopeid;
        source.sourcehash =
            slicer_core::ComputeSha256(ReadFile(modelPaths.at(index)));
        source.resourcehash =
            slicer_core::ComputeSceneResourceHash(models.at(index));
        source.displayname = "plate-" + std::to_string(index);
        scene.models.push_back(std::move(source));
    }

    for (int index{0}; index < kInstanceCount; ++index)
    {
        // 实例数多于模型数时轮转取用——用户那一版正是 12 件、目录里 10 个模型。
        const std::size_t modelIndex =
            static_cast<std::size_t>(index) % scene.models.size();
        const slicer_core::SceneModel& bound = models.at(modelIndex);
        slicer_core::SceneModelInstance item;
        item.instance.instanceid = "instance-" + std::to_string(index + 1);
        item.instance.modelid = scene.models.at(modelIndex).modelid;
        item.instance.sourcetransformidentity =
            modelPaths.at(modelIndex).generic_string();
        item.instance.sourcebboxmm = bound.bbox_mm;
        item.instance.transform.translatexmm = marginMm - bound.bbox_mm.min.x
            + static_cast<double>(index) * (modelWidth + gapMm);
        item.instance.transform.translateymm = marginMm - bound.bbox_mm.min.y;
        item.instance.effectivebboxmm = bound.bbox_mm;
        item.instance.effectivebboxmm.min.x += item.instance.transform.translatexmm;
        item.instance.effectivebboxmm.max.x += item.instance.transform.translatexmm;
        item.instance.effectivebboxmm.min.y += item.instance.transform.translateymm;
        item.instance.effectivebboxmm.max.y += item.instance.transform.translateymm;
        item.requestedtransform = item.instance.transform;
        item.effectivetransform = item.instance.transform;
        item.admissionstatus = slicer_core::SceneInstanceAdmissionStatus::Admitted;
        item.resolvedprofileid = scene.resolvedprofileid;
        scene.instances.push_back(std::move(item));
    }
    return scene;
}

struct PlateRun
{
    bool valid{false};
    std::string error;
    std::filesystem::path packageDir;
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

    slicer_core::MultiModelProductionRequest request;
    request.effectiveconfigpath = effectiveRequest.generatedconfigpath;
    request.transferchannel = transferChannel;
    const slicer_core::MultiModelProductionResult produced =
        slicer_core::RunMultiModelProductionService(request);
    if (!produced.packagewritten || produced.error.has_value())
    {
        run.error = produced.error.has_value()
            ? produced.error->message
            : "package not written";
        return run;
    }
    run.valid = true;
    run.packageDir = produced.packagedir;
    return run;
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
        const PlateRun varnish = RunPlate(
            samples / "material_process" / "nail_varnish_only.json",
            modelPaths, root / "varnish", false);
        if (!varnish.valid)
        {
            std::cerr << "FAILED: 单材料光油整版切片失败：" << varnish.error << '\n';
            return 1;
        }
        const PlateRun wrap = RunPlate(
            samples / "matvol_t" / "monowrap_whole_model_rgbwsvt.json",
            modelPaths, root / "wrap", true);
        if (!wrap.valid)
        {
            std::cerr << "FAILED: 整模缩裹整版切片失败：" << wrap.error << '\n';
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
