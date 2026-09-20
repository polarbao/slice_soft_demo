#pragma once

// MW3-10 及后续整版缩裹用例共用的场景与工艺搭建。
//
// 取值刻意跟着【用户的实际配置】走：12 个实例（10 个模型轮转取用）；
// X/Y 补齐至 0 都打开——补白在合成前扩展全局网格，整版掩膜与合成层必须
// 落在同一张已补白的画布上；版面推离原点 3mm，否则补白那段对 origin <= 0
// 直接 continue、开关形同虚设。
//
// 网格刻意取粗：整版缩裹的不变量说的是【通道落点】，与分辨率无关，
// 粗网格一样能证且秒级跑完；产线分辨率由字节级基线负责。

#include "slicer_core/config.h"
#include "slicer_core/json_value.h"
#include "slicer_core/model.h"
#include "slicer_core/scene/MultiModelScene.h"
#include "slicer_core/scene/SceneModel.h"
#include "slicer_core/scene/SceneResourceIdentity.h"
#include "slicer_core/system/Sha256.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace monowrap_test
{

constexpr int kInstanceCount{12};
constexpr int kDpi{150};
constexpr double kLayerHeightMm{0.2};
/// 版面推离原点的距离：不推则补白开关形同虚设。
constexpr double kOriginOffsetMm{3.0};

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
    // 用户实际配置里「补齐至 X=0 / Y=0」都开着，本测试跟上——
    // 补白会在合成前扩展全局网格，整版掩膜与合成层必须落在同一张
    // 已补白的画布上。不开就等于这条路径从未被验过。
    output["scenePadToOriginX"] = true;
    output["scenePadToOriginY"] = true;
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
    // 版面被推离原点后，构建体积也要跟着放大，否则实例会越界。
    scene.buildvolume.widthmm = marginMm * 2.0 + kOriginOffsetMm
        + modelWidth * kInstanceCount + gapMm * (kInstanceCount - 1);
    scene.buildvolume.heightmm =
        marginMm * 2.0 + kOriginOffsetMm + modelHeight;
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
        // 故意把版面整体推离原点：补白那段对 origin <= 0 直接 continue，
        // 原点贴着 0 时开关形同虚设，补白路径根本不会被走到。
        item.instance.transform.translatexmm = marginMm + kOriginOffsetMm
            - bound.bbox_mm.min.x
            + static_cast<double>(index) * (modelWidth + gapMm);
        item.instance.transform.translateymm =
            marginMm + kOriginOffsetMm - bound.bbox_mm.min.y;
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
}  // namespace monowrap_test
