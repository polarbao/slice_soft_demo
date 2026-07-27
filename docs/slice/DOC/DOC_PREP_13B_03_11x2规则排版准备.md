# DOC_PREP 13B-03 11x2 规则排版准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-27
> 前置：13B-02 COMPLETE
> 当前任务：13B-03 11x2 确定性规则排版

## 1. 任务目标

在 13B-02 的 1..22 实例场景草稿上增加确定性 `row_major` 网格排版。排版只修改实例的
`translateXmm/translateYmm` 派生值，不改变源模型、Z 落台、旋转、缩放、镜像、材料、切片引擎或
生产协议。

本任务输出可编辑、可撤销、可序列化的排版草稿，不实现碰撞精确检测、幅面生产准入、联合 Raster
或生产 package。

## 2. 固定产品规则

```text
实例上限：22；
最大列数：1..11，默认 11；
最大行数：1..2，默认 2；
列间净距：默认 20.00 mm，UI 步长 0.01 mm；
行间净距：默认 30.00 mm，UI 步长 0.01 mm；
排列顺序：SceneDocument 稳定实例顺序；
填充顺序：row_major，先填满第一行再进入第二行；
间距口径：变换后 XY AABB 的边到边净距；
软件坐标：+X 向右、+Y 向上；
Z：不修改，继续沿用当前落台规则；
锁定实例：保持现有位置，不被规则排版移动；
隐藏实例：保留在场景中，P0 默认参与排版占位，避免显示切换改变稳定布局；
blocked 实例：允许生成诊断草稿，但不得因此获得 production admission。
```

当实例数超过 `maxColumns * maxRows` 时必须失败关闭，不允许部分排版。

## 3. 排版数学

输入为每个实例已经应用 rotateZ、uniformScale 和 mirror 后的 `effectivebboxmm`。排版基于稳定列表
顺序计算每行的格位：

```text
cellWidth[column] = 该列所有参与实例的最大 bbox width；
rowHeight[row] = 该行所有参与实例的最大 bbox height；
columnOrigin[0] = 0；
columnOrigin[c] = columnOrigin[c-1] + cellWidth[c-1] + columnGapMm；
rowOrigin[0] = 0；
rowOrigin[r] = rowOrigin[r-1] + rowHeight[r-1] + rowGapMm；
实例 bbox minX/minY 对齐到对应 cell origin；
derived translate = requested transform + layout offset。
```

不同尺寸模型必须保持相邻 bbox 的实际净距不小于配置值。计算使用 `double`，序列化遵循现有
Scene Effective Config 稳定浮点规则，不得在 UI 层用像素坐标反推毫米值。

锁定实例的处理：

```text
锁定实例位置不变；
其 AABB 作为占用约束进入结果；
若规则格位与锁定实例 AABB 重叠，本任务返回 LayoutLockedInstanceConflict；
不自动移动锁定实例，也不静默跳过冲突。
```

精确轮廓碰撞属于 13B-04；13B-03 只保证规则格位和 AABB 级显式冲突结果。

## 4. 无 Qt 核心设计

建议新增：

```text
src/slicer_core/layout/GridLayoutPolicy.h/.cpp；
src/slicer_core/layout/GridLayoutTypes.h 或等价 DTO；
tests/unit/grid_layout_policy/Main.cpp。
```

公共 DTO 至少包含：

```text
GridLayoutRequest：
  maxcolumns；
  maxrows；
  columngapmm；
  rowgapmm；
  spacingmode=edge_clearance；
  order=row_major；
  expectedscenerevision。

GridLayoutPlacement：
  instanceid；
  row；
  column；
  requestedtransform；
  layoutoffsetxmm/layoutoffsetymm；
  effectivetransform；
  effectivebboxmm。

GridLayoutResult：
  valid；
  errorcode/field/message；
  sourceSceneRevision；
  derivedSceneRevision；
  placements；
  boundsmm。
```

稳定错误至少覆盖：

```text
LAYOUT_INSTANCE_CAPACITY_EXCEEDED；
LAYOUT_PARAMETER_OUT_OF_RANGE；
LAYOUT_SCENE_REVISION_STALE；
LAYOUT_INSTANCE_BOUNDS_INVALID；
LAYOUT_LOCKED_INSTANCE_CONFLICT；
LAYOUT_INSTANCE_NOT_FOUND。
```

Public API 使用 Doxygen；核心模块不得依赖 Qt。

## 5. SceneDocument 与配置事务

排版先对 `SceneDocument::Items()` 建立不可变快照，调用无 Qt policy 完整计算，再一次性提交全部实例
变换：

```text
任何输入或 placement 失败 -> 场景不变；
成功 -> 全部 placement 原子提交，sceneRevision 只增加一次；
每个实际移动实例的 transformRevision 增加一次；
current instance 和稳定实例顺序不变；
完成后所有移动实例的 view/preflight 状态进入 stale，异步重投影按 revision 丢弃旧结果。
```

Scene Effective Config 保存：

```text
layout.requested 保存用户输入；
layout.derived 保存 row/column、offset 和 bounds；
instances[].requestedTransform 保留手工值；
instances[].derivedTransform 记录排版偏移；
instances[].effectiveTransform 用于后续 13B-04/05；
buildVolume unresolved 时仍可保存 scene draft；
不得把 unresolved buildVolume 标记为 production ready。
```

“恢复排版前位置”必须使用一次排版前快照，不能通过负偏移猜测恢复。重新排版后旧快照被新事务替代。

## 6. Qt 交互

新增 `SceneLayoutPanel`，放在模型页右侧页签，不新建独立主窗口。控件：

```text
最大列数：QSpinBox，1..11；
最大行数：QSpinBox，1..2；
列间净距：QDoubleSpinBox，单位 mm，步长 0.01；
行间净距：QDoubleSpinBox，单位 mm，步长 0.01；
执行排版；
恢复排版前位置；
状态摘要。
```

所有用户可见文本为中文；数值控件带 tooltip，说明“边到边净距”。排版成功后俯视画布立即显示新
位置，列表选择不改变。错误显示稳定错误的中文映射，不做 silent fallback。

## 7. 测试

新增 target：

```text
grid_layout_policy_unit_tests
```

核心必测：

```text
1/11/12/22 实例；
第 23 个或容量不足拒绝且无部分 placement；
row_major 稳定顺序；
相同尺寸和不同尺寸 bbox；
列净距 20.00 mm、行净距 30.00 mm；
0.01 mm 自定义间距；
旋转/缩放/镜像后的 bbox；
隐藏实例参与占位；
锁定实例保持位置；
锁定冲突 fail-closed；
stale sceneRevision；
连续执行结果确定；
恢复排版前位置；
保存/回读 requested/derived/effective layout。
```

UI Smoke：

```text
--ui-smoke-test --case scene-grid-layout
```

覆盖 1280x720、1440x900、1920x1080，确认布局设置、模型列表、变换页和俯视画布不重叠。

## 8. 验证命令

```powershell
cmake --build build --config Debug --target grid_layout_policy_unit_tests scene_document_unit_tests scene_transform_controller_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(grid_layout_policy_unit_tests|scene_document_unit_tests|scene_transform_controller_unit_tests|model_transform_unit_tests|scene_view_geometry_unit_tests|transformed_model_preflight_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test --repo-root .
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case multi-model-list
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scene-grid-layout
.\scripts\run_ci_quick.ps1
git diff --check
```

## 9. 非目标与停止条件

非目标：

```text
精确轮廓碰撞；
设备 buildVolume production admission；
自动 nesting；
跨模型支撑；
联合 Raster/TIFF/package；
mixed-profile；
修改 RGBWSV/TIFF 协议。
```

停止条件：

```text
需要改变 Z 落台；
需要在 Qt 层实现排版数学；
需要绕过锁定或 blocked 状态；
排版失败产生部分实例移动；
需要提前实现 13B-04 碰撞/生产准入；
单模型 13A 或 13B-02 回归失败。
```

## 10. 准备结论

13B-02 已提供稳定实例顺序、1..22 上限、锁定/显隐、current instance、俯视多实例显示和多实例
Scene Effective Config。13B-03 的输入、数学、原子提交、UI、错误和验证边界已明确，状态为
`READY FOR DEVELOPMENT`。
