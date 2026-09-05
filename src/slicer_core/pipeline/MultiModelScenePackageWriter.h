#pragma once

#include "slicer_core/layout/SceneCollisionService.h"
#include "slicer_core/output/rgbwsv/RgbwsvPackageWriter.h"
#include "slicer_core/pipeline/SceneLayerComposer.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <filesystem>
#include <vector>

namespace slicer_core
{

/**
 * @brief Publish one admitted composed scene through the shared RGBWSV writer.
 * @param request Package path, storage, preview, DPI, and pipeline metadata.
 * @param composition Complete 13B-05 writer-ready scene layers.
 * @param scene Canonical model and instance scene document.
 * @param admission Passing 13B-04 collision and build-volume evidence.
 * @param instanceRasters Authoritative per-instance production bytes.
 * @param profileConfigPath Effective Profile identity source.
 * @return Atomic package publication summary.
 * @throws std::invalid_argument when scene evidence is invalid or stale.
 */
/**
 * @brief 按合成结果补齐写请求（栅格、场景报告、能力摘要），不写任何字节。
 *
 * MF-05 步骤 4 第二步需要「先建发布会话逐层写层、最后补齐再发布」，
 * 故把补齐从写出里分离出来。`WriteMultiModelSceneProductionPackage`
 * 现在是「补齐 + 整栈写出」的组合，语义不变。
 */
void PrepareMultiModelScenePackageRequest(
    RgbwsvProductionPackageWriteRequest& request,
    const SceneLayerComposeResult& composition,
    const MultiModelScene& scene,
    const SceneCollisionResult& admission,
    const std::vector<SceneInstanceRaster>& instanceRasters,
    const std::filesystem::path& profileConfigPath);

RgbwsvProductionPackageWriteResult
WriteMultiModelSceneProductionPackage(
    RgbwsvProductionPackageWriteRequest request,
    SceneLayerComposeResult composition,
    const MultiModelScene& scene,
    const SceneCollisionResult& admission,
    const std::vector<SceneInstanceRaster>& instanceRasters = {},
    const std::filesystem::path& profileConfigPath = {});

/**
 * @brief Publish composer-validated scene evidence without redundant byte scans.
 *
 * The persisted staging package is still independently decoded and strictly
 * validated before publication.
 */
RgbwsvProductionPackageWriteResult
WriteValidatedMultiModelSceneProductionPackage(
    RgbwsvProductionPackageWriteRequest request,
    ValidatedSceneLayerComposeResult composition,
    const MultiModelScene& scene,
    const SceneCollisionResult& admission,
    const std::filesystem::path& profileConfigPath = {});

}  // namespace slicer_core
