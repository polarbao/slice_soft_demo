# REPORT_12C Qt 工作台当前状态

> 文档状态：COMPLETE / Stage 12C
> 日期：2026-07-15
> 验证基线：`build-12c-ui-r2-final`，Debug，Qt 5.15.2，MSVC 19.51

## 1. 阶段结论

12C 已完成 Qt 调试 UI 从“多个配置文件和分散预览入口”向“Profile + 设置 + 生效配置 + 统一预览 + 可折叠诊断”的增量收口。R0、R1、R2 全部完成，最终 fresh build、完整 UI Smoke 和 CTest 通过。

本阶段没有修改切片算法、生产 package 或固定协议，也没有把 OpenVDB 提升为生产引擎。Legacy 仍是默认生产切片路径。

## 2. R0/R1/R2 完成矩阵

| 阶段 | 内容 | 状态 |
|---|---|---|
| 12C-R0-01 | Qt 5.15.2 / MSVC fresh build lane | COMPLETE |
| 12C-R0-02 | UI self-test 与 Smoke 基线 | COMPLETE |
| 12C-R0-03 | 布局与组件复用审查 | COMPLETE |
| 12C-R1-01 | Profile 元数据与普通/高级/fixture 分层 | COMPLETE |
| 12C-R1-02 | `SliceSettingsModel` | COMPLETE |
| 12C-R1-03 | generated effective config 链路 | COMPLETE |
| 12C-R1-04 | 中文帮助元数据与设置说明 | COMPLETE |
| 12C-R2-01 | `PreviewWorkspace` 与共享真实 `layerIndex` | COMPLETE |
| 12C-R2-02 | RGB/W/S/V 图例与六通道像素探针 | COMPLETE |
| 12C-R2-03 | 底部可折叠 `DiagnosticsDock` | COMPLETE |
| 12C-R2-04 | OpenVDB Utility/Candidate 安全摘要 | COMPLETE |
| 12C-R2-05 | 多尺寸布局、最终 Smoke、手册和报告 | COMPLETE |

## 3. 当前 UI 功能

```text
四个稳定中文 Profile 默认可见，高级/fixture 通过“显示全部场景”显式查看；
支持选择现有 Profile、手动配置、一键导入模型并生成会话配置；
常用设置覆盖模型、输出、层高、模型填充、支撑、内部镂空、表面/外侧光油和 preview；
配置页可查看磁盘差异、设置说明和本次生效配置摘要；
主工作区统一为“预览”“配置”两个入口；
报告、曲线、日志通过“视图 -> 诊断区域”按需展开；
左侧项目区和配置页可滚动，小尺寸窗口不再被长表单强制放大。
```

## 4. 配置生成与生产安全边界

当前运行链路为：

```text
Profile template + 内存 UI override + SliceSettingsState
-> session/slice_config.generated.json
-> SliceSettingsModel / ConfigValidator
-> slicer_cli
```

原模板和回归 fixture 不被覆盖。固定协议继续保持：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

OpenVDB 只允许 utility/candidate/diagnostic 角色，默认关闭，摘要固定要求 `productionReplacementAllowed=false`。本阶段没有开放 OpenVDB 生产 RGBWSV 输出。

## 5. 预览与诊断能力

统一预览工作区复用生产层、材料叠加和原始调试预览三个既有面板，并以真实 `layerIndex` 作为唯一共享层标识。模式切换、缩放和诊断区域展开不会跨层寻找替代图，也不会改变当前层。

常驻图例区分 RGB 模型颜色/填充、W 白墨填充、S 支撑、V 光油/填充和真实空白。生产层像素探针显示 R/G/B/W/S/V、打印通道、材料语义和来源策略；伪彩仅用于显示，不替代生产值判断。

OpenVDB Utility 报告可从 package 自动发现或显式加载独立 JSON。错误 schema、非法输出策略或 `productionReplacementAllowed=true` 会被标为无效报告。

## 6. 构建和 Smoke 证据

最终 fresh lane：

```powershell
.\scripts\Configure12CQtUi.ps1 -BuildDir build-12c-ui-r2-final -Config Debug
.\scripts\Run12CUiClosure.ps1 -BuildDir build-12c-ui-r2-final -Config Debug
ctest --test-dir build-12c-ui-r2-final -C Debug --output-on-failure
git diff --check
```

结果：

```text
fresh configure/build：PASS；
12 项 UI self-test/Smoke：PASS；
workspace-layout-sizes：PASS；
  1440x900 = 320/786/300；
  1280x720 = 320/626/300；
  1024x768 = 305/400/285；
CTest：6/6 PASS；
git diff --check：PASS。
```

`Run12CUiClosure.ps1` 会构建 UI、CLI 和 CTest 目标，重新生成两个 UI fixture，并在任一命令非零退出时立即失败。

## 7. 已知限制

```text
Qt UI 仍是调试工作台，不是生产作业管理软件；
不包含 RIP 半色调、设备通信、喷头 bitstream 和作业队列；
小窗口下左侧项目区和配置页需要滚动查看全部内容；
不提供完整 3D 模型编辑或自动 mesh repair；
OpenVDB 仍是辅助诊断/候选能力，不能替代 Legacy 生产切片；
12D 材料闭环诊断、精确 semantic mask 和 1px repair 尚未实现。
```

## 8. 12D 交接条件

以下条件已满足：

```text
12C-R2-05 完成；
最终状态报告已生成；
fresh Qt lane、完整 Smoke 和 CTest 通过；
12D-R0 的 PRD、DEV、DEMO、schema、fixture matrix、TASKS 和 CODEX_PROMPT 已存在；
12D-R1 可以从 12D-02 MaterialClosureConfig 开始。
```

12D 不应重写 `PreviewWorkspace` 或 `DiagnosticsDock`。后续闭环 UI 展示应复用 `ReportPanel` 和现有诊断区域，业务判断留在 core/report 层。

## 9. 下一阶段建议

进入 12D-R1，按顺序实施：

```text
12D-02 MaterialClosureConfig；
12D-03 MaterialClosureReport 骨架；
12D-04 TIFF inferred candidate 诊断。
```

R1 必须保持“默认只诊断、不修复生产 TIFF”；只有 semantic mask exact 路径才允许在后续阶段形成生产闭环验收证据。
