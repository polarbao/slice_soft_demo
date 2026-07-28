# CLAUDE_08 基线差异与文档更新清单（2026-07-22 → 2026-07-27）

> 目录位置：`docs/claude/VERIFICATION/`。校验日期：2026-07-27。
> 结论先说：**需要更新，且属于"结论级"更新而非措辞微调**——我 07-22 基线下的多条载荷性判断已被新开发推翻。

## 1. 为什么必须更新（A）

07-22 之后项目新增了 **Stage 13 专项**并完成了 **12E 双模式落地**，其中至少 **5 条**我此前写入文档的"当前事实"已经过期。若不修订，后续在 Claude 中做架构分析会基于错误前提。

## 2. 已核实的关键变化（A）

### 2.1 新专项：Stage 13 模型场景排版 + 联合切片 + TIFF 原生预览

目标是把"导入单模型直接切片"演进为：

```text
导入模型 → 场景俯视检查 → 选择和变换 → 多模型规则排版
→ 几何/幅面/碰撞准入 → 联合切片 → 单一 RGBWSV package
→ 直接从生产 TIFF 检查单通道与全材料叠加
```

状态（`TASKS_12_13_后续开发计划总览清单.md` v1.6，2026-07-27）：

| 工作流 | 状态 |
|---|---|
| 13A 模型俯视与变换 | **13A-01..05 COMPLETE**，M13-1 CANDIDATE PASS |
| 13B 多模型排版与联合切片 | **13B-01..04、13B-04A COMPLETE**；13B-05 READY FOR FIXTURE；13B-06/07 WAIT |
| 13C TIFF 原生统一预览 | 设计与原子准备完成，**代码未开始**（13C-01 READY）|
| 12E-09A Diagnostic UI | 09A-01/02 COMPLETE；09A-03..06 PREPARED |
| 12E-10 最终收口 | 概念级准备，执行文档不完整 |
| 12F 性能专项 | 12F-01 COMPLETE；**12F-02..09 NOT ACTIVE** |
| 12G-TCWS | **FROZEN / NO AUTHORIZATION**（2026-07-27）|

当前原子任务：**13B-05 fixture 全局 Raster 与联合层合成**；下一 Gate：13B-05 PASS → 13B-06 单 package 与 scene report。

外部 Gate（未关闭）：设备 `buildVolume`、机器原点与 X/Y 轴方向、22 实例性能预算、mixed-profile 决策、3D 后端 Spike。

### 2.2 双模式（12E-08D）已落地 —— 推翻旧结论

新增并已存在的代码（A）：

```text
src/slicer_core/config/SlicePipelineConfig.h    → SlicePipelineMode{Legacy, GlobalSurfaceShell}
                                                   SlicePipelineConfig{mode, explicitly_configured}
                                                   SlicePipelineErrorCode（11 个稳定码）+ SlicePipelineError
src/slicer_core/pipeline/SlicePipelineRouter.h   → ResolveSlicePipelineRoute / RequireSlicePipelineRoute
                                                   SlicePipelineRouteDecision{requested/effective_mode,
                                                   allowed, fallback_applied, error_code}
src/slicer_core/pipeline/GlobalSurfaceShellProductionPipeline.h / ...ProductionPackage.h
                        / ...ProductionLayerAdapter.h / ...MaterialEvidence.h
```

准入事实（`AGENTS.md`）：`global_surface_shell_restricted_candidate` 与 `global_surface_shell_material_parity_candidate` 已作为**显式 opt-in Profile（0.01mm）**准入，`xiao_ma`/`yecan` 的 TIFF 与 RIP strict 通过。但 2026-07-24 的 09B 收口矩阵显示 **Global 比 Legacy 慢 4.09×–5.92×、峰值内存 8.19×–8.74×**，因此 **Legacy 仍为默认，且禁止静默回退**。

> 也就是说：不再是"global 只能诊断/BLOCKED"，而是"**global 已可写生产包，但仅限显式 opt-in Profile，且性能/内存代价显著，故不作默认**"。

### 2.3 DPI 不再固定 600 —— 推翻旧结论

旧：`config.cpp` 强制 `dpiX==dpiY==600`。
新（A）：改为区间校验 `IsSupportedOutputDpi(dpi) = dpi ∈ [kMinimumOutputDpi, kMaximumOutputDpi]`，并新增 `IsOutputPixelSizeConsistent(dpi, pixelSizeMm)`（按 `25.4/dpi` 加容差校验一致性）。对应 12E-09C「X/Y DPI」专项。

影响：像素间距不再恒为 42.3µm，`pixelSize=25.4/dpi` 仍成立但 dpi 可变，且允许 X/Y 各自取值。

### 2.4 新增两个子系统（A）

```text
src/slicer_core/scene/    MultiModelScene / ModelInstance / ModelTransform
                          SceneEffectiveConfig / SceneViewGeometry / SceneModel
src/slicer_core/layout/   GridLayoutPolicy（ComputeGridLayout：确定性行主序排版、
                          scene revision 乐观并发、7 个稳定错误码、fail-closed）
                          SceneCollisionService
```

`scene/` 从"16 行轻量容器"变成完整场景子系统；`layout/` 是全新模块。二者共同把数据模型从**单模型**扩展为**场景 + 实例（1..22）**。

### 2.5 单体仍在（A，重构判断的关键）

`run_slicer()` 仍位于 `src/slicer_core/slicer.cpp:3964`。**新能力是"在单体旁边新增模块"长出来的，而不是通过拆解单体获得的。** 这既说明模块化设计有效（新子系统边界清晰），也意味着结构债在持续累积——详见 `PLANNING/CLAUDE_09`。

## 3. 文档更新清单（逐条）

> 状态：✅ 本轮已改 / 🔶 建议后续随阶段推进再核

| 文档 | 过期内容 | 处理 |
|---|---|---|
| `BASELINE/CLAUDE_00` §5 锚点 | "无 slicePipeline.mode"、"dpi 强制 600"、12E-08D BLOCKED | ✅ 已加"基线已更新"提示并指向本篇 |
| `ANALYSIS/CLAUDE_01` §4/§6 | global "diagnostic-only/blocked"；`slicePipeline.mode` 为目标态；管线可组合性 35% | ✅ 已修订当前生产事实 |
| `ANALYSIS/CLAUDE_02` §4 | 双模式"Accepted，尚未实现" | ✅ 已标注已落地 + 性能代价 |
| `ANALYSIS/CLAUDE_03` §1.5/§2 | 12E-08D BLOCKED 阻断链 | ✅ 已改为"已落地为 opt-in，阻断转为性能/默认化" |
| `KNOWLEDGE/CLAUDE_K01` §4 | `dpi` 强制 600 → 42.3µm 恒定 | ✅ 已改为区间 + 一致性校验 |
| `KNOWLEDGE/CLAUDE_K03` | global 仅诊断、未落地 | ✅ 已改为已实现 Router + opt-in 准入 |
| `KNOWLEDGE/CLAUDE_K06` §1/§11 | dpi=600 固定写入"红线" | ✅ 已修正（dpi 不再是固定协议项）|
| `KNOWLEDGE/CLAUDE_K02`/`K05` | 隐含"单模型"假设 | 🔶 已加场景/实例提示；Stage 13 联合切片落地后需补 K07 |
| `PLANNING/CLAUDE_04` §3 | 近期计划以"双模式收口"为主 | 🔶 主线已变为 Stage 13 + 性能默认化，见 `CLAUDE_09` §7 |
| `PLANNING/CLAUDE_06` T-30/T-31/T-32 | 视双模式为待做任务 | 🔶 T-30/31 实质已完成；改由 `CLAUDE_09` 承接新 backlog |
| `VERIFICATION/CLAUDE_07` | 校验记录基于 07-22 | ✅ 已加指向本篇的更新说明 |

## 4. 结论（P）

1. **必须更新**：涉及"双模式是否可生产""dpi 是否可变""是否已有场景/排版能力"三类判断，都会直接改变后续架构决策，属于必须修订的结论级差异。
2. **更新方式**：本篇作为**差异真源**（delta 记录），其余文档只做定点修订 + 指回本篇，避免多处重复叙述再次不同步。
3. **后续机制建议**：每次专项推进后，先更新本篇 §2，再定点改受影响文档——这样 `docs/claude` 的"当前事实"永远只有一个入口。
