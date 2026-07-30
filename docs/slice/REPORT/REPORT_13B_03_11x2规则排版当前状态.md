# REPORT 13B-03 11x2 规则排版当前状态

> 文档状态：COMPLETE
> 日期：2026-07-27
> 实现提交：`aef10c1 feat(13B-03): 建立11x2规则排版与Qt操作闭环`
> 下一任务：13B-04 幅面、碰撞和逐实例准入

## 1. 结论

13B-03 已在 13B-02 的 1..22 实例场景草稿上完成确定性 11x2 规则排版：

```text
1..11 列、1..2 行、最多 22 个实例；
默认列净距与行净距均为 10.00 mm（2026-07-29 产品默认值修订）；
稳定 row_major，隐藏实例继续占位；
变换后 XY AABB 边到边净距；
锁定实例不移动，冲突时整场景 fail-closed；
失败不返回部分 placement，也不部分修改 SceneDocument；
成功时 sceneRevision 只增加一次，实际移动实例 transformRevision 增加一次；
requested/derived/effective transform 和 layout 参数保存、回读一致；
Qt 模型工作区新增中文“排版”页，并可恢复最近一次排版前位置。
```

13B-03 功能 Gate PASS。它只产生可编辑、可序列化的场景排版草稿，不代表多模型生产切片或设备
幅面生产准入已完成。

## 2. 实现内容

### 2.1 无 Qt 排版核心

新增：

```text
src/slicer_core/layout/GridLayoutPolicy.h
src/slicer_core/layout/GridLayoutPolicy.cpp
tests/unit/grid_layout_policy/Main.cpp
```

`ComputeGridLayout` 使用不可变请求计算完整结果，稳定错误覆盖：

```text
LAYOUT_INSTANCE_CAPACITY_EXCEEDED；
LAYOUT_PARAMETER_OUT_OF_RANGE；
LAYOUT_SCENE_REVISION_STALE；
LAYOUT_INSTANCE_BOUNDS_INVALID；
LAYOUT_LOCKED_INSTANCE_CONFLICT；
LAYOUT_INSTANCE_NOT_FOUND。
```

核心不依赖 Qt。旧排版偏移会先从 effective bbox 中移除，再计算新偏移，因此连续重排不会累加漂移。

### 2.2 SceneDocument 原子事务

`SceneDocumentItem` 现区分：

```text
requestedtransform：用户变换；
derivedlayouttransform：规则排版偏移；
instance.transform：最终 effective transform；
layoutrow/layoutcolumn：稳定格位。
```

`ApplyGridLayout` 先完成全部计算，再一次性更新场景；`RestoreGridLayout` 使用排版前快照恢复，不通过
负偏移猜测。模型新增、删除、手工变换、显隐或锁定状态改变后，旧恢复快照失效。

规则排版只增加 XY 平移。Qt 俯视三角形和 world/effective bounds 因此使用同一平移量同步更新；
transformed preflight 状态进入 stale。13B-04 将在此 revision 之上执行逐实例和场景准入。

### 2.3 Scene Effective Config

`SceneTransformController` 已保存：

```text
scene.layout；
instances[].requestedTransform；
instances[].derivedLayoutTransform；
instances[].effectiveTransform。
```

回读继续复用现有 scene hash、revision 和原子事务。`buildVolume=unresolved` 时只允许 draft/save，
不得标记为 production ready。

### 2.4 Qt UI

模型工作区右侧页签现为：

```text
模型列表；
变换；
排版。
```

排版页提供最大列数、最大行数、列间净距、行间净距、执行规则排版和恢复排版前位置。数值单位为
mm，间距步长为 0.01 mm，用户可见文本和 tooltip 均为中文。

## 3. 测试证据

TDD 首次构建在缺少 `GridLayoutPolicy.h` 时按预期 RED；实现后执行：

```powershell
cmake --build build --config Debug --target grid_layout_policy_unit_tests scene_document_unit_tests scene_transform_controller_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(grid_layout_policy_unit_tests|scene_document_unit_tests|scene_transform_controller_unit_tests|model_transform_unit_tests|scene_view_geometry_unit_tests|transformed_model_preflight_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test --repo-root .
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case multi-model-list --repo-root .
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scene-grid-layout --repo-root .
.\scripts\run_ci_quick.ps1
git diff --check
```

结果：

```text
定向 CTest 6/6 PASS；
Qt self-test PASS；
multi-model-list PASS；
scene-grid-layout PASS，覆盖容量失败、11x2、净距、恢复和三种窗口尺寸；
Quick CI PASS；
git diff --check PASS。
```

## 4. 边界与剩余风险

未在 13B-03 实现：

```text
正式设备 buildVolume、原点和机器轴；
投影轮廓或层 mask 精确碰撞；
逐实例 post-transform production admission；
全局 Raster、联合层合成、单 package 和 scene report；
自动 nesting、跨模型联合支撑或 mixed-profile；
修改 p0.rgbwsv.2、RGBWSV、uint8 或 black_is_print。
```

正式设备幅面仍是外部 OPEN INPUT。13B-04 可以先完成显式 fixture buildVolume 下的功能 Gate；
production Gate 必须继续失败关闭，直到设备/Profile 提供正式 width/height/origin/axes。

## 5. 下一入口

```text
docs/slice/DOC/DOC_PREP_13B_04_幅面碰撞与逐实例准入准备.md
docs/codex_task/current/CODEX_PROMPT_13B_04_幅面碰撞与逐实例准入执行指令.md
```

13B-04 只能在 13B-03 排版结果之上增加诊断和准入，不得提前实现 13B-05 联合 Raster 或生产写包。
