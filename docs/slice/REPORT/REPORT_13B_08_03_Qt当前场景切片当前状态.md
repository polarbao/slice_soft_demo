# REPORT 13B-08-03 Qt 当前场景切片当前状态

> 状态：COMPLETE / GATE PASS
> 日期：2026-07-28
> 下一入口：13B-08-04 真实模型作业流矩阵与阶段收口

## 1. 任务结论

Qt 工作台已经接通显式的当前场景生产动作。用户批量导入并完成排版后，可直接点击
“切片当前场景”，冻结当前 `SceneDocument`，生成场景 Effective Config，调用
`slicer_cli --scene-config`，校验场景身份与单一 RGBWSV Package，并自动进入 TIFF
生产预览。

本任务没有恢复旧单模型入口来绕过场景，也没有开放 Global 多模型生产或修改 TIFF
协议。当前使用 fixture build volume，因此结论仍是 functional PASS，不是设备
production GO。

## 2. 已实现能力

### 2.1 Qt 状态机

新增 `SceneSliceActionController`，状态为：

```text
Idle
-> Snapshotting
-> Preflighting
-> Slicing
-> Validating
-> LoadingResult
-> Completed / Blocked / Failed / Cancelled。
```

控制器冻结 `sceneId/revision/hash/effectiveConfigHash/Profile/mode/outputDir`。进程完成前
场景 revision 变化时，输出被判定为 stale，不会覆盖或回载到当前场景。

### 2.2 主动作和取消

新增 `SceneActionBar`：

```text
“切片当前场景”始终显示；
无实例、导入中、实例 blocked、Global 未准入和运行中均显示中文原因；
运行时可显式取消；
取消、失败或 stale 结果不会自动加载旧 Package。
```

### 2.3 场景快照与结果校验

当前场景动作会：

```text
生成本次 session 专属材料工艺 Effective Config；
写入 SceneDocument、Profile、DPI、层厚、Legacy 模式和输出 Package；
使用显式 --scene-config 启动 CLI；
校验 manifest 与 multimodel_scene_report 的 sceneId/revision/hash；
校验 Package/session/configPath/slice_report/preview_report；
成功后只加载本次单一 Package，并使用 manifest 权威 TIFF 层表。
```

### 2.4 集成缺陷修正

本任务验证中发现并修正两项真实集成问题：

```text
批量导入成功的 Legacy 几何此前仍标记 Unknown，导致场景动作永远不可用；
Debug UI 此前优先选择旧 build-slicesoft CLI，旧程序会忽略 --scene-config，
并错误写入 output/SlicePackage。
```

现在批量导入成功的 Legacy 几何进入 admitted 场景 Gate；工具路径保持运行包同目录
CLI 为第一优先级，在源码构建环境中优先选择当前 `build/<Config>`，再回退
`build-slicesoft/<Config>`，避免 UI 与 CLI 版本错配。

## 3. 稳定失败合同

```text
SCENE_SLICE_SCENE_UNAVAILABLE
SCENE_SLICE_IMPORT_IN_PROGRESS
SCENE_SLICE_INSTANCE_BLOCKED
SCENE_SLICE_PIPELINE_MODE_NOT_ADMITTED
SCENE_SLICE_SNAPSHOT_FAILED
SCENE_SLICE_STALE
SCENE_SLICE_PROCESS_LAUNCH_FAILED
SCENE_SLICE_PROCESS_FAILED
SCENE_SLICE_PACKAGE_INVALID
SCENE_SLICE_CANCELLED
```

Global 多模型模式会在 Qt 动作层阻断，不启动进程，也不回退到 Legacy。

## 4. 自动化覆盖

新增或扩展：

```text
scene_slice_action_controller_unit_tests；
production_package_result_unit_tests 的冻结场景身份校验；
scene-slice-current：三模型一次动作、一个 Package、TIFF 自动回载；
scene-slice-stale：运行中 revision 变化，旧输出拒绝回载；
scene-slice-cancel：终止进程且不加载输出；
scene-slice-no-fallback：Global 阻断且不回退 Legacy。
```

同时复测：

```text
model-top-view；
scene-batch-import-three；
scene-batch-import-partial-failure；
tiff-native-preview-all-materials。
```

## 5. 实际验证

本轮实际运行并通过：

```text
Debug 全量构建；
Debug CTest：81/81 PASS；
Qt --self-test：PASS；
scene-slice-current：PASS；
scene-slice-stale：PASS；
scene-slice-cancel：PASS；
scene-slice-no-fallback：PASS；
model-top-view：PASS；
scene-batch-import-three：PASS；
scene-batch-import-partial-failure：PASS；
tiff-native-preview-all-materials：PASS；
scripts/run_ci_quick.ps1：PASS；
git diff --check：PASS。
```

## 6. 固定边界

```text
schema = p0.rgbwsv.2；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
printValue = 0；
emptyValue = 255；
OpenVDB 默认关闭；
Legacy 仍为默认；
Global 多模型生产未准入且无静默 fallback；
Qt 未进入 slicer_core。
```

## 7. 剩余工作

13B-08-04 已解除顺序等待，可以进入真实资产作业流矩阵和阶段收口。仍需验证
1/3/11/12/22 实例、OBJ/MTL/3MF、部分导入失败、容量/碰撞/越界负向、
Debug/Release、RIP strict 和 Quick CI。

设备 build volume、原点/轴向和 22 实例生产预算仍为 `INPUT_OPEN`。这些输入关闭前，
13B-08 最终报告只能声明 functional PASS。
