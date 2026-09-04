#pragma once

#include "slicer_core/model.h"

#include <span>
#include <string>
#include <vector>

namespace slicer_core
{

/** @brief 由命名规范识别出的素材类别。 */
enum class MaterialLayerClass
{
    /// @brief 常规素材，落 RGB。
    Regular,
    /// @brief 透明素材（`transparent` / `trans`），落 V 光油通道。
    Transparent,
    /// @brief 缩裹 / 弹性材料（`elasticity` / `el`），落 T 通道。
    Elasticity,
};

/** @brief 单个材质名的解析结果。 */
struct MaterialLayerName
{
    std::string material_name;
    /**
     * @brief 名字主体：去掉 `-L<n>` 后缀【与可选的 `-<同层序号>`】之后的部分。
     *
     * 类别判定只看本字段，故序号必须在此剥离——否则 `trans-1` 无法与 `trans`
     * 精确匹配，透明素材会被误判为常规素材。
     */
    std::string base_name;
    /// @brief 图层号，1 为最上层；解析失败时为 0。
    int layer{0};
    /**
     * @brief 同层同类素材的序号；该层同类素材唯一时为 0（名字中省略）。
     *
     * 序号【只用于区分】同层同类的多个素材，**不参与优先级计算**：
     * 它们在工艺上等价，理应共享同一 priority。若两者在空间上真有重叠，
     * 由 MATVOL 自身的同级重叠阻断兜住，而不是靠序号硬分先后。
     */
    int layer_index{0};
    MaterialLayerClass material_class{MaterialLayerClass::Regular};
    /// @brief 成功解出 `-L<n>` 后缀。
    bool parsed{false};
};

/** @brief 全部材质的命名解析与自动优先级。 */
struct MaterialLayerNaming
{
    std::vector<MaterialLayerName> names;
    /// @brief 与 `names` 同序的自动 priority；解析失败的条目为 0。
    std::vector<int> priorities;
    int max_layer{0};
    /// @brief 违反命名规范的材质（缺 `-L<n>` 后缀、层号非法等）。
    std::vector<std::string> violations;
    /// @brief 自动 priority 撞号的说明；非空即不可用于生产。
    std::vector<std::string> collisions;
};

/**
 * @brief 按命名规范解析材质名，并生成 MATVOL 的自动优先级。
 *
 * 规范见 `docs/slice/DOC/DOC_SPEC_MATERIAL_NAMING_多图层素材命名与语义标识规范.md`：
 * 名字形如 `<素材名>-L<图层号>`，L1 为最上层；`transparent`/`trans` 为透明素材，
 * `elasticity`/`el` 为缩裹（弹性）材料，其余为常规素材。匹配大小写不敏感。
 *
 * `L<n>` 是【设计语义】而非物理 Z 位置：`autoOrient` 反转模型后物理上 L2 可能
 * 位于 L1 之上，但编号与优先级都不变。priority 回答的是「设计意图上谁覆盖谁」，
 * 而重叠关系与设计意图都不随朝向改变。因此本函数【只看材质名，不看几何与朝向】——
 * 一旦让它感知 Z，同一份资产在不同摆放下会切出不同产物。
 *
 * 优先级公式为 `(最大层号 + 1 - 本层号) * 100 + 类别次序`，
 * 类别次序取 弹性 30 > 常规 20 > 透明 10。
 *
 * **层序主导，类别只在同层内决定次序**：层步长 100 远大于类别间隔 10，
 * 因此任何上层素材都压过任何下层素材。这是刻意的——L1 是设计上的表面层，
 * 其光油必须覆盖 L2 的彩色；若让类别主导，下层彩色会反过来顶掉上层光油。
 *
 * @param materialInfos 模型的材质表。
 * @return 逐材质解析、自动优先级与违规/撞号诊断。
 */
[[nodiscard]] MaterialLayerNaming ResolveMaterialLayerNaming(
    std::span<const MaterialInfo> materialInfos);

/**
 * @brief 类别的稳定名称，用于报告与错误信息。
 * @param materialClass 素材类别。
 * @return 稳定标识串。
 */
[[nodiscard]] std::string MaterialLayerClassName(MaterialLayerClass materialClass);

}  // namespace slicer_core
