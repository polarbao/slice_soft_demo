# CLAUDE_07 事实校验记录

> 目录位置：`docs/claude/VERIFICATION/`。本篇记录本轮分析中对关键断言的**只读**核验动作与结果，便于后续核对与追溯。
> 证据等级：A=代码事实，P=Claude 判断。校验日期：2026-07-22。

## 1. 校验目的

本集（CLAUDE_00–06）大量引用代码路径、行号、协议常量、阶段状态。为避免"把目标态当现状"或"引用失真"，本篇集中记录哪些断言经过**直接核验**、用什么方法、结果如何，以及哪些断言**未独立复核**（需项目方以真实构建/CTest 复核）。

## 2. 已直接核验的断言（A）

| 断言 | 核验方法 | 结果 | 采信文档 |
|---|---|---|---|
| 概念管线 14 步仅为字符串 | 读 `src/slicer_core/pipeline/SlicePipeline.cpp` | 确认 `DefaultSlicePipelineSteps()` 返回 14 个字符串 | 02 §3、05 §7 |
| `RunSlicePipelineLegacy` 经预检门后整体 `run_slicer()` | 同上，定位到**第 45 行** `result = run_slicer(configPath, options);` | 确认转调 | 02 §3、00 §5 |
| `config.h` 无 `slicePipeline.mode` 字段 | 通读 `src/slicer_core/config.h`（`SliceConfig` 全部成员）| 确认无该字段，双模式=目标态 | 03 §1.5、09/教程一致 |
| 协议默认值 | 读 `config.h`：`OutputConfig` | `dpi 600/600`、`layer_thickness_mm{0.01}`、`channel_order{"R","G","B","W","S","V"}`、`bit_depth{8}`、`background.value{255}`、`storage_mode "stripped"`、`rows_per_strip{64}`、`tile_size{256,256}` | 00 §5 |
| 5 处材料意图并存 | 读 `config.h`：`SliceConfig` 成员 | 确认 `material` / `material_policy` / `model_fill` / `material_process_profile` / `material_role_mapping` 并存 | 02 §6.3、03 D-09、05 §1 |
| 红线与协议常量 | 读 `.agents/AGENTS.md §4/§5/§6` | 确认 `p0.rgbwsv.2` / `R G B W S V` / `uint8` / `black_is_print` / `printValue=0`,`emptyValue=255` / `Model>Support>Empty` / OpenVDB 默认 OFF / 禁止静默回退 / confirmed self-intersection fail fast | 00 §5、02 §2/§4、04 §1 |
| 进度令牌 `SLICE_PROGRESS` 双处硬编码 | grep `apps/` | 命中 `slicer_cli/main.cpp:289`、`slicer_debug_ui/services/SliceProgressProtocolParser.cpp:9`（另测试 `UiSmokeTestRunner.cpp:2234`）| 03 D-05、02 §6.1 |
| RGBWSV 通道数/顺序重复定义 | grep `tiff_io.h`、`rip_reader.h` | `tiff_io.h:11 constexpr int rgbwsv_channel_count{6}`；`rip_reader.h:62 std::array<std::string,6>{"R","G","B","W","S","V"}` 独立硬编码 | 03 D-06（已据此把行号从初稿的 54/62-67 修正为 62）|
| `slicer_core` 对 Qt 零依赖 | 结构化清单：grep Qt 符号于 `src/slicer_core` | 0 命中，边界被遵守 | 02 §2 |

## 3. 依据结构化清单/正式文档采信（A/B，未逐行手验）

以下断言来自源码结构化清单（行计数工具，误差约 ±1）或正式文档，未逐行手验，采信时保留量级口径：

| 断言 | 来源 | 说明 |
|---|---|---|
| `slicer.cpp≈4830`、`model.cpp≈1662`、`config.cpp≈1030`、`UiSmokeTestRunner.cpp≈2618`、`MainWindow.cpp≈1611` | 行计数清单 | 用于"god file"判断，量级可靠，±1 |
| `slicer_core` 约 197 源文件、约 37 单测目标、约 49 脚本 | 清单 | 用于工程形态描述，取约数 |
| CMake `USE_OPENVDB` 默认 OFF、C++20、vcpkg(json/tiff/assimp+openvdb feature) | 读根 `CMakeLists.txt`/`vcpkg.json` 清单 | 关键构建事实 |
| 阶段状态（12A–12D 完成、12E 进行中、R3-04 NO-GO、12E-08D BLOCKED）| `.agents/AGENTS.md §6` + `docs/slice/REPORT` 最新报告 | B/A 混合，以最新 REPORT 为准 |
| 三必需 OBJ 拓扑数据、R3-02 自交对数、资产预检 7/1/7 | `REPORT_12E_启动准备状态.md`、`REPORT_12E_08C_R4_模型资产预检清单.md` | 引用报告原值 |
| 性能热点（support≈2801.9ms、layerCompose≈1595.7ms）| `PRD_12F` 历史 Release 剖面 | 历史剖面值，非本轮实测 |

## 4. 语言基线校验（P）

- 对 `docs/claude/*.md` 执行英文短语扫描，确认文档为**中文为基础**，英文仅限：专有名词/标识符、代码块、项目自有状态词（`BLOCKED`/`NO-GO`/`diagnostic`/`admitted` 等）、`.agents §7` 强制模板表头。
- 本轮据此将两处 Claude 自引入的描述性英文标签中文化：`Admission Gate → 准入门（拓扑/修复/预算/闭包）`、`Shared Output Stack → 共享输出栈`（均在 CLAUDE_02 架构图）。

## 5. 未复核项 / 待项目方验证（P）

以下内容本轮**未运行**，需项目方以真实构建/CTest/回归复核后再采信：

1. 所有 `ctest` / `run_ci_quick.ps1` / `run_material_closure_tests.ps1` 等**未在本轮执行**（分析为纯只读，且本会话沙箱不可用）；文中出现的验证门均为**建议应跑**，非"已跑通"。
2. 行数、文件数、脚本数为工具计数近似，精确值以实际仓库为准。
3. 阶段状态可能随分支/未提交改动变化；推进前须 `git branch --show-current` + `git status --short` 复核。
4. 完整度百分比（01/03）为 P 级主观打分，非验收结论。

## 6. 结论（P）

本集的**结构性事实**（管线未拆、单体规模、协议常量重复、双模式未落地、五处材料意图、红线清单）均经直接读码核验，可信度高；**阶段状态与性能数值**引自最新正式报告与 PRD，属"引用可信、需现场复跑确认"。据本集推进开发时，请以"代码 + 现场 CTest/回归"为最终裁决，并按各文档标注的验证门执行。
