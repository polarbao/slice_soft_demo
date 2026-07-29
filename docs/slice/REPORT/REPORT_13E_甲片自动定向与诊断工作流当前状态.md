# REPORT 13E 甲片自动定向与诊断工作流当前状态

> 文档版本：v1.0
> 文档状态：COMPLETE / FUNCTIONAL PASS
> 完成日期：2026-07-29
> 插入位置：13D COMPLETE -> 13E COMPLETE -> 12E-10A READY

## 1. 阶段结论

Stage 13E 已完成准备、实现和功能回归：

```text
自动定向候选选择已改为带容差的确定性线性决策；
标准甲片等价姿态稳定选择 rotate_x_90，正面朝场景 +Z；
产品默认 autoOrient.maxHeightMm 已由 6 mm 调整为 9 mm；
右侧 Context Inspector 已提供“预检与诊断”；
底部区域已改名“任务详情”，不再重复承载“诊断”页；
RGBWSV TIFF 协议、Legacy/Global 路由和 OpenVDB 默认状态均未改变。
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

这样既避免非严格弱序比较风险，也保证标准甲片在等价时稳定选择
`rotate_x_90`。

### 2.2 高度语义

`maxHeightMm=9.0` 是自动定向评分目标，不是缩放指令。显式关闭
`autoOrient` 的配置仍保持源姿态；Golden、负向和专项 fixture 的显式历史值
没有被批量替换。

## 3. 真实模型方向证据

| 模型 | maxHeightMm | selectedOrientation | 定向后 Z 高度 | 结论 |
|---|---:|---|---:|---|
| `model/obj/meigui_fudiao/03.obj` | 9 | `rotate_x_90` | 7.34728 mm | 正面朝 +Z |
| `model/obj/meigui_fudiao/04.obj` | 9 | `rotate_x_90` | 6.88910 mm | 正面朝 +Z |
| `MF_Mei_gui_wumingzhi_fx04.obj` | 9 | `rotate_x_90` | 7.98364 mm | 已修复反向 |

三个模型均保持原始物理尺寸，没有通过缩放满足高度。

## 4. UI 信息架构

### 4.1 右侧日常检查

`ContextInspector` 的第五页由“预检”升级为“预检与诊断”，内部包含：

```text
准入：模型预检、状态和重新检查入口；
问题：Package/材料/切片报告中的可操作警告。
```

`warningsDiagnosticView` 只迁移父容器，仍由原有
`ReportPanel::warningsChanged` 信号更新，没有复制第二个诊断实例。

### 4.2 底部任务详情

底部保留兼容 objectName `diagnosticsDock` 和 `diagnosticsTabs`，用户可见名称改为
“任务详情”，页签为：

```text
报告、材料闭环、曲线、材料参数、工艺对比、切片耗时、日志。
```

底部默认折叠。切片开始时只预选“切片耗时”，不再自动展开并压缩中央工作区。

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
| 03/04/目标模型 `--inspect-model` | 均为 `rotate_x_90` |

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
本阶段冻结的是标准甲片源 +Y 正面约定，不保证任意第三方模型的语义正面；
不包含任意角度最佳摆放、AI 正面识别或自动缩放；
未对目标玫瑰模型执行完整生产切片，仅完成真实模型只读方向检查；
生产协议不变，下一任务恢复为 12E-10A 同层最终一致性。
```
