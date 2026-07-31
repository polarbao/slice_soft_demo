# REPORT_13G 支撑投影铺底与层间连续性当前状态

> 状态：13G-00..07、13G-R1 COMPLETE / FUNCTIONAL PASS
> 版本：v1.2
> 日期：2026-07-30

## 1. 已完成

```text
建立 Reality 五模型 Z 下包络证据；
确认 layerIndex 仍为低 Z 到高 Z；
完成薄壳甲片 front-up 自动定向和 Z=0 落台修正；
Reality 五模型只读检查均选择 rotate_x_180_rotate_z_minus_90；
完成 segment_105 正确姿态支撑连续性复测；
新增 support.baseProjection Config/DTO/CLI/Qt validator；
冻结历史配置缺省 enabled=false，生产 UI 默认 true/30；
新增最大普通支撑 footprint 和 layerIndex [0,N) 铺底；
Legacy 与 Global 材料证据链均消费同一配置；
新增 ProjectionBase、逐层统计和 support_report.baseProjection；
Qt 常用配置和支撑编辑器提供启停及层数控件；
新增生成式 fixture、自动化脚本和 RIP strict 验证；
完成 segment_105 生产参数 Release 单模型复测。
完成 13G-R1 语义修正：生产 UI 铺底改为 prepend_below_model，实际新增 N 个
模型下方物理 TIFF 层；历史 overlay_existing 继续兼容。
```

## 2. 当前结论

```text
Reality 五模型源姿态均为中心低、两侧高，低层先出现中间区域的主因是 Z 正反面错误；
正确姿态下 S 支撑从 layerIndex 0 连续到 93，原第 21 层中断已消失；
当前 internalVoid 只实现 layer_enclosed_2d，不是三维空腔或跨层承托体；
支撑最大投影铺底是独立工艺能力，不能用于掩盖错误摆放；
prepend_below_model 的 layerCount=30 精确新增 layerIndex 0..29，原模型整体后移 30 层；
overlay_existing 的 layerCount=30 仍只覆盖既有 layerIndex 0..29，不增加 TIFF；
材料优先级仍为 Model > OuterVarnishShell > Support > Empty；
协议仍为 p0.rgbwsv.2、RGBWSV、uint8、black_is_print。
```

## 3. 实现模块

```text
src/slicer_core/support/SupportBaseProjection.*：最大 footprint 和前 N 层应用；
src/slicer_core/slicer.cpp：Legacy SupportType、统计、report 和材料优先级；
src/slicer_core/pipeline/GlobalSurfaceShellMaterialEvidence.cpp：Global 证据链接入；
src/slicer_core/config.*：baseProjection 配置解析和校验；
apps/slicer_debug_ui：Qt 控件、默认值、Effective Config 和帮助文本；
samples/models/support/base_projection_steps.obj：生成式支撑阶梯 fixture；
scripts/run_13g_support_base_projection_tests.ps1：协议、边界和 RIP 自动验收。
```

## 4. 真实模型结果

以下 Release Package 是 13G v1.1 的 `overlay_existing` 历史验证证据：

```text
output/13g_segment105_base_projection_release_20260730/package
```

| 指标 | 结果 |
|---|---:|
| 定向 | `rotate_x_180_rotate_z_minus_90` |
| 网格 | 175 x 395 x 107 |
| Z 范围 | 0..4.0404 mm |
| 有效铺底范围 | 0..29 |
| footprint | 55791 px |
| projection_base | 5533 px |
| 总 S | 3529089 px |
| S 连续层 | 0..93 |
| Release totalMs | 986.358 |
| RIP strict | PASS |

13G-R1 新增物理层 fixture：

```text
output/SupportBaseProjectionPrepend30Layers
```

| 指标 | 结果 |
|---|---:|
| 原模型层数 | 16 |
| 新增铺底层数 | 30 |
| 输出总层数 | 46 |
| layerPlacement | `prepend_below_model` |
| modelLiftMm | 1.5 mm（fixture 层高 0.05 mm） |
| projection_base | 34560 px |
| RIP strict | PASS |

关键层：

| layerIndex | model | S | bottom_projection | projection_base | internal_void |
|---:|---:|---:|---:|---:|---:|
| 20 | 3695 | 52485 | 52242 | 243 | 0 |
| 21 | 3748 | 52432 | 52138 | 294 | 0 |
| 29 | 4341 | 51826 | 51103 | 723 | 0 |
| 30 | 4429 | 50950 | 50950 | 0 | 0 |
| 90 | 17485 | 4416 | 45 | 0 | 4371 |

## 5. 当前验证证据

```text
Debug auto_orient_unit_tests: PASS
Debug experimental_config_unit_tests: PASS
Debug support_shape_unit_tests: PASS
Debug global_surface_shell_material_evidence_unit_tests: PASS
Debug production_effective_config_unit_tests: PASS
Qt setting-help-metadata smoke: PASS
Qt generated-effective-config smoke: PASS
scripts/run_13g_support_base_projection_tests.ps1: PASS
prepend fixture: 16 + 30 = 46 TIFF，projection_base > 0，RIP strict PASS
Reality --inspect-model: 5/5 rotate_x_180_rotate_z_minus_90
segment_105 Release/RIP: PASS
git diff --check: PASS
```

## 6. 保留边界

```text
internalVoid 仍是逐层二维闭合空洞，不是三维腔体追踪；
baseProjection 只负责模型下方最大投影铺底，不能修复错误摆放；
Global 生产能力锁尚未单独准入 prepend_below_model，当前新增物理铺底以 Legacy 为正式入口；
Legacy 仍为默认生产引擎，OpenVDB 仍为显式候选；
未进行真实打印机、墨路、剥离力或 30 层工艺参数的物理认证；
若正确姿态下仍出现非铺底区支撑断层，应另立跨层 cavity/support continuity Gate。
```
