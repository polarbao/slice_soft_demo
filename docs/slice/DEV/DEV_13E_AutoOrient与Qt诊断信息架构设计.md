# DEV 13E AutoOrient 与 Qt 诊断信息架构设计

> 文档版本：v1.0
> 文档状态：READY
> 日期：2026-07-29

## 1. 模块边界

```text
slicer_core/model.cpp
  -> 生成候选、计算几何评分、确定 selectedOrientation

slicer_core/config.h
  -> 产品默认 maxHeightMm

apps/slicer_debug_ui
  -> 右侧预检与诊断、右侧互斥任务详情和生成配置

tests
  -> 生成式 AutoOrient fixture、Qt UI Smoke、真实模型只读回归
```

Qt 不参与候选计算，core 不依赖 Qt。

## 2. 自动定向评分

### 2.1 当前缺陷

当前 `choose_auto_orientation` 在两个候选均不满足高度时直接比较：

```cpp
bbox_height(lhs.bbox) < bbox_height(rhs.bbox)
```

对理论等价的 `rotate_x_90` 与 `rotate_x_minus_90`，三角函数舍入可能产生
`1e-15 mm` 级差异，破坏候选声明顺序对应的稳定语义。

### 2.2 新比较规则

建议常量：

```text
height epsilon = 1e-9 mm；
footprint epsilon = 1e-9 mm^2。
```

选择顺序：

```text
1. 能满足 maxHeightMm 的候选优先；
2. 两者都满足时，占地面积显著更小者优先；
3. 两者都不满足时，高度显著更小者优先；
4. 次级评分只有超过容差才参与；
5. 全部等价时保留候选声明顺序。
```

固定候选顺序：

```text
identity；
rotate_x_90；
rotate_x_minus_90；
rotate_y_90；
rotate_y_minus_90。
```

这使标准长轴甲片的等价 X 旋转稳定选择 `rotate_x_90`。

### 2.3 高度语义

`maxHeightMm=9.0` 只参与姿态评分。不得修改模型 scale。后续可单独增加
`heightLimitSatisfied` 报告字段，但本专项不改变现有报告 schema 的强制字段集合。

## 3. 配置迁移

修改：

```text
AutoOrientConfig::max_height_mm；
AutoOrientReport::max_height_mm；
Qt 生成配置；
普通用户可见生产 Profile。
```

不机械修改以下配置：

```text
Golden fixture；
负向测试；
OpenVDB 专用几何 fixture；
历史协议兼容配置。
```

这些配置的显式数值属于测试输入，不是产品默认值。

## 4. UI 结构

目标组件：

```text
ContextInspector
  -> 场景
  -> 变换
  -> 排版
  -> 切片设置
  -> 预检与诊断
       -> 准入
       -> 问题

TaskDetailsDock（保留 DiagnosticsDock 类名以减少迁移风险）
  -> 报告
  -> 材料闭环
  -> 曲线
  -> 材料参数
  -> 工艺对比
  -> 切片耗时
  -> 日志
```

`warningsDiagnosticView` 只迁移父容器，不复制数据源。既有
`ReportPanel::warningsChanged` 连接继续写入同一实例。

### 4.1 兼容策略

```text
保留 diagnosticsDock / diagnosticsTabs objectName，避免破坏布局状态和自动化查找；
菜单 action objectName 保持 diagnosticsToggleAction；
用户可见标题改为“任务详情”；
只允许右侧停靠，并与 ContextInspector 互斥显示；
Dock 从底部迁移到右侧后，将布局 schema 升级为 2，旧几何状态回退安全默认布局。
```

## 5. 测试设计

### 5.1 AutoOrient 单元测试

运行时生成小型 OBJ，不依赖真实大模型：

```text
原始长轴 Z = 30.371832 mm；
原始厚度 Y = [-3.799789, 4.183847] mm；
默认值测试断言 maxHeightMm = 9.0；
等价候选回归显式使用 maxHeightMm = 6.0，使正负 X 候选同时超限；
断言 selectedOrientation == rotate_x_90；
断言 bbox Z 高度约 7.983636 mm；
重复加载结果完全一致。
```

再以 `maxHeightMm` 大于原始 Z 高度验证 identity 不被破坏，并验证
`autoOrient.enabled=false` 保持 identity。

### 5.2 Qt Smoke

```text
Context Inspector 页为：场景/变换/排版/切片设置/预检与诊断；
warningsDiagnosticView 是 Context Inspector 后代；
任务详情页不包含“诊断”；
任务详情默认折叠，展开/收起不改变真实 layerIndex；
任务详情展开时隐藏 Context Inspector，关闭后恢复；
1280x720、1440x900、1920x1080 不重叠。
```

### 5.3 真实模型回归

对 03、04、MF_Mei_gui_wumingzhi_fx04 只读加载并记录：

```text
selectedOrientation；
bbox；
模型/支撑像素；
生产层序；
RIP strict。
```

## 6. 风险

| 风险 | 控制 |
|---|---|
| 容差改变旧 fixture 候选 | 新单测加稳定优先级；保留显式 autoOrient=false |
| “正面”约定不适用于任意第三方模型 | 本专项只冻结标准甲片 +Y 正面约定，不做文件名特判 |
| UI 迁移产生重复诊断实例 | Smoke 检查唯一 warningsDiagnosticView |
| 大量显式 6 mm fixture 被误改 | 仅更新产品默认与用户 Profile |
| 当前工作树已有未提交 UI 修改 | 只做增量 patch，不覆盖现有耗时/Profile 改动 |

## 7. 文件清单

```text
src/slicer_core/model.cpp
src/slicer_core/model.h
src/slicer_core/config.h
apps/slicer_debug_ui/MainWindow.cpp
apps/slicer_debug_ui/widgets/ContextInspector.*
apps/slicer_debug_ui/widgets/DiagnosticsDock.cpp
apps/slicer_debug_ui/services/UiSmokeTestRunner.cpp
samples/configs/material_process/*.json
samples/configs/relief/relief_nail_white_support.json
samples/configs/obj_standard/standard_obj_texture_legacy.json
tests/unit/auto_orient/*
CMakeLists.txt
```
