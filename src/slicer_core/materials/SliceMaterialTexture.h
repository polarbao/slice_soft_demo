#pragma once

// 贴图取色、材质角色映射与模型填充（F-09 第 5 步从 slicer.cpp 搬出）。
//
// 8 个类型放在 slicer_core 而非 materials 子命名空间：它们出现在公开函数签名里，
// 且在 slicer.cpp 中仍有使用（MaterialRole 6 处、MaterialRoleColumn 7 处、
// TextureColumnColor 11 处等）。留在外层可使 slicer.cpp 侧的类型使用一字不改。
//
// 包含 support/SliceSupportGeneration.h 是为了 ColumnLayerRange——该别名在第 4 步
// 随支撑单元落地，而 build_texture_preview_mask 的签名要用它。方向是 材质 -> 支撑，
// 不成环（支撑头不引用本头）。若日后要理顺，该别名的自然归属其实在本单元。

#include "slicer_core/config.h"
#include "slicer_core/diagnostics/MaterialClosureSemanticDetector.h"
#include "slicer_core/geometry/ReliefColumnInfo.h"
#include "slicer_core/geometry/SliceGridSpec.h"
#include "slicer_core/material/MaterialClosureRepair.h"
#include "slicer_core/material/RetainedMaterialLayerComposer.h"
#include "slicer_core/model.h"
#include "slicer_core/output/reports/SliceReportJson.h"
#include "slicer_core/support/SliceSupportGeneration.h"
#include "slicer_core/texture_image.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace slicer_core {

/**
 * @brief 逐列逐材质的顶面（MATOPQ-RGB M2）。
 *
 * `ReliefColumnInfo` 只记全列最高面，故被顶面遮住的下层材质取不到自己的 UV，
 * 其贴图永远无法采样。本表按 (材质, 列) 记录该材质在该列的最高面，
 * 使每个材质都能用自己的 UV 采自己的 map_Kd。
 *
 * materialIndex 与 `MaterialVolumePlan::MaterialNames()` 同序，因此与 MATVOL 的
 * owner buffer 及 M1 的 topMaterialIndexByColumn 共用同一索引体系，无需桥接。
 *
 * 规模为 O(材质数 x 列数)，**没有层数因子**，故不属于 MV-03 禁止的
 * O(材质数 x 层数 x 像素数) 稠密所有权栈。MATVOL 未启用时本表为空、不产生开销。
 */
struct ReliefPerMaterialTopSurface {
    std::size_t materialCount{0};
    std::size_t columnCount{0};
    /// @brief 该 (材质, 列) 的最高面三角下标；-1 表示该材质未覆盖该列。
    std::vector<int> topTriangle;
    std::vector<std::array<double, 3>> topBarycentric;

    [[nodiscard]] bool Empty() const noexcept {
        return materialCount == 0U || columnCount == 0U;
    }
    [[nodiscard]] std::size_t Index(
        const std::uint32_t materialIndex, const std::size_t column) const noexcept {
        return static_cast<std::size_t>(materialIndex) * columnCount + column;
    }
};

using TextureColumnColor = BoundedTextureColumnFact;

struct MaterialPixel {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t w{255};
    std::uint8_t v{255};
};

enum class ModelFillMaterial
{
    Rgb,
    White,
    Varnish,
    None,
};

using MaterialRole = BoundedMaterialRole;
using MaterialRoleColumn = BoundedMaterialRoleColumnFact;

struct RuntimeMaterialTexture {
    MaterialInfo material;
    TextureImage image;
    bool loaded{false};
};

struct TextureRuntime {
    TextureReportData report;
    std::map<std::string, RuntimeMaterialTexture> materials;
};

namespace materials {

MaterialRoleMappingReportData build_material_role_mapping_report(
    const SliceConfig& config, const ModelReport& model_report);

std::vector<MaterialRoleColumn> build_material_role_columns(
    const SliceConfig& config,
    const ModelReport& model_report,
    const std::vector<ReliefColumnInfo>& columns,
    const std::vector<TextureColumnColor>* texture_columns);

TextureRuntime prepare_texture_runtime(
    const SliceConfig& config, const ModelReport& model_report);

std::vector<TextureColumnColor> build_relief_texture_columns(
    const SliceConfig& config,
    const ModelReport& model_report,
    const std::vector<ReliefColumnInfo>& columns,
    TextureRuntime& runtime);

std::vector<TextureColumnColor> build_per_material_texture_columns(
    const SliceConfig& config,
    const ModelReport& model_report,
    const ReliefPerMaterialTopSurface& perMaterialTop,
    TextureRuntime& runtime);

ModelFillMaterial ResolveProfileDefaultModelFillMaterial(const SliceConfig& config);

std::string ModelFillMaterialToString(const ModelFillMaterial material);

MaterialClosureRepairValues ResolveMaterialClosureRepairValues(const SliceConfig& config);

std::uint8_t ResolveSurfaceVarnishValue(const SliceConfig& config);

std::vector<std::uint8_t> build_texture_preview_mask(
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& model_mask,
    const std::vector<MaterialRoleColumn>* material_role_columns,
    const std::vector<ColumnLayerRange>* column_ranges,
    const int layer_index);

void compose_layer(
    std::vector<std::uint8_t>& pixels,
    const SliceConfig& config,
    const GridSpec& grid,
    const std::vector<std::uint8_t>& model_mask,
    const std::vector<std::uint8_t>& outer_varnish_mask,
    const std::vector<std::uint8_t>& outer_surface_varnish_mask,
    const std::vector<std::uint8_t>& inner_surface_varnish_mask,
    const std::vector<std::uint8_t>& support_mask,
    const std::vector<SupportType>& support_type_map,
    const std::vector<TextureColumnColor>* texture_columns,
    const std::vector<MaterialRoleColumn>* material_role_columns,
    const std::vector<ColumnLayerRange>* column_ranges,
    const std::vector<std::uint8_t>* material_volume_rgb,
    const std::vector<std::uint8_t>* material_volume_varnish_mask,
    // M1：逐列顶面材质下标与本层材质所有者，仅用于判断逐列顶面贴图对本像素
    // 是否为正确来源；两者任一为空即退回既有取色路径。
    const std::vector<std::uint32_t>* top_material_index,
    const std::vector<std::uint32_t>* material_volume_owner,
    // M2：按 (材质, 列) 预采好的各材质自身贴图色；为空则 MATVOL 分支沿用 Kd（M1 行为）。
    const std::vector<TextureColumnColor>* per_material_texture_columns,
    // MF-03X2b 稀疏遍历：非空时只遍历这些列，其余列保持预填的 background.value。
    // 这是【精确等价】而非近似：下方 else 链没有末尾 else，故 model / outer_varnish /
    // support 三者皆零的列在原实现里也不写任何字节。调用方须保证该列表覆盖三者的并集。
    const std::vector<std::uint32_t>* active_columns,
    const int layer_index,
    TextureReportData* texture_report,
    MaterialPolicyReportData* material_policy_report,
    MaterialClosureSemanticLayerInput* materialClosureInput,
    LayerSemanticStats& semantic_stats,
    int& model_pixels,
    int& support_pixels);

}  // namespace materials

}  // namespace slicer_core
