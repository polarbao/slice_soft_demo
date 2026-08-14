#pragma once

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace slicer_core
{

/**
 * @brief 写入确定性作业专属 OBJ 修复产物的输入。
 */
struct DeterministicObjAssetWriteRequest
{
    const AdaptedTriangleMesh* mesh{nullptr};
    std::filesystem::path outputObjPath;
    std::function<bool()> cancellationRequested;
};

/**
 * @brief 确定性 OBJ 写入器产生的文件和属性证据。
 */
struct DeterministicObjAssetWriteResult
{
    std::filesystem::path objPath;
    std::filesystem::path mtlPath;
    std::vector<std::filesystem::path> texturePaths;
    bool uvPreserved{false};
    bool materialsPreserved{false};
    bool textureBytesPreserved{false};
};

/**
 * @brief 将索引修复候选项写为确定性 OBJ/MTL 资源。
 * @param request 候选网格、暂存 OBJ 路径和可选取消回调。
 * @return 暂存修复产物的路径和属性保持证据。
 * @throws std::runtime_error 几何、材质资源或输出路径无效，以及写入失败时抛出。
 */
DeterministicObjAssetWriteResult WriteDeterministicObjAsset(
    const DeterministicObjAssetWriteRequest& request);

}  // namespace slicer_core
