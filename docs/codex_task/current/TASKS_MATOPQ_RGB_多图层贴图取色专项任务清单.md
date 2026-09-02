# TASKS_MATOPQ_RGB 多图层贴图取色专项任务清单

> 文档状态：**M1+M2 全部 COMPLETE / MR-07 包裹型命名语义待用户裁定**
> 版本：v1.1 ｜ 日期：2026-09-02
> 定位：不占 Stage 编号的独立取色修正专项；**本清单是任务状态的唯一真源**
> 分支：`MATOPQ-RGB`（分叉自 `codex/matopq-material-opacity` @ `0023dae`）
> 设计：`docs/slice/DOC/DOC_DESIGN_MATOPQ_RGB_多图层贴图取色实施设计.md`
> 上游缺口：`TASKS_MATOPQ` §10.5（MO-12）
> 授权：用户 2026-09-02 授权 M1+M2 作为一个专项

---

## 1. 固定边界

见设计文 §1。要点复述：

- 单材质资产的 RGB 取色**逐字节零漂移**
- 不改 MATVOL 区间求解与 owner 归属（已验证正确）
- 不碰 MV-05 红线（只写 RGB）与 MV-03 内存边界（禁稠密所有权栈）
- 不夹带 `main.cpp` / `config.cpp` / `slicer.cpp` 的历史行数问题

---

## 2. 状态表

| 卡号 | 任务 | 状态 | 依赖 | 完成日期 |
|---|---|---|---|---|
| MR-00 | 根因定位、实施设计与自审 | **COMPLETE** | 用户授权 | 2026-09-02 |
| R-01 | 默认路径零漂移基线复核 | **COMPLETE** | MR-00 | 2026-09-02 |
| R-02 | `owner == 顶面` 零漂移基线 | **COMPLETE** | MR-00 | 2026-09-02 |
| MR-01 | M1：owner-vs-顶面判据，取色归属修正 | **COMPLETE** | R-01、R-02 | 2026-09-02 |
| MR-02 | M1 验收：`nail-L2` 段变 `(167,243,255)` + 全量回归 | **COMPLETE** | MR-01 | 2026-09-02 |
| MR-03 | M2-a：relief 采样记录逐材质顶面（稠密表，见 §8） | **COMPLETE** | MR-02 | 2026-09-02 |
| MR-04 | M2-b：按 owner 采样各材质自身 `map_Kd` | **COMPLETE** | MR-03 | 2026-09-02 |
| MR-05 | M2 验收：两层贴图均正确 + 全量回归 | **COMPLETE** | MR-04 | 2026-09-02 |
| MR-06 | 文档收口与 MO-12 关闭 | **COMPLETE** | MR-05 | 2026-09-02 |
| MR-07 | 包裹型透明介质的命名语义（`yz/bg-test01`） | **BLOCKED / 待用户裁定** | MR-05 | — |

---

## 3. MR-00 根因定位与设计（COMPLETE，2026-09-02）

### 3.1 产出

- 设计文 `DOC_DESIGN_MATOPQ_RGB`（含根因链、M1/M2 设计、零漂移口径、风险登记、自审）
- 本清单

### 3.2 关键结论

| 结论 | 依据 |
|---|---|
| MATVOL 侧正确，缺陷只在 RGB 取色 | 优先级反转实验仅改变 1,672 像素（0.062%），证明区间已正确分离 |
| `250` 是 trans 的 Kd 而非贴图 | `0.9804 × 255 = 250.002 → 250`，逐位吻合 |
| MATVOL 的 RGB 分支是死代码 | `ShouldApplyTextureToLayer` 对 `solid_volume_from_top_surface` 恒返回 true |
| UV 无需先贯通 | `SurfaceTriangleAttributes` 已带 `has_uv` + `uv[3]`，且是 MATVOL 构建输入 |
| M1 目标数值可达 | 零代码探针（`applyMode: top_surface_only`）实测 `nail-L2` 段 = `(167,243,255)` |

### 3.3 设计自审否决项（必须保留）

M1 首版方案「MATVOL 有 owner 时优先于贴图分支」**已否决**：单材质资产同时启用 MATVOL 与 texture 时，owner 与顶面同为该唯一材质，简单换分支会让本有贴图的场景退化为 Kd 单色，构成回归。

更正为 **owner-vs-顶面判据**：owner == 顶面时走原路径（结构性零漂移），owner != 顶面时才让位给 MATVOL。详见设计文 §3。

---

## 4. R-01 默认路径零漂移（COMPLETE，2026-09-02）

**目的：** 确认 M1 未影响 MATVOL 关闭时的默认路径。

**方法：** 资产 `fenandtou_d0_clean`（MATVOL 与 texture 均未启用），Release 二进制，94 层 TIFF 拼接 sha256。

**实测（M1 落地后）：**

```text
层数 = 94
M1 后 = 3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f
基线  = 3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f
结果  = 逐字节一致
```

**结论：** MATVOL 关闭时 `topMaterialIndexByColumn` 为空，判据恒真，`compose_layer` 走与修订前完全相同的路径——设计文 §4.4 的结构性零漂移得到实测印证。

---

## 5. R-02 `owner == 顶面` 零漂移基线（COMPLETE，2026-09-02）

**目的：** 这是 M1 唯一的回归风险面（设计文 §3.1 的否决方案正是在此处出问题）。

### 5.1 原计划及其否决

原计划用「单材质 + MATVOL + texture」资产建基线。**该计划不可执行**，原因有二：

1. 仓库内**没有单材质含贴图资产**。实测各资产材质数与贴图：

```text
含 map_Kd 的资产    tm2 / tm2-1..tm2-5 / tm2-4n / spec-l   均为 4 材质
无 map_Kd 的资产    fenandtou(5) / s1-probe(4) / tm3(4) / tm4(2) / fenandtou_d0_clean(2)
```

2. 无法从 `tm2-5` 构造。把四个 `usemtl` 全指向同一材质（几何逐行不动）后切片直接被拒：

```text
E_MATVOL_TOPOLOGY_INVALID: material 'nail-L1' topology is non_manifold
```

**几何事实（本卡产出）：** `tm2-5` 的四个材质**单独看都不是封闭体**，它们靠彼此的共享界面共同构成封闭。合并材质后原本的材质界面退化为同材质内的非流形面；反之单独提取一个材质则界面退化为真开边。这与 `MaterialVolumeBuildRequest::materialNameFilter` 的注释一致——拓扑分类必须针对完整网格进行，材质界面边才不会被误判为真开边。

### 5.2 更正后的判据

否决方案的失败机理是「owner 存在 → 走 MATVOL 分支 → 失去贴图」。该机理**不要求单材质**，任何 `owner == 顶面` 的像素都会以同样方式失效。

`tm2-5` 的 `nail-L1` 段正是 `owner == 顶面` 的真实样本（该段顶面即 `nail-L1`，owner 亦为它），故已有基线即可承担 R-02 的职责，且比构造资产更贴近生产形态。

### 5.3 基线（Release，2026-09-02 实测）

```text
tm2-5  层数=124  尺寸=310x567x6
  R = 910,405   G = 910,676   B = 910,676
  W = 0         S = 9,593,222  V = 1,803,775
  V+G = 2,714,451

按 Y 分段
  nail-L1 段 (原Y 17.6~30.8)   RGB像素 356,190   唯一色 14,363   均值 [215.2 134.3 121.9]
  nail-L2 段 (原Y 31.7~41.5)   RGB像素 554,486   唯一色      1   均值 [250.0 250.0 250.0]
```

**M1 判据：** `nail-L1` 段的「RGB像素 356,190 / 唯一色 14,363 / 均值 [215.2 134.3 121.9]」必须**逐项不变**——该段若发生任何变化，即证明 M1 误伤了 `owner == 顶面` 的路径。

---

## 6. MR-01 M1 实施（COMPLETE，2026-09-02）

### 6.1 改动清单

| 文件 | 改动 | 预估 |
|---|---|---|
| `slicer.cpp` `run_slicer` | 构建 `topMaterialIndexByColumn`（O(列数)，一次性，非热路径） | ~25 行 |
| `slicer.cpp` `compose_layer` 签名 | 增 `top_material_index` 与 `material_volume_owner` 两个指针参数 | ~4 行 |
| `slicer.cpp` 判据函数 | 新增 `TextureColumnMatchesOwner()` | ~18 行 |
| `slicer.cpp:3299` | 贴图分支增加 `textureColumnMatchesOwner` 前置条件 | ~3 行 |

### 6.2 实现纪律

1. **信息缺失一律返回 true**（退回既有行为）。本卡是取色修正，不是校验加严，不得新增 fail 路径、不得改变任何资产的可切性。
2. **不改** `MaterialRgbTable`、relief 采样、MATVOL 区间求解。
3. **不新增配置项**。分流由 MATVOL 是否启用自然决定。
4. `materialVolumeOwner` 是层循环内复用的 buffer，传入 `compose_layer` 时须确认其生命周期覆盖调用点（现有代码在同一层迭代内，安全）。

### 6.3 待开发时确认（非阻塞）

- `compose_layer` 现有参数中是否已有可复用的 owner 通道，避免重复传参
- `topMaterialIndexByColumn` 的构建位置需在 `materialVolumePlan` 建成之后（需 `MaterialNames()` 查下标）

---

## 7. MR-02 M1 验收（COMPLETE，2026-09-02）

### 7.1 实测结果

| 项 | 期望 | 实测 | |
|---|---|---|---|
| `tm2-5` `nail-L2` 段唯一色 | 1 个 = `(167,243,255)` | 1 个 = `(167,243,255)` | 通过 |
| `tm2-5` `nail-L1` 段唯一色 | 14,363（不变） | 356,190 像素 / 14,363 色 / 均值 `[215.2 134.3 121.9]` | 通过 |
| `V+G` | `= 2,714,451` | `2,714,451` | 通过 |
| 层数 | 124 | 124 | 通过 |
| R-01 基线 | 逐字节不变 | `3cbfdec2…f363b5f` 一致 | 通过 |
| R-02 基线 | 逐项不变 | `nail-L1` 段三项全同 | 通过 |
| 全量回归 | 231 项 / 失败 7 项 | 231 项 / 失败 7 项，逐项与继承基线相同 | 通过 |

回归明细（`MATOPQ-RGB` @ `3224978`，Debug，2026-09-02 19:03）：

```text
BUILD=0   CTEST=8
97% tests passed, 7 tests failed out of 231
Total Test time (real) = 1215.68 sec

失败 7 项（继承，非本卡引入）
 18 slicer_stage14c04_sync_capability_safety_test    149 scene_layer_adapters_unit_tests
 53 stage14f03_single_model_s1_gate                  187 slicer_stage14e02_qt_host_boundary_test
 55 stage14f05_local_closure_gate                    226 slicer_stage14e04d_dual_view_contract_test
229 hostflow_hd02_real_asset_matrix
```

### 7.2 已知中间态局限（非本卡缺陷）

B 通道非空像素数由 `910,676` 降为 `356,181`，差值 `554,495` 与 `nail-L2` 段像素数 `554,486` 相当。

根因：`nail` 的 Kd 蓝分量为 `1.0000`，`1.0 × 255 = 255`，而 **255 正是 `emptyValue`**。在 `black_is_print` 语义下 255 既表示「空」又表示「最大值」，故该段 B 通道在统计上被计为空。R（167）与 G（243）不撞值，两者均未变。

这是 **M1 中间态（Kd 单色）的固有性质**：只要材质 Kd 含 1.0 分量就会撞值。M2 改用真实贴图采样后，采样值不再是常量 255，此现象自然消失。**不作为 M1 的缺陷处理，也不为它引入特例逻辑**——引入特例会破坏 §1 的零漂移边界。

### 7.3 过程失误记录

M1 第一轮回归被实施方自己污染：回归跑动期间切换分支去执行 MATOPQ 合并，导致 `source_size_guard_self_test`（读工作树源文件实际行数）与 contract 测试读到不含 M1 的分支内容。该轮结果作废并重跑。**教训：回归跑动期间不得切换分支或改动工作树。**

### 7.4 统计工具

`chan_stats.py` / `layer_profile.py` / `mtl_zrange.py` 目前在会话 scratchpad。M2 验收仍需它们；若要长期保留应移入 `scripts/`（本卡未做，避免在验收卡里夹带非验收改动）。

---

## 8. MR-03 / MR-04 M2 实施（COMPLETE，2026-09-02）

见设计文 §5。要点：

### 8.1 MR-03 存储形态修正：CSR 改为稠密表

设计文 §5.2 原定 CSR（`offsets` / `materialIndices` / …）。落到代码后改为**稠密表**：

```cpp
// 索引 = materialIndex * columnCount + pixelIndex
std::vector<double> zMaxByMaterial;                    // 材质数 x 列数
std::vector<int> topTriangleByMaterial;
std::vector<std::array<double, 3>> topBaryByMaterial;
```

理由：

1. 规模为 `O(材质数 × 列数)`，**没有层数因子**。MV-03 禁止的是 `O(材质数 × 层数 × 像素数)` 稠密所有权栈，本表不在其列。
2. `310×567 列 × 4 材质 × (8+4+24)B ≈ 25MB`，与设计文估算的 20MB 同量级。
3. CSR 省下的内存换不回它引入的转换步骤与索引复杂度；采样热路径需要 O(1) 随机写，稠密直接满足。

该修正需同步回设计文 §5.2。

### 8.2 填充点

两处均在三角形遍历循环内、`triangleIndex` 可用，且 `sample_relief_heightfield_masks` 已有 `model_report` 入参（`slicer.cpp:1299`），故 `triangleIndex → material_name` 可直接查 `model_report.triangle_textures`：

```text
slicer.cpp:1375   if (zMm >= zMax[pixelIndex])              { 全列顶面 }   标准档
slicer.cpp:1478   if (zMm >= representativeTopZ[pixelIndex]) { 同上 }      超采样档
```

在同一个 `if` 旁并列一份逐材质判定即可，**同一遍遍历完成，无额外几何开销**。需预先建 `triangleIndex → materialIndex` 映射（O(三角数)，一次）。
- **MR-04**：`MaterialLayerRgbComposer` 增可选 `MaterialColumnColorSource`，回退顺序为「该材质贴图 → 该材质 Kd → MV-05 既有 fallbackPolicy」。缺图 / 无 UV / 越界三种情形的处置见设计文 §5.4，一律不静默。

---

## 9. MR-05 M2 验收（COMPLETE，2026-09-02）

### 9.1 三态对照（Release，tm2-5）

```text
                 nail-L2 段（下层贴图区）                nail-L1 段（上层贴图区）
基线(改前)   唯一色      1  = (250,250,250)          356,190 像素 / 14,363 色 / [215.2 134.3 121.9]
             取到 trans-L1 的 Kd —— 跨材质错取
M1           唯一色      1  = (167,243,255)          356,190 像素 / 14,363 色 / [215.2 134.3 121.9]
             归属修正为 nail 自身 Kd，仍为单色
M2           唯一色 22,882  均值 [213.3 124.7 112.6]  356,190 像素 / 14,363 色 / [215.2 134.3 121.9]
             13_24_46.png 的真实采样
```

| 项 | 期望 | 实测 | |
|---|---|---|---|
| `nail-L2` 段唯一色 | 数千（真实采样） | **22,882** | 通过 |
| `nail-L1` 段 | 14,363 不变 | 三态逐项相同 | 通过 |
| `V+G` | `2,714,451` | 三态守恒 | 通过 |
| 层数 | 124 | 124 | 通过 |
| R-01 默认路径 | 逐字节不变 | 94 层拼接 `3cbfdec2…f363b5f` 一致 | 通过 |
| R-02 `owner==顶面` | 逐项不变 | `nail-L1` 段三项全同 | 通过 |
| 全量回归 | 231 / 失败 7 | 231 / 失败 7，逐项同继承基线 | 通过 |

**两图层色数不同（22,882 vs 14,363）即两张贴图各自被采样的证据** —— 若两段取自同一张图，色数与均值应当接近。

回归明细（`MATOPQ-RGB` @ `4ab3d47`，Debug，2026-09-02 19:46）：

```text
BUILD=0   CTEST=8
97% tests passed, 7 tests failed out of 231
Total Test time (real) = 1229.41 sec
失败 7 项与继承基线逐项相同（18/53/55/149/187/226/229）
```

### 9.2 M1 中间态的 B 通道撞值现象已消失

MR-02 §7.2 记录的现象（`nail` 的 Kd 蓝分量 1.0 折算 255 与 `emptyValue` 撞值，B 通道统计由 910,676 降为 356,181）在 M2 后不再出现：真实贴图的采样值不是常量 255。这印证了当时「不为其引入特例逻辑」的判断是对的——该现象是 Kd 单色中间态的性质，不是需要修的缺陷。

## 9.5 MR-06 文档收口（COMPLETE，2026-09-02）

- 本清单 MR-00..06 收口
- 设计文 §5.2 存储形态由 CSR 修正为稠密表（`35fdb7c`）
- `TASKS_MATOPQ` §10.5（MO-12）状态由 BLOCKED 改为 RESOLVED，指向本专项

## 9.6 MR-07 包裹型透明介质的命名语义（BLOCKED / 待用户裁定）

用户 2026-09-02 提供 `model/obj/multi-material/yz/bg-test01`，并指示暂缓处理。该资产暴露命名规范的一个真实缺口。

**几何事实（实测）：**

```text
材质        面顶点数      Z 范围            XY 范围
trans         53396   -9.0000 ~ 9.0000   [-8.00,-10.00] ~ [10.00, 8.00]   外层透明块
sjx1-L1       19400   -4.1481 ~ 4.5000   [-4.76,-4.41] ~ [ 6.76, 5.62]   完全被包在内部
```

`sjx1-L1` 的 Z 与 XY 都真包含于 `trans` 之内——这是**内嵌**，而非 tm2-5 那种并列分区。

**两个待裁定问题：**

1. `trans` 缺 `-L<n>` 后缀，当前 `auto_by_material_name` 会直接 fail-closed（`E_MATOPQ_LAYER_NAME_INVALID`）。用户已确认「单图层模型也要带 -L1」，故规范要求所有材质带后缀。
2. **更根本：`-L<n>` 的「上下层」语义对内嵌关系不成立。** `trans` 同时在 `sjx1` 的上方、下方与四周，不存在「谁在谁之上」。`trans-L1 + sjx1-L2` 描述不了该关系；`trans-L2 + sjx1-L1` 会让内嵌物体优先级更高（可能反而符合工艺意图，但无依据）。

**实施方不得从几何或工艺名推断该语义**——见 `DOC_DECISION_MATOPQ` §8.3 的实施纪律。

**技术前瞻（非裁定）：** M2 的取色机制不依赖上下关系，只按 owner 取色，故内嵌场景在**命名问题解决后**应当可直接工作，无需额外开发。但这需要实测确认，不作为结论。

---

## 10. 资产

```text
model/obj/multi-material/tm2-5/     四材质 nail-L1/trans-L1/nail-L2/trans-L2，主验证资产
model/obj/multi-material/tm2-4n/    与 tm2-5 几何相同（v=31349 f=37401），对照用
model/obj/multi-material/tm2-4/     旧命名反例，应被 E_MATOPQ_LAYER_NAME_INVALID 拒绝
model/obj/multi-material/tm3/       单材质含贴图，R-02 基线候选
```

`tm2-5` 的材质空间分布（原始坐标，`mtl_zrange.py` 实测）：

```text
材质        面顶点数     Zmin     Zmax    Y 范围
nail-L1       17422    7.9312  11.9112   17.60 ~ 30.83
trans-L1      28198    7.3754  12.0260   24.02 ~ 41.57   ← 顶面
trans-L2      62146    7.6747  12.0526   17.70 ~ 33.69
nail-L2       21916    7.6916  12.0840   31.72 ~ 41.49   ← 被 trans-L1 遮住
```

`autoOrient` 实际生效（`identity_rotate_y_180_rotate_z_180`），设计上最上层的 L1 在切片坐标系被翻到下方 —— 层序解析只看材质名不看朝向，priority 不随翻转改变。

---

## 11. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-09-02 | v1.1 | M1+M2 收口：MR-01..06 全部 COMPLETE。nail-L2 段唯一色 1 -> 22,882（真实贴图采样），nail-L1 段三态逐项不变，V+G 三态守恒，R-01 默认路径 94 层哈希逐字节不变，全量回归 231 项失败 7 项与继承基线相同。MR-03 存储形态由 CSR 改为稠密表。新增 MR-07 记录包裹型透明介质命名语义缺口（yz/bg-test01），待用户裁定。上游 MO-12 已置 RESOLVED。 |
| 2026-09-02 | v1.0 | 首版。建立 MR-00..06 与 R-01/R-02 两项准备卡；MR-00 完成（含设计自审与 M1 首版方案否决记录）；MR-01 标 READY。 |
