# TASKS_12C Qt UI 配置预览工作台任务清单

> 文档版本：v0.2
> 文档状态：Current Task Plan / Stage 12C
> 更新日期：2026-07-10

## 1. 边界

12C 只做 Qt UI 工作台收口：构建、Profile、设置、generated config、统一预览、诊断布局、说明与 smoke。

12C 不做：

```text
不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
不新增切片算法；
不删除 regression fixture；
不默认启用 OpenVDB；
不把 OpenVDB utility/candidate 标为 production-safe；
不提前实现 12D material closure 算法；
不重写已经存在的 preview/report/config panel。
```

执行规则：

```text
1. 每次只执行用户明确指定的一个原子任务；
2. 验证通过后按项目提交模板提交当前原子任务；
3. 提交后停止，不自动进入下一任务；
4. R0 允许修改 CMake/构建脚本/VSCode 配置和最小 compatibility 文件；
5. R1 允许修改 apps/slicer_debug_ui、samples/scenarios、UI config fixture 和用户手册；
6. R2 允许修改 apps/slicer_debug_ui、UI smoke fixture、用户手册和阶段报告；
7. 如果需要修改 slicer_core 公共 API、Qt 依赖版本或第三方 Qt 安装目录，停止并先输出影响分析等待确认。
```

提交要求：

```text
需要 commit；
subject 使用 type(12C): 中文摘要；
正文包含【模块】【验证】【边界】；
必须说明未修改 RGBWSV 协议、未默认启用 OpenVDB。
```

## 2. Task 12C-00 文档准入与现状审查

状态：DONE

输出：

```text
DOC_AUDIT_12C_现有QtUI能力与收口缺口审查.md
DOC_DECISION_12C_R0_R1_R2_Qt工作台阶段拆分.md
DOC_DECISION_12C_UI产品默认值与交互冻结.md
DOC_CHECKLIST_12C_阶段准入与上下文完整性.md
ROADMAP_12C_Qt工作台分阶段执行路线.md
REPORT_12C_Qt工作台启动状态.md
CODEX_PROMPT_12C_Qt工作台收口执行指令.md
ai_workspace/context_handoff/2026-07-10_12B-R2到12C-R0阶段交接.md
```

结论：12C 文档和上下文准备完成，可以进入 R0；R1/R2 需等待 fresh UI build gate。

## 3. Phase 12C-R0 构建与基线

### Task 12C-R0-01 Qt/MSVC Fresh Build Lane

状态：PENDING

内容：

```text
复现 Qt 5.15.2 / MSVC 19.51 stdext 编译错误；
比较 VS2022 toolchain、项目 compatibility shim、Qt patch/LTS 升级三条路线；
输出构建决策；
实现最小可维护修复；
从新 build dir 构建 slicer_debug_ui。
```

完成标准：

```text
fresh configure PASS；
fresh slicer_debug_ui Debug build PASS；
未直接修改本机 Qt 安装目录；
构建入口可在 VSCode/PowerShell 复现。
```

### Task 12C-R0-02 UI Self-Test 与 Smoke 基线

状态：PENDING

验证：

```text
--self-test；
--ui-smoke-test --case scenario-registry；
--ui-smoke-test --case layer-preview-load；
--ui-smoke-test --case overlay-load-real；
配置编辑器既有 smoke。
```

完成标准：所有 case 使用 R0 fresh binary 通过。

### Task 12C-R0-03 布局与组件复用基线

状态：PENDING

内容：记录现有 MainWindow、三个 preview panel、报告、曲线、日志、配置 panel 的职责和复用点；采集 1440x900、1280x720、1024x768 截图或等价几何检查。

完成标准：明确 R1/R2 不重写哪些组件，并记录现有遮挡/溢出问题。

## 4. Phase 12C-R1 Profile 与 Settings

### Task 12C-R1-01 Profile Metadata 收口

状态：PENDING

内容：在 ScenarioRegistry 上增量固化 Profile id/displayName/category/visibility/input formats/material capabilities/production safety/doc path。

完成标准：普通用户默认只看到稳定 Profile；advanced/fixture 仍可显式打开；scenario-registry smoke 更新。

### Task 12C-R1-02 SliceSettingsModel

状态：PENDING

内容：建立 UI 设置 DTO，覆盖模型、输出、层高、模型填充、支撑 placement/internal void、表面/外侧光油、preview 和 engine role。

完成标准：设置状态不依赖单个 QWidget，也不暴露到 slicer_core。

### Task 12C-R1-03 Generated Effective Config

状态：PENDING

内容：实现 `Profile template + UI overrides -> session generated config -> validation -> slicer_cli`。

完成标准：运行切片不再忽略 dirty UI 设置；原始 template/fixture 不被修改；UI 可查看 effective config 摘要和差异。

### Task 12C-R1-04 设置项中文帮助元数据

状态：PENDING

内容：集中提供 title/description/affects/default/productionSafety/docPath，并复用到 tooltip 和说明面板。

完成标准：模型填充、支撑、光油、preview、legacy/OpenVDB 均有一致说明。

## 5. Phase 12C-R2 预览与诊断工作区

### Task 12C-R2-01 PreviewWorkspace 与共享层状态

状态：PENDING

内容：复用 LayerPreviewPanel、PreviewOverlayPanel、PreviewPanel，以模式切换整合入口，并共享真实 layerIndex。

完成标准：生产层检查、材料叠加、原始调试预览切换时保持同层；不跨层兜底 RGB/W/S/V。

### Task 12C-R2-02 图例与像素探针收口

状态：PENDING

内容：统一 RGB/W/S/V 图例、生产值/显示值说明和六通道像素探针上下文。

完成标准：用户可区分真实空白、RGB 模型填充、W/V 模型填充和 S 支撑。

### Task 12C-R2-03 DiagnosticsDock

状态：PENDING

内容：复用 ReportPanel、ChannelChartPanel、LogPanel，移入可折叠诊断区域。

完成标准：报告、曲线、日志不作为主预览顶级入口；小窗口可折叠且无明显遮挡。

### Task 12C-R2-04 OpenVDB Utility/Candidate 摘要

状态：PENDING

内容：读取 `slicesoft.openvdb_sdf_utility.12b_r2.1`，显示 utility role、可用性、promoteDecision、blocker 和 legacy fallback。

完成标准：始终显示 `productionReplacementAllowed=false`；不把 utility PASS 显示为生产切片 PASS。

### Task 12C-R2-05 Smoke、手册与阶段报告

状态：PENDING

内容：补齐 shared-layer、generated-config、diagnostics-collapse、多尺寸布局 smoke；更新用户手册；生成 `REPORT_12C_Qt工作台当前状态.md`。

## 6. 总完成标准

```text
1. fresh Qt UI build 可复现；
2. 普通用户不编辑 fixture JSON 即可完成标准切片；
3. Profile/advanced/fixture 分层明确；
4. 三种预览共享 layerIndex；
5. 报告、曲线、日志不遮挡主工作区；
6. OpenVDB 始终保持 candidate/utility 定位；
7. user guide、smoke 和最终 REPORT 完整。
```
