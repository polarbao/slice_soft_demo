# CLAUDE_00 分析方法与事实基线

> 本篇约定后续所有 Claude 分析文档共用的证据口径、基线与术语，确保结论可核对、可维护。
> 目录位置：`docs/claude/BASELINE/`。跨文档引用键为文件序号（如文中"见 02 §5.2"即指 CLAUDE_02）。
>
> ⚠ **基线已推进（2026-07-27）**：§5 的部分事实锚点（`slicePipeline.mode` 不存在、dpi 强制 600、12E-08D BLOCKED）已过期。**方法论与证据分级仍然有效**，但事实锚点请以 [`VERIFICATION/CLAUDE_08 基线差异与文档更新清单`](../VERIFICATION/CLAUDE_08_基线差异与文档更新清单.md) 为最新入口。

## 1. 分析基线（Baseline）

| 维度 | 基线内容 |
|---|---|
| 分析日期 | 2026-07-22 |
| 代码基线 | 主线 `src/`、`apps/`、`tests/`、`scripts/`、根 `CMakeLists.txt`、`vcpkg.json` 的当前状态 |
| 教程基线 | `docs/tutorials` v1.0（教程自述基线 2026-07-20，同步到提交 `bcb0dfa` 记录状态）|
| 正式文档基线 | `docs/slice` 全量（PRD/DEV/DEMO/ROADMAP/REPORT/DOC），以最新 `REPORT_12E_启动准备状态.md`（2026-07-22）为阶段状态锚点 |
| 规则基线 | 根 `AGENTS.md`、`.agents/AGENTS.md`、`.agents/docs/*` |

> 说明：分析期间 Windows 环境未提交改动可能与本快照存在差异。任何据本集推进的工作，都应先执行 `git branch --show-current` 与 `git status --short`，并以当时代码为准。

## 2. 证据分级（沿用项目 A/B/C/D）

本集严格沿用 `.agents/AGENTS.md §2` 与教程 00 的分级，并在每条关键结论旁标注：

| 级别 | 含义 | 能否作为"已实现"依据 |
|---|---|---|
| **A** | 当前代码 / CMake / 脚本 / 测试 / fixture | 能 |
| **B** | `docs/slice` 正式 PRD/DEV/ROADMAP/Decision | 只证明方向，不证明落地 |
| **C** | `docs/archive`、历史报告、聊天、已完成任务 | 仅背景 |
| **D** | 废弃/冲突资料 | 仅追溯，不可作依据 |
| **P**（本集新增标记）| **Claude 的推导 / 建议 / 打分** | **不可**，属主观判断，需项目方复核 |

冲突处理原则：**代码/CMake/带命令的最新 REPORT > `docs/slice` 目标文档 > `docs/codex_task/current` > archive/聊天**。

## 3. 四抽屉归位法

阅读或引用任何结论时，先归入以下抽屉之一（教程 00 已定义，本集强制使用）：

```text
Current State      当前代码确实能做到      例：legacy CLI 可写完整 p0.rgbwsv.2 包（A）
Target State       已批准但未全部落地      例：slicePipeline.mode 双模式（B，config.h 尚无此字段）
Historical State   过去为何这样演进        例：09P OpenVDB experimental path（C）
Pending Confirmation 需真实证据或用户授权    例：12E-08D 生产写包接入（B + 待授权）
```

最常见误判是把「有设计文档 / 有 demo / 有报告字段」当作「生产功能可用」。本集所有"现状"结论都必须能落到 A 级证据，否则改标 B/P。

## 4. 本次分析的取证方法

为保证结论可核对，本集结论主要来自以下取证动作（均为只读）：

1. **教程通读**：`docs/tutorials/00–16` 全套，建立心智模型与"当前态/目标态"边界；
2. **正式文档抽取**：`docs/slice/README`、`ROADMAP_12E_*`、最新 `REPORT_12E_*`、`DOC_DECISION_12E_*双切片模式*`、`DOC_SCHEMA_12E_DualSlicePipelineConfig*`、`.agents/*`；
3. **源码实测**：直接读取并核对关键文件——
   - `src/slicer_core/pipeline/SlicePipeline.cpp`（确认 14 步为字符串、`RunSlicePipelineLegacy` 在预检门成功回调中于**第 45 行**整体 `run_slicer()`）；
   - `src/slicer_core/config.h`（确认 `SliceConfig` 结构、默认值、**无 `slicePipeline` 字段**）；
   - `.agents/AGENTS.md`（确认红线、协议常量、阶段轨道）；
   - 通过结构化清单核对 `slicer.cpp≈4830`、`model.cpp≈1662`、`config.cpp≈1030` 等行数与模块目录职责、CMake 目标、测试与脚本清单。

> 行数为通过行计数工具获得，误差约 ±1；引用时以量级和"最大/最集中文件"的相对关系为准，不追求逐行精确。

## 5. 关键事实锚点（A 级，供后续文档反复引用）

以下为本集反复引用的 A 级事实，集中列出以便核对：

**协议与默认值（`config.h` / `tiff_io.h` / `.agents/AGENTS.md`）**

```text
schema        = p0.rgbwsv.2
channelOrder  = R G B W S V         （config.h 默认 {"R","G","B","W","S","V"}）
bitDepth      = 8 (uint8)
polarity      = black_is_print       printValue=0  emptyValue=255
优先级         Model > Support > Empty
背景默认       background.value = 255
默认 DPI       600 x 600            默认层厚 layer_thickness_mm = 0.01
存储           stripped(默认) / tiled；rowsPerStrip=64；tileSize=[256,256]
SupportType    仅存在于 metadata/report/debug，绝不进 TIFF 通道取值
```

**管线现实（`pipeline/SlicePipeline.cpp`）**

```text
DefaultSlicePipelineSteps() 返回 14 个步骤"名字"（字符串）
RunSlicePipelineLegacy() = ModelPreflight 门 + 成功回调内 run_slicer()（第 45 行）
=> 概念管线尚未分解；真实逻辑集中在 slicer.cpp（约 4830 行）
```

**配置现实（`config.h`）**

```text
SliceConfig 聚合约 25 个子结构；无 slicePipeline.mode 字段（双模式=目标态）
材料意图有 5 处可能重叠表达：
  material(legacy) / material_policy / model_fill / material_process_profile / material_role_mapping
experimental.openvdb_pipeline 全部默认安全关闭
```

**阶段与阻断（`docs/slice` REPORT/DECISION）**

```text
12A/12B/12C/12D 完成；12E 进行中；12F 仅 R0 完成，R1–R5 planned/not active
12E-08C R1/R2/R3 完成但 non-production；R3-04 = NO-GO / FROZEN
12E-08C-R4-01/02/03 完成；R4-04 Qt Preflight UI READY / 待显式执行
12E-08D 双模式生产写包 = BLOCKED（待修复输入 + 四例闭包 + 预算冻结 + Quick CI 基线 + 用户授权）
三必需 OBJ 严格准入被阻断：
  nai_you_new   boundaryEdges=113
  aishen_fudiao boundaryEdges=3, nonManifoldEdges=59
  meigui_fudiao nonManifoldEdges=10940
  R3-02 自交对数：8409 / 19270 / 5592
模型资产预检（15 个 OBJ/3MF）：7 strict-PASS / 1 需人工修复 / 7 需重建
已知失败基线：material_process_top2 widthPx expected=48 actual=226（记录基线，未在范围内修复）
```

**性能热点（`PRD_12F_*`，Release，`meigui_fudiao` 历史剖面）**

```text
supportGenerationMs ≈ 2801.870
layerComposeMs      ≈ 1595.716
maskSamplingMs      ≈ 92.076
=> 热点是"支撑生成 + 逐层材料合成"，不是 OpenVDB/几何采样
```

## 6. 打分口径（本集"完整度"如何计算）

01/03 中出现的完整度百分比为 **P 级（Claude 主观）**，口径如下，便于项目方校准或推翻：

- 每个能力域按 `设计清晰度(0.2) + 代码落地(0.4) + 测试/证据(0.25) + 生产可用(0.15)` 加权；
- "生产可用"严格按项目口径：受阻断、诊断-only、默认关闭均**不计满分**；
- 汇总为整体完整度时，按各域对"上游切片+交付契约"这一当前产品边界的权重再加权（协议/输出、配置、几何、材料、测试权重高；产品化外围此刻权重低但缺口大）。

打分只用于沟通"哪里强、哪里弱、差多少"，不作为验收依据。任何验收仍以项目的 CTest / 回归 / RIP strict / 真实模型证据为准。

## 7. 引用与可核对性

- 引用代码：给出 `路径` 或 `路径:行号`（如 `src/slicer_core/pipeline/SlicePipeline.cpp:45`）。
- 引用正式文档：给出 `docs/slice/...` 相对路径与文档内小节。
- 引用阶段状态：优先引最新 REPORT，并标注日期。
- 凡 Claude 建议：显式写"建议/方案（P 级）"，与事实分离。
