# REPORT 13B-08-01 批量导入与主切片入口当前状态

> 状态：COMPLETE / GATE PASS
> 日期：2026-07-28
> 对应任务：13B-08-01
> 下一任务：13B-08-02 场景生产服务与显式 CLI

## 1. 完成结论

Qt 模型工作区已经从单文件导入改为可多选的串行批量导入，并新增始终可见的“切片当前场景”主动作。
本任务没有把该主动作错误接到旧单模型配置；在 13B-08-02/03 完成前，它保持禁用并显示中文原因。

## 2. 实现内容

### 2.1 批量导入控制器

新增 `SceneBatchImportController`：

```text
一次接收 1..remaining 个 OBJ/STL/3MF；
按文件选择顺序串行调用现有 ModelTopViewLoader；
使用 loader generation 拒绝迟到完成通知；
一个模型失败后继续后续模型；
成功模型在取消或部分失败后保留；
批次完成后只执行一次 GridLayout；
输出 selected/imported/failed/cancelled、逐项错误和最终 sceneRevision。
```

### 2.2 容量与身份

```text
scene 总容量固定为 22；
21+2 在首个模型分发前整体拒绝；
同一路径可重复导入为独立 instance；
modelId/instanceId 在现有场景和本批次内保持唯一；
路径规范化为绝对路径，扩展名在启动前统一校验。
```

### 2.3 Loader 与 UI

`ModelTopViewLoadRequest` 增加 `autolayoutoncompletion`。旧单模型调用默认保持 `true`；批量控制器显式
传入 `false`，避免每导入一个模型就重新排版。

Qt 左侧动作区现在包含：

```text
导入模型（可多选）；
取消模型导入；
切片当前场景。
```

模型列表中的添加按钮继续复用 `OnImportModelPreview()`，没有第二套导入实现。批次状态和失败摘要进入
现有状态区/日志区。

## 3. 测试覆盖

新增 `scene_batch_import_controller_unit_tests`，覆盖：

```text
三个模型顺序导入；
批次只执行一次排版；
两成功一失败并继续；
21+2 容量整体阻断；
取消保留已提交实例；
迟到完成不改变终态摘要；
不支持扩展名在分发前阻断。
```

新增 UI Smoke：

```text
scene-batch-import-three；
scene-batch-import-partial-failure；
model-top-view 主入口/取消入口/主切片占位集成。
```

## 4. 验证证据

2026-07-28 实际执行：

```text
cmake -S . -B build                                         PASS
cmake --build build --config Debug                          PASS
ctest --test-dir build -C Debug --output-on-failure         PASS 76/76
scene_batch_import_controller_unit_tests                    PASS
UI Smoke model-top-view                                     PASS
UI Smoke scene-batch-import-three                           PASS
UI Smoke scene-batch-import-partial-failure                 PASS
slicer_debug_ui --self-test                                 PASS
scripts/run_ci_quick.ps1                                    PASS
```

Quick CI 首次运行受工具 124 秒超时终止，未取得失败结论；随后用 600 秒时限完整重跑，292.9 秒结束并
输出 `CI quick complete.`。

## 5. 固定边界

本任务没有：

```text
启用旧“运行切片”消费 SceneDocument；
实现 --scene-config；
把 multi_model_scene_matrix 当产品入口；
修改 TIFF/RIP/manifest 协议；
修改 OpenVDB 默认模式；
执行 13D 全窗口布局重排。
```

协议继续保持：

```text
p0.rgbwsv.2 / R G B W S V / uint8 / black_is_print。
```

## 6. 后续 Gate

`13B-08-01` 已满足进入 `13B-08-02` 的 Gate。下一任务必须先建立无 Qt
`MultiModelProductionService` 和显式 `slicer_cli --scene-config`，通过 core/CLI/RIP 负向测试后，
才允许在 `13B-08-03` 启用“切片当前场景”按钮。
