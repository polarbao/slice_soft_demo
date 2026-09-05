// MF-05 多实例内存验证台。
//
// 场景路径的驻留量是 O(实例数 x 列数 x 层数)：N 份每实例栅格（10 B/列/层）
// 加一份合成结果（6 B/列/层）。用户 0.2+0.3 @10um 合计约 269.6 GiB，
// 无法直接跑「修好前」的对照。
//
// 故本台的判据不是「跑通某个绝对值」，而是【峰值随层数的斜率】：
//   修好前  峰值随层数线性增长
//   修好后  峰值与层数无关（只与实例数 x 列数有关）
// 这样在 0.4 / 0.2 / 0.1mm 三档就能证明，不必真的准备 269 GiB。
//
// 用法: SceneMemoryBench <layerHeightMm> <instanceCount> [modelPath]

#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/model.h"
#include "slicer_core/pipeline/MultiModelProductionService.h"
#include "slicer_core/scene/SceneEffectiveConfig.h"
#include "slicer_core/scene/SceneModel.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#if defined(_WIN32)
// windows.h 会定义 max/min 宏，与 std::max 冲突。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace
{

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("failed to read " + path.generic_string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

/// 复制黄金 profile，并把 modelPath 指向本次要测的模型。
std::filesystem::path WriteBenchProfile(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath,
    const std::filesystem::path& modelPath,
    const double layerHeightMm,
    const int dpi)
{
    std::ifstream source(sourcePath, std::ios::binary);
    if (!source)
    {
        throw std::runtime_error("failed to read profile: " + sourcePath.generic_string());
    }
    slicer_core::Json::Object root = slicer_core::Json::parse(source).as_object();
    slicer_core::Json::Object input = root.at("input").as_object();
    input["modelPath"] =
        std::filesystem::absolute(modelPath).lexically_normal().generic_string();
    root["input"] = slicer_core::Json{std::move(input)};

    // 场景准入要求 effective config 的 DPI / 层厚与显式 Profile 一致，
    // 故把本次基准的取值一并写回 profile，避免 SCENE_PROFILE_MISMATCH。
    slicer_core::Json::Object output = root.at("output").as_object();
    output["dpiX"] = static_cast<double>(dpi);
    output["dpiY"] = static_cast<double>(dpi);
    output["layerThicknessMm"] = layerHeightMm;
    root["output"] = slicer_core::Json{std::move(output)};

    std::filesystem::create_directories(destinationPath.parent_path());
    std::ofstream out(destinationPath, std::ios::binary);
    out << slicer_core::Json{std::move(root)}.dump();
    return destinationPath;
}

std::uint64_t PeakWorkingSetBytes()
{
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
    {
        return static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
    }
#endif
    return 0U;
}

slicer_core::MultiModelScene BuildScene(
    const std::vector<std::filesystem::path>& profileConfigPaths,
    const int instanceCount)
{
    // 每个 profile 对应一个模型；实例按 instanceCount 轮转取用，故
    // 「一个模型 N 个实例」与「N 个不同模型各一个实例」都能表达。
    std::vector<slicer_core::SliceConfig> profiles;
    std::vector<slicer_core::SceneModel> models;
    std::vector<std::filesystem::path> modelPaths;
    for (const std::filesystem::path& configPath : profileConfigPaths)
    {
        slicer_core::SliceConfig profile =
            slicer_core::load_slice_config(configPath);
        slicer_core::SceneModel model =
            slicer_core::load_model_report(profile, configPath.parent_path());
        modelPaths.push_back(
            std::filesystem::absolute(model.model_path).lexically_normal());
        profiles.push_back(std::move(profile));
        models.push_back(std::move(model));
    }
    const slicer_core::SliceConfig& profile = profiles.front();
    const slicer_core::SceneModel& model = models.front();
    const std::filesystem::path& modelPath = modelPaths.front();

    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-memflow-bench";
    scene.scenerevision = 1U;
    scene.resolvedprofileid = profile.material_process_profile.name;

    // 幅面按【所有模型】的最大尺寸铺开，保证任一实例都放得下。
    double modelWidth{0.0};
    double modelHeight{0.0};
    for (const slicer_core::SceneModel& item : models)
    {
        modelWidth =
            std::max(modelWidth, item.bbox_mm.max.x - item.bbox_mm.min.x);
        modelHeight =
            std::max(modelHeight, item.bbox_mm.max.y - item.bbox_mm.min.y);
    }
    constexpr double marginMm{2.0};
    constexpr double gapMm{2.0};
    scene.buildvolume.source = slicer_core::BuildVolumeSource::Fixture;
    scene.buildvolume.widthmm =
        marginMm * 2.0 + modelWidth * instanceCount + gapMm * (instanceCount - 1);
    scene.buildvolume.heightmm = marginMm * 2.0 + modelHeight;
    scene.buildvolume.origin = slicer_core::BuildVolumeOrigin::LowerLeft;
    scene.buildvolume.xdirection = slicer_core::BuildVolumeAxisDirection::Positive;
    scene.buildvolume.ydirection = slicer_core::BuildVolumeAxisDirection::Positive;
    scene.buildvolume.isfixture = true;

    for (std::size_t index{0}; index < modelPaths.size(); ++index)
    {
        slicer_core::ResourceScope scope;
        scope.resourcescopeid = "scope-bench-" + std::to_string(index);
        scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
        scope.rootpath = modelPaths.at(index).parent_path();
        scene.resourcescopes.push_back(scope);

        slicer_core::ModelSource source;
        source.modelid = "model-bench-" + std::to_string(index);
        source.sourcepath = modelPaths.at(index);
        source.format = "obj";
        source.resourcescopeid = scope.resourcescopeid;
        source.sourcehash =
            slicer_core::ComputeSha256(ReadFile(modelPaths.at(index)));
        source.resourcehash =
            slicer_core::ComputeSceneResourceHash(models.at(index));
        source.displayname = "bench-" + std::to_string(index);
        scene.models.push_back(std::move(source));
    }

    for (int index{0}; index < instanceCount; ++index)
    {
        // 实例轮转取用模型：instanceCount 大于模型数时重复取用。
        const std::size_t modelIndex =
            static_cast<std::size_t>(index) % scene.models.size();
        const slicer_core::SceneModel& boundModel = models.at(modelIndex);
        slicer_core::SceneModelInstance item;
        item.instance.instanceid = "instance-" + std::to_string(index + 1);
        item.instance.modelid = scene.models.at(modelIndex).modelid;
        item.instance.sourcetransformidentity =
            modelPaths.at(modelIndex).generic_string();
        item.instance.sourcebboxmm = boundModel.bbox_mm;
        item.instance.transform.translatexmm =
            marginMm - boundModel.bbox_mm.min.x
            + static_cast<double>(index) * (modelWidth + gapMm);
        item.instance.transform.translateymm =
            marginMm - boundModel.bbox_mm.min.y;
        item.instance.effectivebboxmm = boundModel.bbox_mm;
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

}  // namespace

int main(const int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: SceneMemoryBench <layerHeightMm> <instanceCount> [modelPath]\n";
        return 2;
    }
    try
    {
        const double layerHeightMm = std::stod(argv[1]);
        const int instanceCount = std::stoi(argv[2]);
        const std::filesystem::path sourceRoot{SLICESOFT_SOURCE_DIR};
        std::vector<std::filesystem::path> modelPaths;
        for (int index{3}; index < argc; ++index)
        {
            modelPaths.emplace_back(argv[index]);
        }
        if (modelPaths.empty())
        {
            modelPaths.push_back(
                sourceRoot / "model/obj/reality/finger_suoguo/a-2/0.2.obj");
        }

        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / "slicesoft_memflow_bench";
        std::error_code cleanup;
        std::filesystem::remove_all(root, cleanup);
        std::filesystem::create_directories(root);

        std::vector<std::filesystem::path> profileConfigPaths;
        for (std::size_t index{0}; index < modelPaths.size(); ++index)
        {
            profileConfigPaths.push_back(WriteBenchProfile(
                sourceRoot
                    / "samples/configs/golden/material_process_top2_fixture.json",
                root / ("bench_profile_" + std::to_string(index) + ".json"),
                modelPaths.at(index),
                layerHeightMm,
                600));
        }
        const std::filesystem::path& profileConfigPath =
            profileConfigPaths.front();

        slicer_core::SceneEffectiveConfigRequest effectiveRequest;
        effectiveRequest.scene =
            BuildScene(profileConfigPaths, instanceCount);
        effectiveRequest.sourcescenepath = root / "scene_config.draft.json";
        effectiveRequest.generatedconfigpath = root / "scene_config.effective.json";
        effectiveRequest.sourceprofileid = effectiveRequest.scene.resolvedprofileid;
        effectiveRequest.sourceprofileconfigpath = profileConfigPath;
        effectiveRequest.outputpackagedir = root / "package";
        effectiveRequest.generatedatutc = "2026-09-05T00:00:00.000Z";
        effectiveRequest.dpix = 600;
        effectiveRequest.dpiy = 600;
        effectiveRequest.layerheightmm = layerHeightMm;
        effectiveRequest.slicepipelinemode = "legacy";

        const slicer_core::SceneEffectiveConfigResult effective =
            slicer_core::WriteSceneEffectiveConfig(effectiveRequest);
        if (!effective.IsValid())
        {
            std::cerr << "BENCH_FAIL effective config invalid\n";
            return 1;
        }

        slicer_core::MultiModelProductionRequest request;
        request.effectiveconfigpath = effectiveRequest.generatedconfigpath;
        const auto start = std::chrono::steady_clock::now();
        const slicer_core::MultiModelProductionResult result =
            slicer_core::RunMultiModelProductionService(request);
        const double elapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();

        std::cout << "BENCH layerHeightMm=" << layerHeightMm
                  << " models=" << modelPaths.size()
                  << " instances=" << instanceCount
                  << " valid=" << (result.IsValid() ? 1 : 0)
                  << " elapsedMs=" << elapsedMs
                  << " peakWorkingSetBytes=" << PeakWorkingSetBytes()
                  << "\n";
        if (!result.IsValid() && result.error.has_value())
        {
            std::cerr << "BENCH_ERROR " << result.error->message << "\n";
            return 1;
        }
        return result.IsValid() ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "BENCH_EXCEPTION " << error.what() << "\n";
        return 1;
    }
}
