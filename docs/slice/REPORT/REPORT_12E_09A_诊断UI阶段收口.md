# REPORT 12E-09A 诊断 UI 阶段收口

> 状态：12E-09A-01..06 COMPLETE / PASS
> 日期：2026-07-29
> 验证环境：Windows x64 / MSVC / Qt 5.15 / Debug / OpenVDB 默认关闭

## 1. 阶段目标

12E-09A 在不改变生产切片协议的前提下，建立当前模型或当前场景实例的纹理表面层与模型填充层诊断能力：

```text
只读 Diagnostic Facade；
single_model / scene 身份绑定的 Diagnostic Effective Config；
中文纹理宽度和填充材料控件；
可取消、可防陈旧结果的异步 Worker；
基于生产 TIFF 真源的同层 Texture / Fill / Partition 语义预览；
统一回归、用户说明和上下文收口。
```

## 2. 原子任务结果

| 任务 | 状态 | 主要成果 |
|---|---|---|
| 09A-01 | COMPLETE | 只读消费诊断报告，保留 pending、unavailable、blocked 和未评估语义 |
| 09A-02 | COMPLETE | 原子保存 Diagnostic Effective Config，并绑定 scene/instance/revision 身份 |
| 09A-03 | COMPLETE | 中文 0.01 mm 宽度控件、填充材料选择、边界和后端状态 |
| 09A-04 | COMPLETE | 后台分析、取消、失败、重入、关闭安全和 stale 隔离 |
| 09A-05 | COMPLETE | 同一 `layerIndex/zMm` 上叠加生产 TIFF 与 Texture/Fill/Partition 诊断语义 |
| 09A-06 | COMPLETE | 构建、全量测试、窗口/字体 Smoke、Quick CI、手册和状态索引收口 |

## 3. 本次收口修正

`production-mode-selector` Smoke 曾固定切换到工作区索引 `1`。Stage 13D 收口后中央工作区顺序为
“模型 / 预览 / 配置”，索引 `1` 已是预览页，导致生产模式面板不可见，并被误报为 1280x720 中文
截断。测试现改为按稳定对象 `configEditorScrollArea` 切换配置页，同时在失败信息中输出窗口、面板和
下拉框实际尺寸。

该修正只提高 UI 回归的身份稳定性，不修改产品布局或切片业务。

## 4. 当前使用边界

```text
右侧“切片设置 -> 纹理与填充诊断试算”只影响诊断请求；
诊断宽度范围为 0.10..6.00 mm，步长 0.01 mm；
诊断填充材料支持 white / varnish / rgb；
生产 Profile、生产 Effective Config 和生产 TIFF 不会被诊断控件修改；
诊断失败时最大宽度和全纹理阈值显示“未评估”，不会伪造 0；
stale、blocked、not_evaluated 不显示为 PASS；
同层预览不允许跨 layer、跨实例或跨 sceneRevision 兜底。
```

## 5. 验证证据

本次实际运行并通过：

```text
cmake --build build --config Debug --target slicer_debug_ui
ctest --test-dir build -C Debug -R
  "(tiff_layer_source|texture_fill_partition_(semantic_preview|diagnostic_composer|diagnostic_facade)|
    diagnostic_effective_config|diagnostic_analysis_worker)_unit_tests" --output-on-failure
  -> 6/6 PASS

diagnostic-settings-controls
diagnostic-semantic-preview
preview-workspace-shared-layer
tiff-native-preview-all-materials
preview-legend-probe-context
preview-physical-aspect
workspace-layout-sizes
production-mode-selector
  -> 全部 PASS

QT_SCALE_FACTOR=1.5：
diagnostic-settings-controls、production-mode-selector
  -> PASS

slicer_debug_ui.exe --self-test
  -> startup / experimental-report-summary PASS

generated-effective-config
  -> PASS，OpenVDB enabled=false、writeProductionRgbwsv=false

cmake --build build --config Debug
  -> PASS

ctest --test-dir build -C Debug --output-on-failure
  -> 84/84 PASS

scripts/run_ci_quick.ps1
  -> PASS，包含 Debug build、quick regression、schema/golden、UI self-test 和 overlay-load-real
```

真实生产包验证使用：

```text
output/ui_sessions/surface_shell_cube_custom_scene_legacy_20260729_121251_654/package
```

完整 RGB/W/S/V 图例与探针 Smoke 使用 Quick CI 生成的：

```text
output/UiSmokeOverlayRgbwv
```

## 6. 协议与安全边界

本阶段没有修改：

```text
schema = p0.rgbwsv.2；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
printValue = 0；
emptyValue = 255；
Legacy 默认生产路径；
OpenVDB 默认关闭和显式 opt-in；
禁止 silent fallback。
```

## 7. 未关闭事项

```text
aishen / meigui / titian 复杂浮雕仍是 0/3 strict-PASS 覆盖缺口；
Global Surface Shell 仍显著慢于 Legacy，并占用更多峰值内存；
设备 buildVolume、原点/轴向和 22 实例生产预算仍是外部输入；
12E-10A..D 只完成执行文档准备，尚未实现。
```

## 8. 下一阶段

12E-09A 已完成。`12E-10A` 的技术和文档前置现已解除，状态更新为 `READY`。按照执行合同，
本报告完成后不自动实施 12E-10A，等待用户明确授权。
