#pragma once

#include "slicer_core/model.h"

#include <filesystem>
#include <string>

namespace slicer_core::model_detail {

/** @brief 解析 MTL 材质声明行所需的路径上下文。 */
struct MtlMaterialContext
{
    std::filesystem::path mtl_dir;
    std::filesystem::path obj_dir;
};

/** @brief 一行 MTL 材质声明的应用结果。 */
struct MtlMaterialLineResult
{
    /// @brief 该行被识别并已写入材质。
    bool applied{false};
    /// @brief 该行声明的不透明度与此前已解析值自相矛盾。
    bool opacity_conflict{false};
};

/**
 * @brief 取 MTL/OBJ 材质名：整行去首尾空白，保留名内空格。
 *
 * OBJ 惯例上不允许材质名含空格，但 Rhino 等导出器会写出 `sg (1)` 这类名字。
 * 只取首个空白分隔令牌会把它截断为 `sg`，使 `sg (1)` 与 `sg (2)` 塌成同一材质，
 * 且与 MeshLab / assimp 取整行的行为不一致。
 *
 * @param arguments 关键字之后的剩余内容。
 * @return 去除首尾空白后的材质名；全为空白时返回空串。
 */
[[nodiscard]] std::string TrimMaterialName(const std::string& arguments);

/**
 * @brief 把一行 MTL 材质声明应用到当前材质。
 *
 * 识别 `Kd`、`map_Kd`、`d` 与 `Tr`。MTL 用两种反向拼写表达同一个不透明度事实：
 * `d` 是 dissolve，1.0 为完全不透明；`Tr` 是 transmission，1.0 为完全透明。
 * 两者统一归一到 `MaterialInfo::opacity`，使下游无需知道导出器选了哪种拼写。
 *
 * 当同一材质先后给出互不相容的两个不透明度（例如同时写 `d 0.0` 与 `Tr 0.0`）时，
 * 不让后出现者静默胜出，而是回报 `opacity_conflict` 由调用方裁决。
 *
 * @param token MTL 关键字。
 * @param arguments 该行关键字之后的剩余内容。
 * @param context 贴图路径解析所需的目录上下文。
 * @param material 待写入的材质；不得为空。
 * @return 应用结果；未识别的关键字回报 `applied = false`。
 */
[[nodiscard]] MtlMaterialLineResult ApplyMtlMaterialLine(
    const std::string& token,
    const std::string& arguments,
    const MtlMaterialContext& context,
    MaterialInfo* material);

}  // namespace slicer_core::model_detail
