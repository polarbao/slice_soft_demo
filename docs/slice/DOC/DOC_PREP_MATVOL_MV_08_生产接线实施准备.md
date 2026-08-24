# DOC_PREP_MATVOL MV-08 生产接线实施准备

> 文档状态：**CONFIRMED / MV08-Q1..Q3 已回签 / 08A 可开工**
> 版本：v1.1 ｜ 日期：2026-08-24
> 任务真源：`../../codex_task/current/TASKS_MATVOL_多材质纵深体积RGB与按需补白根治专项任务清单.md`
> 依据：`CODEX_PROMPT_MATVOL` §2「生产语义变化卡必须先给 Implementation Plan 并等待确认」
> 排期依据：用户 2026-08-24 裁定 MV-08 优先，且先出实施准备文档再动手

---

## 0. 为什么单独立这份准备

MV-08 是**真实模型测试的唯一关键路径**。MV-01..07 完成的是语义栈与宿主管道，但
`src/` 与 `apps/` 下对 `MaterialVolumePlan`、`ComposeMaterialLayerRgb`、
`MaterializeMaterialOwnershipLayer`、`MaterialTopologyClassifier`、`BuildMaterialVolumeReport`
的引用数**全部为零**——没有任何生产调用方。MV-04 是开放表面壳层候选、MV-09 是回归矩阵、
MV-10 是生产准入，三者都不在这条路上。

本卡要动 `slicer.cpp` 的逐层合成路径，而 2026-08-24 当天 MV-07A 刚在同一区域出过一个
出厂缺陷（宿主自造 `rgb.source` 枚举，见 `DOC_DECISION_MATVOL` 与提交 `51a3e15`）。
因此接入面必须先写死再动手。

---

## 1. 接入形状（A 级，已实测）

### 1.1 逐层合成本来就是逐像素逐层的

`compose_layer`（`src/slicer_core/slicer.cpp:3171`）返回**单层**像素缓冲，由 `:4700`
的层循环逐层调用；其内部 `compose_material_policy_pixel`（`:2960`，调用点 `:3259`）
同时接收 `pixel_index` 与 `layer_index`。

⇒ MATVOL 的 caller-owned 单层物化 API（`MaterializeMaterialOwnershipLayer` 写入调用方
提供的单层缓冲）与该形状**天然匹配**，不需要为它改造合成循环的结构。

### 1.2 逐列预计算结构已有既定传参形状

`compose_layer` 已经以指针接收三个「循环前算一次」的逐列结构：
`texture_columns`、`material_role_columns`、`column_ranges`（`:3179-3182`），
各自由 `config.*.enabled` 决定传指针还是 `nullptr`（`:4709-4712`）。

⇒ `MaterialVolumePlan` 应照同一形状加入：循环前构建一次，以
`const MaterialVolumePlan* material_volume_plan` 传入，`enabled` 为假时传 `nullptr`。
**不新增全局状态，不改既有三个参数的语义。**

### 1.3 内存形态：Legacy 本就是 retained dense

层循环以 `model_masks.at(layer_index)`（`:4703`）取层掩码，即**全部层掩码同时驻留**。
这正是 MEMFLOW 要解决的对象。MATVOL 在此之上只新增一个 compact CSR interval plan
（`columnIntervalOffsets_` 大小为 columnCount+1，加扁平化 `intervals_`），
数量级远小于既有的全层掩码驻留。

⇒ **MV-08 对 MEMFLOW 的依赖是组织性的，不是技术性的。** MEMFLOW 解决的是「把既有的
retained dense 变成有界流式」，与「能否表达 Z 向材质所有权」是两件事。本卡按 Legacy
retained dense 接线即可，内存代价等于现状加一个紧凑 plan。
（原任务卡把 MV-08 依赖写成 `MF-03B4/MF-04`，据本节应改为「可选加速项」而非开工门。）

---

## 2. 唯一的真实缺口：plan 构建器要的网格类型，`run_slicer` 没有

`MaterialVolumeBuildRequest`（`src/slicer_core/materials/volume/MaterialVolumePlan.h`）要求：

```text
const AdaptedTriangleMesh* mesh;
const MaterialVolumePolicyConfig* policy;
MaterialVolumeGrid grid;
std::function<bool()> cancellationRequested;
```

而 `AdaptedTriangleMesh` 全仓只有一个产出口
`AdaptSceneModelToTriangleMesh(const SceneModel&)`
（`src/slicer_core/geometry/SceneModelTriangleMeshAdapter.h:53`），
`run_slicer` 走的是 `load_model_report(config, config_dir)`（`slicer.cpp:4371`），
手里是 `ModelReport` 而**没有** `SceneModel`；`slicer.cpp` 对该适配器的引用数为零。

好消息是 `ModelReport`（`src/slicer_core/model.h:106`）已经携带全部所需数据：

| 需要 | `ModelReport` 中的来源 |
|---|---|
| 三角面 | `triangles` |
| 逐三角材质名 | `triangle_textures[i].material_name`（`model.h` 的 `TriangleTextureInfo`） |
| 材质定义（含 Kd） | `material_infos`，**与 `AdaptedTriangleMesh::material_infos` 同类型** |

**两个可选方案，需在实施前择一并写入本文档：**

- **方案 A（推荐）**：新增 `ModelReport → AdaptedTriangleMesh` 的窄适配器，
  只填 MATVOL 实际读取的字段，其余留空并在注释中写明。
  优点是不动 `MaterialVolumeBuildRequest` 的既有合同，MV-01..06 的全部用例不受影响。
  风险是「留空字段」若将来被 MATVOL 读取会静默拿到空值——必须以断言封住。
- **方案 B**：给 `MaterialVolumeBuildRequest` 增加一条以三角面加逐三角材质名直接构建的入口。
  优点是不产生半填充的 `AdaptedTriangleMesh`，风险是改动已被五个测试目标覆盖的既有合同。

---

## 3. 当前没有任何来源产生 RGB（这是本卡必须补上的）

启用 `materialVolumePolicy` 时：

1. `materialProcessProfile.rgb.source` **不驱动 Legacy 合成**——它只被解析、校验、
   回写进报告（`slicer.cpp:3994`），以及在 Global 管线里做一次比较
   （`GlobalSurfaceShellProductionPipeline.cpp:141`），而 matvol 禁用 Global。
2. 真正驱动合成的是 `material_policy.rgb.source`（`slicer.cpp:2969`、`:2978`、`:3250`），
   而 matvol 校验**禁用** `materialPolicy`。
3. 纹理路径关闭。

⇒ 若只放行校验而不接线，产出是全黑，或在 `require_rgb_pixels` 上以
`E_MATERIAL_PROCESS_PROFILE_EMPTY_RGB` 失败。这正是 2026-08-24 加入生产入口门的原因。

---

## 4. 必须移除或改写的两道门

| 位置 | 现状 | 本卡应做 |
|---|---|---|
| `slicer.cpp` `EnsureMaterialVolumeWiringImplemented` | 生产入口 fail closed，消息指向 MV-08 | **接线完成后整体移除**；该函数注释已写明这一点 |
| `config.cpp` 约 1207 起的 matvol 校验块 | 只校验形状：mode/missingMaterial/openSurface/overlap/slicingMode/pipeline/geometrySampling 及 materialPolicy 与 roleMapping 禁令 | **保持不变**。契约先行是刻意设计，`MatvolTopologyTests.cpp:146` 与 `MatvolWhiteCarrierTests.cpp:75` 以 `enabled=true` 期望校验通过 |

---

## 5. 建议的原子拆分

```text
MV-08A  ModelReport→网格适配器 + 以 03.obj 证明 plan 结构正确
        【不改 slicer.cpp】纯新增文件与测试目标            —— 零生产行为变化
MV-08B  compose_layer 内按 (pixel_index, layer_index) 查 owner 并合成 RGB，
        移除生产入口门                                    —— 本卡的生产语义变化在此
MV-08C  按需补白顺序接入（复用 MV-06 的 ApplyUnprintableWhiteCarrier）与报告落盘
```

拆分理由：把「几何是否算对」与「合成是否接对」两类失败分离开。

> **v1.1 更正**：v1.0 的 08A 曾写作「循环前构建 plan 并以指针传入 `compose_layer`，
> 结果只进报告」。该写法**不可验证**——`EnsureMaterialVolumeWiringImplemented` 在
> 模型加载之前就抛出，那段接线在 08B 移除该门之前是死代码，跑不到也观测不到。
> 更正后 08A **完全不动 `slicer.cpp`**：只交付适配器与测试目标，用 `03.obj` 直接构建 plan
> 并断言逐材质区间数、区间层范围与无塌缩列。全部 `slicer.cpp` 改动集中到 08B。
> 这样 08A 的生产行为变化严格为零，也与 MV-01..06 的非生产验证形状一致。

---

## 6. 静默出错的风险与对应断言

| 风险 | 会静默产出错误结果吗 | 拟用的门 |
|---|---|---|
| 自动摆正后材质上下颠倒 | 会：顶面全绿 vs 全桃色，肉眼难判 | 对 `03.obj` 断言生产姿态下顶层 owner 为 `01`（实测 100% 绿），与 DOC_DECISION §2.1 一致 |
| owner 解析出但 Kd 查表落空 | 会：静默取到默认色 | `ComposeMaterialLayerRgb` 已有 `E_MATVOL_MODEL_PIXEL_UNOWNED`，须确保接线后仍触发 |
| plan 与 grid 尺寸不一致 | 不会：已 throw `std::invalid_argument` | 保持 |
| 只有部分层用了 owner | 会：层间突变 | 逐层统计 owner 覆盖像素数写入报告，回归断言其单调性 |
| 旧 Profile 被误改 | 会：影响既有工艺 | 沿用 MV-06 做法，以哨兵证明未启用时逐字节不变 |

---

## 7. 边界

不改 `p0.rgbwsv.2`、RGBWSV 通道序与语义、SPI v1 的 11 个导出与 15 项能力、
Worker 文件合同、既有预设的字节与 `profileHash`。
生产默认仍为 matvol 关闭，新预设保持显式 opt-in。
不合入 MEMFLOW 分支（2026-08-24 裁定暂缓）。

---

## 8. 待确认事项

| 编号 | 事项 | 建议 |
|---|---|---|
| MV08-Q1 | §2 的方案 A 与方案 B 择一 | **已回签：方案 A（窄适配器）** |
| MV08-Q2 | MEMFLOW 依赖由「开工门」降为「可选加速项」 | **已回签：接受**，MV-08 不再等 MEMFLOW |
| MV08-Q3 | 三段拆分是否照此执行 | **已回签：是**（08A 按 v1.1 更正后的口径） |

---

## 8.1 回退与分级止损（用户 2026-08-24 要求先写清）

**每段一个独立提交**，回退即 `git revert <sha>`，不改写历史。三段的风险等级不同，
止损粒度也不同：

| 段 | 生产行为变化 | 效果不好时的处置 | 回退代价 |
|---|---|---|---|
| 08A | **零**。不动 `slicer.cpp`，只新增适配器与测试目标；matvol 仍被生产入口门挡住 | 几何算错就在 08A 内修，不进入 08B | 无。回退只是删掉新增文件 |
| 08B | **有**。合成接线并移除生产入口门 | 回退本段即可：门函数随之回来，行为退回「以诚实错误 fail closed」，而不是退回静默出错 | 单提交回退；08A 的适配器与断言保留 |
| 08C | 补白顺序与报告落盘 | 单独回退，不影响 08B 的 RGB 结果 | 单提交回退 |

**为什么 08B 的回退是安全的：** 生产入口门 `EnsureMaterialVolumeWiringImplemented` 是 08B 才移除的。
一旦回退 08B，该门恢复，启用 matvol 的作业重新以「生产接线未实现」明确失败，
**不会**出现「跑完了但结果是错的」这种最坏形态。这正是当初把它加在生产入口而非契约校验的价值。

**两条独立于回退的兜底：** 其一，生产默认始终为 matvol 关闭，新预设是显式 opt-in，
任何一段出问题都不影响既有工艺；其二，沿用 MV-06 的哨兵做法，
以未启用时输出逐字节不变作为每段的必过断言。

**效果不好但不必回退的情形与补充方向：** 若 03.obj 出现材质上下颠倒，
按 §6 第一行用生产姿态顶层 owner 断言定位，属 08B 内修；
若出现层间突变，查逐层 owner 覆盖像素数的单调性；
若性能或内存不可接受，此时才引入 MEMFLOW 作为**加速项**（MV08-Q2 已回签其为可选），
而不是回退功能本身。

## 9. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-24 | v1.1 | MV08-Q1..Q3 全部回签：方案 A 窄适配器、MEMFLOW 依赖降为可选加速项、三段拆分照此执行。**更正 §5 的 08A 口径**：v1.0 写法不可验证，因生产入口门在模型加载前即抛出，那段接线在 08B 之前是死代码；更正后 08A 完全不动 slicer.cpp。新增 §8.1 回退与分级止损。 |
| 2026-08-24 | v1.0 | 建立 MV-08 实施准备。实测确认逐层合成本就逐像素逐层、逐列结构已有既定传参形状、Legacy 为 retained dense，据此判定 MEMFLOW 依赖为组织性而非技术性。定位唯一真实缺口为 `MaterialVolumeBuildRequest` 要求的 `AdaptedTriangleMesh` 在 `run_slicer` 中不存在，并给出两个方案。登记 MV08-Q1..Q3。 |
