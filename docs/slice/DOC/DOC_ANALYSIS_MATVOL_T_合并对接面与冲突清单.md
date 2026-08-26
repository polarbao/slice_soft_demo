# DOC_ANALYSIS MATVOL-T 合并对接面与冲突清单

> 文档状态：**参考件 / 供 MATVOL 与 MATVOL-T 两条线对接时使用**
> 版本：v1.0 ｜ 日期：2026-08-24
> 撰写方：MATVOL 专项（`product/packaged-slicer`）
> 对接方：MATVOL-T 专项（`codex/matvol-t-channel-protocol`，工作树 `slice_soft_demo_matvol_t`）
>
> 本文只陈述事实与冲突面，不替任何一方裁定取舍。所有结论均标注出处，
> 引用自对方分支的内容截至 2026-08-24 该工作树的**未提交状态**。

## 1. 为什么需要这份文件

两条线**同时修改同一批文件**，且对方分支的代码基线停在 `90a59ba`，而 MATVOL 在该基线之后又有 6 个提交，其中 4 个正落在对方改动过的位置上。越晚合并冲突越难解，且有两处是**语义冲突而非文本冲突**，自动合并不会报错但结果是错的。

MATVOL 侧 `90a59ba` 之后的提交：

| 提交 | 影响冲突面 |
|---|---|
| `5cb2549` MV-09 cancel 取消点贯通 | `slicer.cpp`、`slicer.h`、`LegacySceneLayerAdapter.cpp` |
| `902f565` MV-09 回填 | 仅文档 |
| `0bc3c75` MQ-06 有界开边放宽 | `config.h`、`config.cpp`、`MaterialVolumePlan.cpp`、`HostVolumetricProfile.c` |
| `2964b7b` MQ-06 回填 | 仅文档 |
| `78fccb8` 未归属填补次级规则 | `slicer.cpp` |
| `36f5afe` 伪彩色还原 | `MaterialPreviewComposer.h`、`HostPackageReviewPanel.cpp` |

## 2. 四处代码冲突面

### 2.1 `contracts/slicesoft.material_volume_report.1.schema.json`（MATVOL 拥有）

MATVOL-T 把该契约的协议字段由常量放宽为枚举：

```diff
-    "packageProtocol": { "const": "p0.rgbwsv.2" },
+    "packageProtocol": { "enum": ["p0.rgbwsv.2", "p0.rgbwsvt.1"] },
```

MATVOL 侧当前仍是 `const`（本文件 `:23`）。这是**对 MATVOL 所有的契约的直接修改**，也是最可能产生文本冲突的一行。

**MATVOL 的立场**：不反对该放宽——它是纯增量、旧值仍合法。但按仓库既有规则，就地修改已发布 `.1` 契约需要在 MATVOL-T 的决策文档里留一句授权痕迹，目前没有。同时 `tests/contracts/ValidateJsonSchemas.py` 的 `ValidateMaterialVolumeReportContracts` 需同步加正例，对方已做。

### 2.2 `src/slicer_core/materials/volume/MaterialVolumePlan.h/.cpp`（MATVOL 拥有）

MATVOL-T 给 `MaterialVolumeBuildRequest` 增加了 `std::string materialNameFilter;`，并在 plan 构建的逐材质循环顶部加了 `continue` 守卫，另加了 filter 感知的错误消息变体。

**冲突风险为高**，因为 MATVOL 在 `0bc3c75` 中改动了**同一个循环**：新增 `maxBoundaryEdges` 放行条件，并把拒绝消息改为附加「放行为何被拒」的细节。两处改动物理相邻。

**建议的合并顺序**：先取 MATVOL 侧的放行条件与错误消息（它们是一个整体，拆开会让诊断退化），再把 `materialNameFilter` 的 `continue` 守卫加在循环最顶端——该守卫应在拓扑判定**之前**执行，与放行条件互不干扰。

### 2.3 `src/slicer_core/reports/MaterialVolumeReport.cpp`（MATVOL 拥有）

MATVOL-T 新增 `BuildDisabledMaterialVolumeReport()`，把 MATVOL 写在 `slicer.cpp` 里的禁用报告字面量抽成函数，并在 `slicer.cpp` 中于 transfer session 激活时**事后改写** `packageProtocol` 字段。

**MATVOL 的提醒**：MATVOL 在 `a71191e` 里让该报告的 `warnings` 承担 MQ-05/MQ-06 的放行披露义务（读取 `plan.ToleratedSelfIntersectingMaterials()`）。抽取函数时**不要丢掉这段**，否则被放行的自交/开边材质将不再出现在产出物上，而 `MaterialVolumePlan.h` 的注释明确要求「不得静默吞掉」。

### 2.4 `src/slicer_core/slicer.cpp`（双方均大量改动）

这是最重的一处。MATVOL-T 把 `MaterialVolumeGrid` 的构造上提为共用常量；MATVOL 在同一区域有三处改动：

- `5cb2549` 在 plan 构建前透传 `options.cancellationRequested`
- `78fccb8` 在层循环内新增「未归属像素按下层材料填补」及其次级规则，并预取每列最低区间材质
- `a71191e` 的补白与逐层统计

**语义提醒**：MATVOL 的取消点透传必须保留。plan 构建是本路径最长的不可中断窗口且发生在 `gridcallback` 之前，若合并时丢失该行，取消在生产路径上会**静默失效**——本轮已用变异检验证明：移除该行后取消完全不生效，切片会跑完 10999 ms 而非被中断。

## 3. 两处需要对方吸收的 MATVOL 侧结论

### 3.1 `08.obj` / `09.obj` 并非「永久 BLOCKED」

MATVOL-T 的任务卡把 `08/09` 标为 `BLOCKED EXPECTED` 且不予修复，理由是 3 条开边。**该结论在 MATVOL 侧已被推翻并解决**（MQ-06，提交 `0bc3c75`）：

- 三个资产的自交对数**完全相同，都是 8 对**；`03` 与 `08/09` 的真实差别只是后者材质 02 有 **3 条真开边**。
- 实测 `08.obj` 材质 02 在 0.100 / 0.042 / 0.021 mm 三档分辨率下**奇数交点列恒为 0**（最多 385,319 个覆盖列），即那 3 条开边不破坏射线奇偶性。
- 新增 `materialVolumePolicy.topology.maxBoundaryEdges`（默认 0，宿主 MATVOL 预设设为 8）后，`08/09` 可正常切出多材质结果。

**对 MATVOL-T 的直接影响**：其 `transferChannelPolicy.topology` 目前只有 `selfIntersectionPolicy` 与 `maxSelfIntersectionPairs`，**缺 `maxBoundaryEdges`**。由于 T 侧复用 `BuildMaterialVolumePlan`，合并后若不带上该字段，`08/09` 在 T 路径上仍会被拒。授权依据见 `DOC_DECISION_MATVOL_MQ_06_有界开边放宽授权.md`。

### 3.2 `08/09` 的材质 02 是**黄色**，不是浅桃色

三个 T 工艺样例的 `materialDiffuseRgbValues` 目前只有 `[[255,220,198]]`（03 的浅桃色）。`08.mtl` / `09.mtl` 把材质 02 定为 `Kd 1.0000 1.0000 0.0000`，即 **`[255,255,0]`**。

由于 MATVOL-T 的识别方式是**材质级 Kd 精确匹配**（明确禁止模糊距离与材质名硬编码），要覆盖 `08/09` 必须把黄色作为显式别名加入该数组。

此事实本轮曾让 MATVOL 侧误判为「材质 02 完全缺失」——实为断言在找一个该资产上不存在的颜色。**涉及这三个资产的任何颜色断言都必须按资产取色**。

## 4. 治理留痕缺口（不是技术问题，但会卡冻结面门禁）

仓库内有两处明确禁止第七通道的记载，MATVOL-T 的六份文档中**既未引用也未重新授权**：

| 出处 | 原文 |
|---|---|
| `docs/slice/DOC/DOC_REVIEW_12G_TCWS_现有RIP白区合同与六通道策略比对.md:19` | 「本专项保持单 TIFF 六通道 R/G/B/W/S/V，不增加第七通道。」 |
| `docs/slice/DOC/DOC_DECISION_12X_剩余任务优先级与专项冻结.md:242` | 「不得在本轮新增纹理铺底层、TIFF 第七通道、压缩、BigTIFF 或多 IFD。」 |

两条措辞分别是「**本专项**」与「**本轮**」，因此「新专项不受其约束」是可主张的——但该主张目前没有写下来。另有 `DEV_MATVOL` 的非目标明列「修改 RIP 或新增通道」。

**建议**：MATVOL-T 在其决策文档里补一节，引用这两处并说明为何不适用或已获重新授权。这与本轮 RIPFLOW 那边直接提升 `contracts/slicesoft.rip.*.2.schema.json` 版本号是同一类问题——不是技术错误，而是留痕缺失，会在冻结面复核时被挡。

## 5. MATVOL-T 已经解决了 MATVOL 侧两个悬而未决的产品问题

这一点对排期有利，记录于此以免重复建卡。

**其一，带贴图甲片 + 缩裹共存。** 用户场景是同一片甲片上表面有贴图、下方仍有缩裹材质。在纯 MATVOL 下这需要「同一模型内 RGB 来源按深度切换」，因为 `compose_layer` 里纹理分支与 MATVOL 分支是互斥的 `else if`。更棘手的是这类贴图模型的 `Kd` 常为纯黑（例如 `MF_Xiao_ma_Xiaozhi_ty03.obj` 的唯一材质 `blinn5SG`，`Kd 0.00 0.00 0.00`，颜色全在 `map_Kd`），而 MATVOL 按材质取一个平铺 Kd，结果会是**整片纯黑**。

MATVOL-T 的 `obj_mtl_texture_rgb_only_rgbwsvt.json` 已给出答案：缩裹让出 RGB 去 T 通道，贴图独占 RGB，且**完全不启用 `materialVolumePolicy`**。**故 MATVOL 侧不再为此立卡。**

**其二，整模型输出到白墨或光油通道。** MATVOL-T 的 `nail_white_underbase_only_rgbwsvt.json` 与 `nail_varnish_only_rgbwsvt.json` 用 `materialPolicy.white/varnish` 配 `mode: "all_model"` 实现；MATVOL 侧的既有预设 `single_material_relief_white` / `single_material_relief_varnish` 亦可做到同样的事。

**两者共同的剩余缺口是「逐模型工艺指定」**：`MultiModelProductionService.cpp:857-859` 把同一个 config 路径发给场景内所有实例，`MultiModelScene.cpp:958-972` 又主动拒绝实例间 Profile 不一致。`SceneModelInstance::resolvedprofileid` 字段已存在但被强制统一。该缺口与 MATVOL-T 的 **T-06（Scene/Worker/Host 透传）** 改动区域重合，两边同时动必然冲突。

## 6. 建议的对接次序

1. **MATVOL-T 先补两处配置**：`transferChannelPolicy.topology` 增加 `maxBoundaryEdges`；样例的 `materialDiffuseRgbValues` 增加 `[255,255,0]`。这样合并后 `08/09` 在两条路径上行为一致。
2. **MATVOL-T 补治理留痕**：引用并处置第 4 节的两条红线。
3. **合并方向建议由 MATVOL-T 向 `product/packaged-slicer` 合**，理由是 MATVOL 侧的改动更细碎且已全部提交，而 MATVOL-T 侧尚未提交、体量大、便于在本地一次性解冲突。
4. **逐模型工艺指定留到 T-06 之后**，由单独一张卡承担，避免两边同时改场景到 Worker 的传递链路。

## 7. MATVOL 侧承诺

- 在对方合并期间**不再改动**第 2 节列出的四个文件，除非发现缺陷；如需改动会先在此文件追加说明。
- 合并后由 MATVOL 侧负责复核：取消点透传是否保留、`warnings` 披露是否保留、`maxBoundaryEdges` 是否生效、以及 `08/09` 在两条路径上的一致性。
- 现有验证资产可直接复用：`matvol_production_wiring_tests` 已覆盖 `03/08` 两资产、生产实设（635/600 dpi、0.033 mm）、严格 RIP 校验、取消与性能记录；`matvol_reality_plan_tests` 覆盖三资产的逐材质拓扑事实。

## 8. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-24 | v1.0 | 建立对接说明。列出四处代码冲突面与合并建议、两处需对方吸收的 MATVOL 结论（`08/09` 非永久阻塞、材质 02 为黄色）、第七通道红线的治理留痕缺口、以及 MATVOL-T 已解决的两个产品问题与共同剩余缺口。 |
