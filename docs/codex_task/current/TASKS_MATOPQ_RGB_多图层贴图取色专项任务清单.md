# TASKS_MATOPQ_RGB 多图层贴图取色专项任务清单

> 文档状态：**ACTIVE / 准备工作完成，MR-01 可进开发**
> 版本：v1.0 ｜ 日期：2026-09-02
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
| R-01 | 默认路径零漂移基线复核 | **TODO** | MR-00 | — |
| R-02 | 单材质 + MATVOL + texture 基线建立 | **TODO** | MR-00 | — |
| MR-01 | M1：owner-vs-顶面判据，取色归属修正 | **READY** | R-01、R-02 | — |
| MR-02 | M1 验收：`nail-L2` 段变 `(167,243,255)` + 全量回归 | **TODO** | MR-01 | — |
| MR-03 | M2-a：relief 采样记录逐材质顶面（CSR） | **TODO** | MR-02 | — |
| MR-04 | M2-b：`MaterialLayerRgbComposer` 注入逐列颜色源 | **TODO** | MR-03 | — |
| MR-05 | M2 验收：两层贴图均正确 + 全量回归 | **TODO** | MR-04 | — |
| MR-06 | 文档收口与 MO-12 关闭 | **TODO** | MR-05 | — |

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

## 4. R-01 默认路径零漂移基线复核（TODO）

**目的：** 确认动代码前的基线仍成立，避免把上游漂移误判为本专项引入。

**命令：**

```bash
./build-slicesoft/main/Release/slicer_cli.exe --config <默认路径配置>
```

**判据：** TIFF 逐字节哈希 `3cbfdec213cfcf1a3397cfd1860c5baa7bc649b669249eddc2238f0b4f363b5f`

**说明：** 该基线取自 MATOPQ 专项，MATVOL 关闭时 `compose_layer` 不进入任何新代码路径。

---

## 5. R-02 单材质 + MATVOL + texture 基线建立（TODO）

**目的：** 这是 M1 唯一的回归风险面（设计文 §3.1 的否决方案正是在此处出问题），而**当前无现成基线**。必须先建基线再动代码。

**资产：** `model/obj/multi-material/tm3/`（单材质含贴图）或 `d0-varnish-test/fenandtou_d0_clean.obj`

**配置要点：**

```json
"texture":              { "enabled": true, "applyMode": "solid_volume_from_top_surface" },
"materialVolumePolicy": { "enabled": true, "mode": "closed_intervals" }
```

**产出：** 六通道统计 + TIFF 逐字节哈希，写入本卡

**判据：** M1 落地后该组数据必须**逐项不变**

---

## 6. MR-01 M1 实施（READY）

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

## 7. MR-02 M1 验收（TODO）

| 项 | 期望 |
|---|---|
| `tm2-5` `nail-L2` 段唯一色 | 1 个 = `(167,243,255)` |
| `tm2-5` `nail-L1` 段唯一色 | 14,363（不变） |
| `V+G` | `= 2,714,451`（不变） |
| 层数 | 124（不变） |
| R-01 基线 | 逐字节不变 |
| R-02 基线 | 逐项不变 |
| 全量回归 | 231 项，失败 7 项（继承基线，逐项相同） |

**统计工具：** 本会话所用脚本（`chan_stats.py` / `layer_profile.py` / `mtl_zrange.py`）在 scratchpad，若需长期保留应移入 `scripts/`。

---

## 8. MR-03 / MR-04 M2 实施（TODO）

见设计文 §5。要点：

- **MR-03**：`ReliefColumnInfo` 附带逐材质顶面，CSR 存储（`offsets` / `materialIndices` / `triangleIndices` / `barycentrics`）。两处填充点 `slicer.cpp:1375` 与 `:1478` 均在三角遍历循环内，同一遍完成，无额外几何开销。规模上界约 20MB。
- **MR-04**：`MaterialLayerRgbComposer` 增可选 `MaterialColumnColorSource`，回退顺序为「该材质贴图 → 该材质 Kd → MV-05 既有 fallbackPolicy」。缺图 / 无 UV / 越界三种情形的处置见设计文 §5.4，一律不静默。

---

## 9. MR-05 M2 验收（TODO）

| 项 | 期望 |
|---|---|
| `tm2-5` `nail-L2` 段唯一色 | 数千个（`13_24_46.png` 真实采样） |
| `tm2-5` `nail-L1` 段唯一色 | 14,363（不变） |
| `V+G` | `= 2,714,451`（不变） |
| R-01 / R-02 基线 | 不变 |
| 全量回归 | 231 项，失败 7 项 |

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
| 2026-09-02 | v1.0 | 首版。建立 MR-00..06 与 R-01/R-02 两项准备卡；MR-00 完成（含设计自审与 M1 首版方案否决记录）；MR-01 标 READY。 |
