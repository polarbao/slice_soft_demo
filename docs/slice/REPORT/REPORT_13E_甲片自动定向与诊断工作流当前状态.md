# REPORT 13E 甲片自动定向与诊断工作流当前状态

> 文档版本：v1.2
> 文档状态：COMPLETE / FUNCTIONAL PASS
> 完成日期：2026-07-30
> 插入位置：13D COMPLETE -> 13E COMPLETE -> 12E-10A READY

## 1. 阶段结论

Stage 13E 已完成准备、实现和功能回归：

```text
自动定向候选选择已改为带容差的确定性线性决策；
标准甲片等价姿态稳定选择正面朝场景 +Z 的候选；
产品默认 autoOrient.maxHeightMm 已由 6 mm 调整为 9 mm；
右侧 Context Inspector 已提供“预检与诊断”；
任务详情已迁移到右侧并与上下文检查器互斥，不再重复承载“诊断”页；
RGBWSV TIFF 协议、Legacy/Global 路由和 OpenVDB 默认状态均未改变。
已平放但长轴沿 X 的甲片会绕 Z 轴四分之一转，使俯视长轴统一沿场景 Y；
已经沿 Y 但尖端位于 -Y 的甲片会绕 Z 轴旋转 180 度，使尖端统一朝 +Y。
```

## 2. 根因与修复

### 2.1 模型反向根因

旧实现直接用 `<` 比较候选高度和占地面积。真实模型
`MF_Mei_gui_wumingzhi_fx04.obj` 的 `rotate_x_90` 与
`rotate_x_minus_90` 理论高度相同，但浮点计算产生约
`8.8817841970012523e-16 mm` 的差异。旧逻辑因此把负角候选判为更优，
使模型正面朝向与同族 `03.obj`、`04.obj` 相反。

新实现采用：

```text
高度容差：1e-9 mm；
占地面积容差：1e-9 mm^2；
固定候选顺序：identity、+X90、-X90、+Y90、-Y90；
容差内保持先出现候选，不使用带容差的 std::min_element comparator。
```

这样既避免非严格弱序比较风险，也保证标准甲片先稳定选择
`rotate_x_90`，再按 Z 正反面和尖端方向追加必要的组合旋转。

### 2.3 Z 正反面与尖端方向

薄型长轴甲片使用中心带与两侧带的 Z 下包络判断凸起外表面是否朝 `+Z`。
模型平放后再比较长轴两端 12% 带区的横向跨度，将更窄的一端统一指向场景 `+Y`。
该规则不按文件名特判，并同时覆盖源模型已平放和直角旋转后平放两条路径。

### 2.2 高度语义

`maxHeightMm=9.0` 是自动定向评分目标，不是缩放指令。显式关闭
`autoOrient` 的配置仍保持源姿态；Golden、负向和专项 fixture 的显式历史值
没有被批量替换。

## 3. 真实模型方向证据

| 模型 | maxHeightMm | selectedOrientation | 定向后 Z 高度 | 结论 |
|---|---:|---|---:|---|
| `model/obj/meigui_fudiao/02.obj` | 9 | `rotate_x_90_rotate_z_180` | 7.27440 mm | 正面 +Z、尖端 +Y |
| `model/obj/meigui_fudiao/03.obj` | 9 | `rotate_x_90_rotate_z_180` | 7.34728 mm | 正面 +Z、尖端 +Y |
| `model/obj/meigui_fudiao/04.obj` | 9 | `rotate_x_90_rotate_z_180` | 6.88910 mm | 正面 +Z、尖端 +Y |
| `MF_Mei_gui_wumingzhi_fx04.obj` | 9 | `rotate_x_90_rotate_z_180` | 7.98364 mm | 正面 +Z、尖端 +Y |

四个模型均保持原始物理尺寸，没有通过缩放满足高度。真实 Qt 批量导入 Smoke
直接读取定向后三角形，断言长轴沿 Y 且高 Y 端横向跨度小于低 Y 端。

### 3.1 Reality 平面朝向证据

Reality 五模型已经满足高度限制，因此不需要改变前后表面姿态；其差异仅是源模型
长轴沿 X。增量规则完成后：

| 模型 | selectedOrientation | rotationDeg | 结论 |
|---|---|---|---|
| `segment_101` | `identity_rotate_z_minus_90` | `[0,0,-90]` | 长轴沿 Y |
| `segment_102` | `identity_rotate_z_minus_90` | `[0,0,-90]` | 长轴沿 Y |
| `segment_103` | `identity_rotate_z_minus_90` | `[0,0,-90]` | 长轴沿 Y |
| `segment_104` | `identity_rotate_z_minus_90` | `[0,0,-90]` | 长轴沿 Y |
| `segment_105` | `identity_rotate_z_minus_90` | `[0,0,-90]` | 长轴沿 Y |

上述五项为 Debug/Release 模型只读检查，不是五模型批量生产切片。`03.obj` 仍保持
`rotate_x_90`，证明已经纵向的标准模型不会被二次旋转。

对 `segment_101` 额外执行一次 Release 生产链验证：

```text
Grid：303 x 614 x 184；
核心计算：2366.3419 ms；
完整写包：6516.322 ms；
TIFF：184 层；
RIP Reader strict：PASS；
协议：p0.rgbwsv.2 / uint8 / black_is_print / R G B W S V。
```

该结果证明俯视预览与生产切片共享同一 SourceTransform；未对其余四个 Reality
模型执行切片。

## 4. UI 信息架构

### 4.1 右侧日常检查

`ContextInspector` 的第五页由“预检”升级为“预检与诊断”，内部包含：

```text
准入：模型预检、状态和重新检查入口；
问题：Package/材料/切片报告中的可操作警告。
```

`warningsDiagnosticView` 只迁移父容器，仍由原有
`ReportPanel::warningsChanged` 信号更新，没有复制第二个诊断实例。

### 4.2 右侧任务详情

任务详情保留兼容 objectName `diagnosticsDock` 和 `diagnosticsTabs`，用户可见名称为
“任务详情”，页签为：

```text
报告、材料闭环、曲线、材料参数、工艺对比、切片耗时、日志。
```

任务详情默认折叠，只允许右侧停靠。打开时替换上下文检查器，关闭后恢复上下文检查器；
切片开始时只预选“切片耗时”，不再自动展开并压缩中央工作区。

## 5. 配置更新

以下产品入口已使用 `maxHeightMm=9.0`：

```text
AutoOrientConfig / AutoOrientReport 默认值；
Qt 一键生成配置；
obj_mtl_texture_rgb_only.json；
obj_mtl_texture_rgb_varnish.json；
obj_mtl_texture_rgb_white_varnish.json；
standard_obj_texture_legacy.json；
relief_nail_white_support.json。
```

## 6. 测试与验证

### 6.1 测试先行证据

实现前新增生成式 OBJ 回归。旧代码实际失败：

```text
FAIL product auto-orient height default is 9 mm
FAIL equivalent X candidates prefer rotate_x_90
```

实现后：

```text
auto_orient_unit_tests: PASS
```

### 6.2 实际运行结果

| 验证 | 结果 |
|---|---|
| `cmake --build build --config Debug --target auto_orient_unit_tests` | PASS |
| `cmake --build build --config Debug --target slicer_cli slicer_debug_ui` | PASS |
| 定向 CTest `auto_orient/model_preflight/multimodel_scene/ui` | 7/7 PASS |
| Qt `--self-test` | PASS |
| `workbench-context-inspector` | PASS |
| `workbench-project-diagnostics` | PASS |
| `diagnostics-collapse` | PASS |
| `workspace-layout-sizes` | 1024x768、1280x720、1440x900 PASS |
| `scripts/run_ci_quick.ps1` | PASS，输出 `CI quick complete.` |
| 04 `--inspect-model` | `rotate_x_90_rotate_z_180` / `[90,0,180]` |
| 生成式横向、纵向和 Z 正反面甲片测试 | PASS |
| Qt 真实玫瑰四模型批量导入 | 02/03/04/MF 均为 +Y 尖端，PASS |
| Reality 五模型 `--inspect-model` | 均为 `identity_rotate_z_minus_90` |

Quick CI 完成了生产 TIFF fixture、schema、Golden、RIP Reader 和真实 Overlay
回归。构建日志存在既有 `LNK4020` 调试 PDB 类型记录警告，但未造成链接或测试失败。

## 7. 修改范围

```text
src/slicer_core/model.cpp
src/slicer_core/model.h
src/slicer_core/config.h
apps/slicer_debug_ui/MainWindow.cpp
apps/slicer_debug_ui/widgets/ContextInspector.*
apps/slicer_debug_ui/widgets/DiagnosticsDock.*
apps/slicer_debug_ui/services/UiSmokeTestRunner.cpp
tests/unit/auto_orient/Main.cpp
CMakeLists.txt
五个产品配置 Profile
Stage 13E 决策、需求、设计、验证、任务和状态文档
```

## 8. 边界与下一步

```text
本阶段冻结的是薄型长轴甲片的几何正面约定，不保证任意第三方模型的语义正面；
平面归一化覆盖长轴 X/Y 两种来源，但不包含任意角度 PCA、AI 正面识别或自动缩放；
未对目标玫瑰四模型执行完整生产切片，仅完成真实模型批量导入和定向后三角形检查；
生产协议不变，下一任务恢复为 12E-10A 同层最终一致性。
```
