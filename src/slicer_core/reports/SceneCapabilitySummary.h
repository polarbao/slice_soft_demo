#pragma once

#include "slicer_core/json_value.h"
#include "slicer_core/pipeline/SceneRasterTypes.h"
#include "slicer_core/scene/MultiModelScene.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace slicer_core
{

/** @brief 随场景生产包持久化的 capability-v1.2 元数据。 */
struct SceneCapabilitySummaryDocument
{
    Json perinstance;
    Json profileecho;

    /**
     * @brief 验证生产包摘要扩展的结构。
     * @return 逐实例证据和 Profile 证据完整时返回 true。
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief 根据实例栅格构造权威生产包摘要证据。
 * @param scene 包含实例变换的已提交场景。
 * @param rasters 可直接写入的逐实例生产栅格。
 * @param profileDocument 携带版本和哈希的有效 Profile JSON。
 * @return 摘要证据；无标识的旧版 Profile 返回空值。
 * @throws std::invalid_argument 场景与栅格证据不一致时抛出。
 */
std::optional<SceneCapabilitySummaryDocument> BuildSceneCapabilitySummary(
    const MultiModelScene& scene,
    const std::vector<SceneInstanceRaster>& rasters,
    const Json& profileDocument);

/**
 * @brief 根据一个有效 Profile 文件构造生产包摘要证据。
 * @param scene 包含实例变换的已提交场景。
 * @param rasters 可直接写入的逐实例生产栅格。
 * @param profileConfigPath 有效 Profile JSON 路径。
 * @return 摘要证据；旧版路径或 Profile 返回空值。
 * @throws std::invalid_argument 文件或证据不一致时抛出。
 */
std::optional<SceneCapabilitySummaryDocument> BuildSceneCapabilitySummary(
    const MultiModelScene& scene,
    const std::vector<SceneInstanceRaster>& rasters,
    const std::filesystem::path& profileConfigPath);

}  // namespace slicer_core
