# DOC_DESIGN_MATOPQ_RGB 多图层贴图取色实施设计

> 文档状态：**ACTIVE / 设计已自审，M1 可进开发**
> 版本：v1.0 ｜ 日期：2026-09-02
> 分支：`MATOPQ-RGB`（分叉自 `codex/matopq-material-opacity` @ `0023dae`）
> 授权：用户 2026-09-02 授权 M1+M2 作为一个专项
> 上游缺口：`TASKS_MATOPQ` §10.5（MO-12）
> 任务清单：`docs/codex_task/current/TASKS_MATOPQ_RGB_多图层贴图取色专项任务清单.md`

---

## 1. 固定边界

| 项 | 约束 |
|---|---|
| 不得改变 | 单材质资产的既有 RGB 取色结果（逐字节零漂移） |
| 不得改变 | MATVOL 的区间求解与 owner 归属（已验证正确，见 §2.4） |
| 不得改变 | MV-05 红线：RGB 合成模块只写 RGB，不触碰 W / S / V，不新增 Z 层 |
| 不得改变 | MV-03 内存边界：禁止 O(材质数 × 层数 × 像素数) 稠密所有权栈 |
| 不得改变 | Stage 14 冻结面（ABI、Profile 哈希、ViewData 契约） |
| 不得夹带 | `main.cpp` / `config.cpp` / `slicer.cpp` 的历史行数问题（用户裁定留待版本重构） |

---

## 2. 问题定位

### 2.1 现象

`tm2-5` / `tm2-4n` 切片成功、层序识别正确、`V+G` 守恒成立，但下层材质的贴图取不到：

```text
按 Y 分段统计 RGB（tm2-5）
  nail-L2 段 (原Y 31.7~41.5)   RGB像素 554,486   唯一值     1 个 → (250,250,250)
  nail-L1 段 (原Y 17.6~30.8)   RGB像素 356,190   唯一值 14,363 个 → 真实贴图
```

### 2.2 资产侧无缺陷

四材质 UV 齐全（`nail-L2` 6,274 面全部有 UV），两张 PNG 均存在，`map_Kd` 均已声明。

### 2.3 根因链

`250` 并非贴图，而是 trans 材质的 Kd：`0.9804 × 255 = 250.002 → 250`。

几何叠放关系（`tm2-5`，原始坐标）：

```text
材质        面顶点数     Zmin     Zmax    Y 范围
nail-L1       17422    7.9312  11.9112   17.60 ~ 30.83
trans-L1      28198    7.3754  12.0260   24.02 ~ 41.57   ← 该列顶面
trans-L2      62146    7.6747  12.0526   17.70 ~ 33.69
nail-L2       21916    7.6916  12.0840   31.72 ~ 41.49   ← 被 trans-L1 遮住
```

`src/slicer_core/slicer.cpp:3299` 的分支链：

```cpp
} else if (config.texture.enabled
           && ShouldApplyTextureToLayer(config, column_ranges, pixel_index, layer_index)) {
    const TextureColumnColor color = resolve_texture_color(config, texture_columns, pixel_index);
    pixels.at(base + 0U) = color.rgb.at(0);          // 逐列顶面色，写进所有层
    ...
} else if (config.material_volume_policy.enabled && material_volume_rgb != nullptr) {
    pixels.at(base + 0U) = material_volume_rgb->at(pixel_index * 3U + 0U);   // 不可达
```

而 `slicer.cpp:2958` 的 `ShouldApplyTextureToLayer`：

```cpp
if (config.texture.apply_mode == "solid_volume_from_top_surface") {
    return true;        // 无条件 true
}
```

生产工艺档位正是 `solid_volume_from_top_surface`，因此**贴图分支无条件抢先，MATVOL 已解算出的 `material_volume_rgb` 被整体丢弃**。

`build_relief_texture_columns()`（`slicer.cpp:2606`）只取全列顶面：

```cpp
const TriangleTextureInfo& texture_info =
    model_report.triangle_textures.at(column.top_triangle_index);   // 只有顶面
const RuntimeMaterialTexture* material = find_runtime_material(runtime, texture_info.material_name);
if (texture_info.has_uv && material != nullptr && material->loaded) { /* 采样 */ }
else { color.rgb = fallback_texture_rgb(config, material); }        // trans-L1 无 map_Kd → Kd 250
```

`material_rgb_for_role()` 的取色优先级同样以 `texture_columns->at(pixel_index)` 为最高，而 `std::vector<TextureColumnColor>` **按 XY 列索引，一列仅一个颜色，无 Z 维度**。

### 2.4 MATVOL 侧已验证正确（不在修复范围）

优先级反转对照实验（`explicit_priority`，L2 组 220/210 > L1 组 120/110）：

```text
自动(L1优先)  R 910,405  G 910,676  V 1,803,775   V+G 2,714,451
反转(L2优先)  R 912,078  G 912,348  V 1,802,103   V+G 2,714,451
差异                +1,673     +1,672     -1,672          ±0
```

反转仅改变 1,672 像素（占 modelPixels 的 0.062%），证明 `closed_intervals` 在每个 XY 列上正确分离了 L1/L2 的 Z 区间，并非 L1 整层压掉 L2；反转有差异亦证明 `nail-L2` / `trans-L1` 均真实参与求解。

**结论：MATVOL 是对的，缺陷只在 RGB 取色。**

### 2.5 第二层缺口：MATVOL 的 RGB 表不支持贴图

`MaterialLayerRgbComposer.h`：

```cpp
[[nodiscard]] std::span<const std::array<std::uint8_t, 3>> RgbByMaterial() const noexcept;
/// @brief 各材质 RGB 的来源，取值为 mtl_kd 或 explicit_fallback
```

每个材质一个固定 RGB，来源只有 `mtl_kd` 或 `explicit_fallback`。这是 MV-05 的设计边界，不是缺陷。

### 2.6 前置条件已核实：UV 已贯通至 MATVOL 输入

```cpp
struct SurfaceTriangleAttributes {          // SceneModelTriangleMeshAdapter.h:17
    std::size_t source_triangle_index{0};
    bool has_uv{false};
    std::array<TexCoord, 3> uv{};
    std::string material_name;
};
struct AdaptedTriangleMesh {
    std::vector<SurfaceTriangleAttributes> triangle_attributes;   // MATVOL 构建输入
};
```

**无需先做 UV 贯通改造。**

---

## 3. 设计更正记录（自审发现，必须保留）

### 3.1 被否决的 M1 首版方案

实施方 2026-09-02 向用户口头描述的 M1 是：

> 「MATVOL 启用且该像素有 owner 时，MATVOL 分支优先于贴图分支。」

**该方案有回归缺陷，已否决。**

反例：单材质资产同时启用 MATVOL 与 texture 时，plan 只有 1 个材质，每列 owner 都是它，顶面材质也是它。此时：

| | 现状 | 首版 M1 |
|---|---|---|
| 单材质 + MATVOL + texture | 贴图采样 | **Kd 单色 ← 回归** |

即：简单交换分支顺序会让**本来有贴图的场景失去贴图**，是净损失，且直接违反 §1 的零漂移边界。

### 3.2 更正后的判据

M1 的正确判据不是「有没有 owner」，而是**「owner 材质是否就是该列顶面材质」**：

```text
owner 材质 == 该列顶面材质  →  用贴图（与现状完全一致，零漂移）
owner 材质 != 该列顶面材质  →  用 MATVOL 的该材质颜色（不再错取顶面色）
```

覆盖情形：

| 场景 | owner vs 顶面 | 取色 | 相对现状 |
|---|---|---|---|
| 单材质 + texture | 相同 | 贴图 | **不变（零漂移）** |
| 多图层的顶层像素 | 相同 | 贴图 | 不变 |
| 多图层的下层像素 | 不同 | 该材质自己的颜色 | **修正（原为顶面色）** |

这条判据使零漂移成为**结构性保证**而非测试后验：owner 与顶面相同时走的是完全相同的代码路径。

---

## 4. M1 设计：取色归属修正

### 4.1 目标

下层像素不再错取上层材质的颜色。M1 阶段该像素取其 owner 材质的 **Kd**（`MaterialRgbTable` 已有），贴图留待 M2。

可观测验收：`tm2-5` 的 `nail-L2` 段由 `(250,250,250)`（trans 的 Kd）变为 `(167,243,255)`（nail 的 Kd，`0.6549×255=167.0`、`0.9529×255=243.0`）。

> 该数值已由零代码探针预先验证：把 `applyMode` 改为 `top_surface_only` 使贴图分支只在顶层命中，
> 下层落入 MATVOL 分支，实测 `nail-L2` 段唯一色即 `(167,243,255)`。
> 故 M1 的目标状态是**已被实测确认可达**的，不是推断。

### 4.2 新增数据结构

每列顶面材质在 plan 材质表中的下标：

```cpp
/// @brief 逐列顶面材质在 MaterialVolumePlan::MaterialNames() 中的下标；
///        kNoMaterialOwner 表示该列无模型或顶面材质不在 plan 材质表内。
std::vector<std::uint32_t> topMaterialIndexByColumn;
```

- 规模：O(列数)，`310×567×4B ≈ 0.7MB`，远低于既有区间量级，不触碰 §1 的 MV-03 禁止项
- 构建时机：`run_slicer` 内、`materialVolumePlan` 建成之后、层循环之前，构建一次
- 构建输入：`ReliefColumnInfo::top_triangle_index` → `model_report.triangle_textures[idx].material_name` → 在 `plan.MaterialNames()` 中查下标

### 4.3 改动点

| 文件 | 改动 |
|---|---|
| `slicer.cpp` `run_slicer` | 构建 `topMaterialIndexByColumn`（一次，非热路径） |
| `slicer.cpp` `compose_layer` 签名 | 增 `const std::vector<std::uint32_t>* top_material_index`、`const std::vector<std::uint32_t>* material_volume_owner` |
| `slicer.cpp:3299` 分支条件 | 贴图分支增加前置条件：owner 未知或 owner == 顶面材质 |

分支改法（保持原有 else-if 链顺序，仅收紧贴图分支的进入条件）：

```cpp
// M1：owner 与该列顶面材质不一致时，说明本像素属于被顶面遮住的下层材质，
// 逐列顶面贴图对它是错误来源（会取到上层材质的色），故让位给 MATVOL 分支。
// owner 未知或与顶面一致时条件恒真，走原路径——单材质场景由此结构性零漂移。
const bool textureColumnMatchesOwner = /* 见 §4.4 */;
} else if (config.texture.enabled
           && textureColumnMatchesOwner
           && ShouldApplyTextureToLayer(config, column_ranges, pixel_index, layer_index)) {
```

### 4.4 判据实现

```cpp
bool TextureColumnMatchesOwner(
    const std::vector<std::uint32_t>* topMaterialIndex,
    const std::vector<std::uint32_t>* owner,
    std::size_t pixelIndex)
{
    // 任一侧信息缺失即视为「一致」，退回既有行为——不引入新的 fail 路径。
    if (topMaterialIndex == nullptr || owner == nullptr
        || pixelIndex >= topMaterialIndex->size() || pixelIndex >= owner->size()) {
        return true;
    }
    const std::uint32_t ownerIndex = owner->at(pixelIndex);
    const std::uint32_t topIndex = topMaterialIndex->at(pixelIndex);
    if (ownerIndex == kNoMaterialOwner || topIndex == kNoMaterialOwner) {
        return true;
    }
    return ownerIndex == topIndex;
}
```

**信息缺失一律返回 true（退回既有行为）**，不新增 fail-closed 路径：本卡是取色修正，不是校验加严，不应改变任何资产的可切性。

### 4.5 M1 不做的事

- 不改 `MaterialRgbTable`（仍是逐材质单色）
- 不改 relief 采样
- 不改 MATVOL 区间求解
- 不新增配置项（判据由 MATVOL 是否启用自然决定）

---

## 5. M2 设计：逐材质贴图采样

### 5.1 目标

`owner != 顶面` 的像素取**其 owner 材质自己的贴图**，而非 Kd。

验收：`tm2-5` 的 `nail-L2` 段唯一色从 1（Kd）变为数千（`13_24_46.png` 的真实采样）。

### 5.2 relief 采样扩展

`ReliefColumnInfo` 现只记全列最高面：

```cpp
struct ReliefColumnInfo {          // slicer.cpp:208
    ...
    int top_triangle_index{-1};
    std::array<double, 3> top_barycentric{0.0, 0.0, 0.0};
};
```

改为附带逐材质顶面。两处填充点（`slicer.cpp:1375` 与 `slicer.cpp:1478`）都在三角形遍历循环内，`triangleIndex` 可用、材质名可由 `triangle_textures[triangleIndex]` 直接查，**同一遍遍历即可完成，无额外几何开销**。

存储采用 CSR 形态（避免逐列 vector 的分配开销）：

```cpp
/// @brief 逐列逐材质顶面。offsets 长度为 列数+1，entries 按列连续存放。
struct PerMaterialTopSurface
{
    std::vector<std::uint32_t> offsets;              // O(列数)
    std::vector<std::uint32_t> materialIndices;      // O(列数 × 该列材质数)
    std::vector<int> triangleIndices;
    std::vector<std::array<double, 3>> barycentrics;
};
```

规模上界：`310×567 列 × 4 材质 × (4B + 4B + 24B) ≈ 20MB`，与既有区间同属 O(列数) 量级；不构成 O(材质数 × 层数 × 像素数) 稠密栈，符合 §1。

### 5.3 逐材质取色

`MaterialLayerRgbComposer` 增加可选的逐列颜色源，保持 MV-05 红线（只写 RGB）：

```cpp
/// @brief 可选逐列逐材质颜色源；为空时退回 RgbByMaterial() 的单色表。
struct MaterialColumnColorSource
{
    /// 返回 false 表示该 (列, 材质) 无贴图可用，调用方回退单色表。
    std::function<bool(std::size_t column, std::uint32_t materialIndex,
                       std::array<std::uint8_t, 3>* rgb)> sample;
};
```

回退顺序（每一级都有明确语义，不静默）：

```text
1. 该 (列, 材质) 有顶面且有 UV 且贴图已加载  → 采样该材质的 map_Kd
2. 该材质有 Kd                                → 用 Kd（等于 M1 行为）
3. MV-05 既有 fallbackPolicy                  → FailClosed 或 explicitFallback
```

### 5.4 UV 越界与缺图的处置（必须显式定义）

MV-05 现有语义是材质缺 Kd 时 `FailClosed`。M2 引入贴图源后新增三种情形，**一律不静默**：

| 情形 | 处置 | 诊断 |
|---|---|---|
| 该材质声明了 `map_Kd` 但文件缺失 | 沿用 `texture.missing_texture_policy`（`warn_and_fallback` / `fail_fast`） | 复用既有 missing_textures 计数 |
| 有贴图但该列该材质无 UV | 回退 Kd | 新增 `per_material_uv_missing_columns` 计数 |
| 有 UV 但采样越界 | 沿用 `texture.uv_address_mode`（clamp / repeat） | 复用既有 `uv_out_of_range` |

### 5.5 `apply_mode` 各档位交互

| `apply_mode` | 与 M1/M2 的关系 |
|---|---|
| `solid_volume_from_top_surface` | 主场景。`ShouldApplyTextureToLayer` 恒真，M1 的 owner 判据成为唯一分流依据 |
| `top_surface_only` | 顶层走贴图分支（owner==顶面，判据恒真），其余层落 MATVOL；M2 后下层亦有各自贴图 |
| `top_surface_band` | 同上，带宽内按 `is_top_material_layer` 判定 |

三档位均不需要额外分支：M1 的判据与 `ShouldApplyTextureToLayer` 是**且**关系，两者独立生效。

---

## 6. 零漂移口径与验收

### 6.1 零漂移（M1 与 M2 都必须满足）

| 场景 | 判据 | 基线 |
|---|---|---|
| 默认路径（MATVOL 关闭） | `compose_layer` 不进入任何新代码 | TIFF 逐字节 `3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f` |
| 单材质 + MATVOL + texture | owner==顶面，判据恒真，走原路径 | 需新建基线（见任务清单 R-02） |
| 既有预设 Profile 哈希 | 本专项不改 ABI、不新增配置项 | Stage 14 门禁 |

### 6.2 功能验收

| 项 | M1 期望 | M2 期望 |
|---|---|---|
| `tm2-5` `nail-L2` 段唯一色 | 1 个 = `(167,243,255)` | 数千个（真实贴图） |
| `tm2-5` `nail-L1` 段唯一色 | 14,363（不变） | 14,363（不变） |
| `V+G` 守恒 | `= 2,714,451` | `= 2,714,451` |
| 层数 | 124 | 124 |
| `tm2-4` | 仍按命名规范拒绝 | 同 |

### 6.3 回归

全量 231 项，失败数必须为继承的 7 项：

```text
 18 - slicer_stage14c04_sync_capability_safety_test
 53 - stage14f03_single_model_s1_gate
 55 - stage14f05_local_closure_gate
149 - scene_layer_adapters_unit_tests
187 - slicer_stage14e02_qt_host_boundary_test
226 - slicer_stage14e04d_dual_view_contract_test
229 - hostflow_hd02_real_asset_matrix
```

---

## 7. 风险登记

| # | 风险 | 缓解 |
|---|---|---|
| R1 | `compose_layer` 参数已很多，再加两个降低可读性 | 本专项只加 2 个指针参数；参数结构化重构属 §1 禁止夹带项 |
| R2 | `slicer.cpp` 属 G2 只减不增名单，M1+M2 为净增 | 沿用已登记的 allowlist 豁免（`642d29e`），不新增豁免项 |
| R3 | M2 的 20MB 在超大幅面下上升 | 规模与列数线性，与既有区间同阶；若超限则按材质数裁剪（只为有 `map_Kd` 的材质建条目） |
| R4 | 逐材质顶面在退化/自交处可能取到非预期三角 | 沿用 MATVOL 既有拓扑分类；本卡不改拓扑判定 |
| R5 | 单材质 MATVOL+texture 无现成基线 | R-02 先建基线再动代码 |

---

## 8. `top_surface_only` 临时规避的处置决定

**决定：只文档化，不新增工艺预设。**

理由：

1. 新增预设需改 `HostProcessPresetCatalog` 并牵动 Profile 哈希面，为一个临时手段付这个代价不成比例
2. M1 交付后该规避即失去意义，预设会沉淀为技术债，且有被误用为长期方案的风险
3. 规避本身有明确代价：顶层以外全部失去贴图（实测 `nail-L1` 段均值由 `[215.2 134.3 121.9]` 漂到 `[171.2 233.4 243.0]`）

紧急需求下的手工用法（不需要任何代码改动）：

```json
"texture": { "applyMode": "top_surface_only", "topSurfaceLayers": 1 }
```

效果：顶层保留贴图，其余层取各自 owner 材质的 Kd。**这也正是 M1 的目标状态的一个特例**，可作为 M1 上线前的过渡。

---

## 9. 自审结论

| 检查项 | 结论 |
|---|---|
| M1 是否会造成回归 | 首版方案会（§3.1），已更正为 owner-vs-顶面判据（§3.2），零漂移成为结构性保证 |
| M1 是否引入新 fail 路径 | 不会。信息缺失一律退回既有行为（§4.4） |
| M1 目标数值是否可达 | 已由零代码探针实测确认 `(167,243,255)`（§4.1） |
| M2 是否触碰 MV-03 内存边界 | 不触碰。O(列数 × 材质数) 与既有区间同阶（§5.2） |
| M2 是否触碰 MV-05 红线 | 不触碰。只写 RGB，颜色源为可选注入（§5.3） |
| UV 是否需先贯通 | 不需要，已核实贯通至 MATVOL 输入（§2.6） |
| `apply_mode` 三档位是否都已定义 | 是（§5.5），且不需额外分支 |
| 缺图/无 UV/越界是否都有显式处置 | 是（§5.4），无静默回退 |
| 是否夹带禁止项 | 无。行数问题沿用已登记豁免，不做参数结构化重构 |

**遗留未定项：无。M1 可进开发。**

---

## 10. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-02 | v1.0 | 首版。含根因链、M1/M2 设计、零漂移口径、风险登记与自审。记录并否决 M1 首版「MATVOL 优先于贴图分支」方案（单材质回归），更正为 owner-vs-顶面判据。`top_surface_only` 定为只文档化不建预设。 |
