#pragma once

#include "slicer_core/geometry/SceneModelTriangleMeshAdapter.h"
#include "slicer_core/geometry/repair/MeshRepairTypes.h"

#include <filesystem>
#include <functional>
#include <string>

namespace slicer_core
{

/**
 * @brief 使用有效 Profile 复核一个暂存修复产物的输入。
 */
struct StrictRepairAssetRecheckRequest
{
    std::filesystem::path stagedObjPath;
    std::filesystem::path profileConfigPath;
    std::string profileHash;
    const AdaptedTriangleMesh* expectedMesh{nullptr};
    std::function<bool()> cancellationRequested;
};

/**
 * @brief 完整的重新导入、属性和严格拓扑证据。
 */
struct StrictRepairAssetRecheckResult
{
    bool assetReimported{false};
    bool strictComplete{false};
    bool strictPass{false};
    bool attributesPreserved{false};
    std::string geometryHash;
    std::string attributeHash;
    MeshRepairDiagnosticsSummary diagnostics;
};

/**
 * @brief 按有效 Profile 资源规则重新导入暂存 OBJ，并运行严格审计。
 * @param request 暂存修复产物、Profile 标识、预期内存候选项和取消状态。
 * @return 完整的重新导入和严格拓扑证据。
 * @throws std::runtime_error 标识过期、导入失败、取消或审计不完整时抛出。
 */
StrictRepairAssetRecheckResult RecheckStrictRepairAsset(
    const StrictRepairAssetRecheckRequest& request);

}  // namespace slicer_core
