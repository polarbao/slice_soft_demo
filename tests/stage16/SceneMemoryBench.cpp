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

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#if defined(_WIN32)
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
    const std::filesystem::path& profileConfigPath,
    const int instanceCount)
{
    const slicer_core::SliceConfig profile =
        slicer_core::load_slice_config(profileConfigPath);
    const slicer_core::SceneModel model =
        slicer_core::load_model_report(profile, profileConfigPath.parent_path());
    const std::filesystem::path modelPath =
        std::filesystem::absolute(model.model_path).lexically_normal();

    slicer_core::MultiModelScene scene;
    scene.sceneid = "scene-memflow-bench";
    scene.scenerevision = 1U;
    scene.resolvedprofileid = profile.material_process_profile.name;

    const double modelWidth = model.bbox_mm.max.x - model.bbox_mm.min.x;
    const double modelHeight = model.bbox_mm.max.y - model.bbox_mm.min.y;
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

    slicer_core::ResourceScope scope;
    scope.resourcescopeid = "scope-bench";
    scope.kind = slicer_core::ResourceScopeKind::ObjDirectory;
    scope.rootpath = modelPath.parent_path();
    scene.resourcescopes.push_back(scope);

    slicer_core::ModelSource source;
    source.modelid = "model-bench";
    source.sourcepath = modelPath;
    source.format = "obj";
    source.resourcescopeid = scope.resourcescopeid;
    source.sourcehash = slicer_core::ComputeSha256(ReadFile(modelPath));
    source.resourcehash = slicer_core::ComputeSceneResourceHash(model);
    source.displayname = "bench";
    scene.models.push_back(source);

    for (int index{0}; index < instanceCount; ++index)
    {
        slicer_core::SceneModelInstance item;
        item.instance.instanceid = "instance-" + std::to_string(index + 1);
        item.instance.modelid = source.modelid;
        item.instance.sourcetransformidentity = modelPath.generic_string();
        item.instance.sourcebboxmm = model.bbox_mm;
        item.instance.transform.translatexmm =
            marginMm - model.bbox_mm.min.x
            + static_cast<double>(index) * (modelWidth + gapMm);
        item.instance.transform.translateymm = marginMm - model.bbox_mm.min.y;
        item.instance.effectivebboxmm = model.bbox_mm;
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
        const std::filesystem::path modelPath = argc >= 4
            ? std::filesystem::path(argv[3])
            : sourceRoot / "model/obj/reality/finger_suoguo/a-2/0.2.obj";

        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / "slicesoft_memflow_bench";
        std::error_code cleanup;
        std::filesystem::remove_all(root, cleanup);
        std::filesystem::create_directories(root);

        const std::filesystem::path profileConfigPath = WriteBenchProfile(
            sourceRoot / "samples/configs/golden/material_process_top2_fixture.json",
            root / "bench_profile.json",
            modelPath,
            layerHeightMm,
            600);

        slicer_core::SceneEffectiveConfigRequest effectiveRequest;
        effectiveRequest.scene = BuildScene(profileConfigPath, instanceCount);
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
