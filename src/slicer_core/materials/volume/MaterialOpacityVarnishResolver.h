#pragma once

#include "slicer_core/config.h"
#include "slicer_core/model.h"

#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace slicer_core
{

/** @brief 一个材质的不透明度判定结果。 */
struct MaterialOpacityDecision
{
    std::string material_name;
    double opacity{1.0};
    /// @brief 源文件显式声明过不透明度；为 false 时按不透明处理且不出诊断。
    bool declared{false};
    /// @brief 判为光油（V 通道体积填充）。
    bool varnish{false};
    /// @brief 落在 (opacity_max, 1) 的半透明材质，按 C4 需出诊断。
    bool semi_transparent{false};
};

/** @brief 全部材质的不透明度判定与诊断。 */
struct MaterialOpacityVarnishResolution
{
    bool enabled{false};
    double opacity_max{0.0};
    std::vector<MaterialOpacityDecision> decisions;
    /// @brief 判为光油的材质名，供逐列 owner 查表。
    std::set<std::string> varnish_materials;
    /// @brief C4 诊断：半透明材质未映射到工艺通道，按 RGB 处理。
    std::vector<std::string> warnings;
};

/**
 * @brief 按不透明度判定哪些材质归属光油（V）通道。
 *
 * 判据只有一条（C1）：`opacity <= opacity_max`。`Tr` 已在 MTL 解析层归一为
 * 同一 opacity，故此处不再区分两种拼写。
 *
 * 落在 `(opacity_max, 1)` 的半透明材质【不是】光油（C4），按 RGB 处理并出诊断；
 * 未声明不透明度的材质按完全不透明处理且不出诊断，以免既有资产产生噪声。
 *
 * @param policy 材质体积策略，其 `opacity_varnish` 决定是否启用与容差取值。
 * @param materialInfos 模型的材质表。
 * @return 逐材质判定、光油材质集合与 C4 诊断。
 */
[[nodiscard]] MaterialOpacityVarnishResolution ResolveMaterialOpacityVarnish(
    const MaterialVolumePolicyConfig& policy,
    std::span<const MaterialInfo> materialInfos);

}  // namespace slicer_core
