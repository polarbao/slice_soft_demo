# DOC PREP 13B-08-03 Qt 当前场景切片准备

> 文档状态：IMPLEMENTED / GATE PASS
> 版本：v1.1
> 日期：2026-07-28
> 对应任务：13B-08-03

## 1. 目标

把 Qt 当前 `SceneDocument` 接到已经验证的 `--scene-config` 产品路由，形成：

```text
冻结快照 -> 场景预检 -> 启动 CLI -> 校验结果 -> 自动加载 Package。
```
## 2. 前置 Gate

```text
13B-08-01 批量导入 PASS；
13B-08-02 core/CLI 正负向与 RIP strict PASS；
SceneTransformController 可写入并回读 Effective Config；
ProductionPackageResultValidator 可校验 scene/package/session identity；
现有单模型兼容入口不得被当前场景动作调用。
```

## 3. 控制器状态机

新增 `SceneSliceActionController`：

```text
Idle
-> Snapshotting
-> Preflighting
-> Slicing
-> Validating
-> LoadingResult
-> Completed / Blocked / Failed / Cancelled。
```

每次运行冻结 `sceneId/revision/hash/profile/mode/outputDir`。迟到进程结果与当前冻结身份不一致时标记
stale，不覆盖当前场景状态。

## 4. UI 合同

```text
“切片当前场景”始终可见；
无实例、导入中、预检阻断、运行中等状态均有中文原因；
点击动作不得重新打开模型文件对话框；
运行时提供取消入口；
成功后加载单一 Package 并进入生产 TIFF 预览；
详细错误和进程日志进入现有诊断区。
```

本任务只接通动作，不执行 13D 全窗口重排。

## 5. 失败合同

至少验证：

```text
无可见实例；
批量导入仍在运行；
碰撞/越界；
任一实例 blocked；
Global 未准入；
CLI 启动失败/非零退出；
输出 Package 无效；
运行期间 scene revision 改变；
用户取消。
```

失败不得产生伪成功 UI、不得加载旧包、不得回退到单模型切片。

## 6. 文件所有权

计划新增或修改：

```text
apps/slicer_debug_ui/controllers/SceneSliceActionController.h/.cpp；
apps/slicer_debug_ui/widgets/SceneActionBar.h/.cpp；
apps/slicer_debug_ui/MainWindow.h/.cpp；
apps/slicer_debug_ui/services/UiSmokeTestRunner.h/.cpp；
apps/slicer_debug_ui/CMakeLists.txt；
对应 Qt 单测/Smoke 与报告。
```

## 7. 验收

```text
三模型场景点击一次产生一个 Package；
按钮不打开文件对话框；
有效运行阶段和耗时可见；
成功结果 identity 与冻结快照一致；
stale/cancel/blocked 结果不覆盖当前场景；
生产 TIFF 预览自动加载；
旧单模型、13C-01/02 预览测试不回归。
```

计划验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scene-slice-current
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scene-slice-stale
ctest --test-dir build -C Debug -R "scene_slice|production_package" --output-on-failure
.\scripts\run_ci_quick.ps1
git diff --check
```

## 8. 实施结果

2026-07-28 已完成状态机、主动作、取消、冻结身份校验和 Package 自动回载。
`scene-slice-current/stale/cancel/no-fallback` UI Smoke、Debug 全量构建、
CTest 81/81、Qt self-test 和 Quick CI 均通过。实现证据见
`REPORT_13B_08_03_Qt当前场景切片当前状态.md`。
