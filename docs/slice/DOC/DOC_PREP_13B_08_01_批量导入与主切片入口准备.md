# DOC PREP 13B-08-01 批量导入与主切片入口准备

> 文档状态：READY / AUTHORIZED
> 版本：v1.1
> 日期：2026-07-28
> 对应任务：13B-08-01

## 1. 本任务只做什么

本任务只建立批量导入控制器和清晰的场景主动作占位，不接通场景生产 CLI：

```text
一次选择多个模型；
按选择顺序串行加载；
结果逐项记录并汇总；
批次结束后一次自动排版；
模型工作区显示“切片当前场景”主按钮；
按钮在 13B-08-02/03 完成前保持明确禁用并解释“场景切片服务尚未接通”。
```

这样可先解决“一次只能导入一个模型”和“完全找不到主动作”的交互问题，同时避免把旧单模型入口
伪装为当前场景切片。

## 2. 当前代码证据

```text
OnImportModelPreview 使用 getOpenFileName；
ModelTopViewLoader 当前适合单请求异步加载；
SceneDocument 支持 1..22 实例和 revision；
GridLayoutPolicy 已支持 11x2；
UpdateActionAvailability 在场景有实例时主动禁用旧 run_slicer_button_；
OnImportModelAndSlice 会重新选择单个文件，不得复用为场景动作。
```

## 3. 设计合同

### 3.1 控制器

新增 `SceneBatchImportController`，状态机为：

```text
Idle -> Validating -> LoadingItem -> CommittingItem -> LoadingItem
     -> ApplyingLayout -> Completed / Cancelled / FailedToStart。
```

每个 item 的普通加载失败不使整个 batch 进入 `FailedToStart`；应记录失败并继续后续 item。

### 3.2 容量和事务

```text
selectedCount <= 22 - currentInstanceCount；
不满足时在首个请求前整体拒绝；
成功 item 以独立 SceneDocument 命令提交；
失败 item 不产生半实例；
批次级自动排版只在队列终止后调用一次；
排版失败时恢复排版前 transform，不删除已导入实例。
```

### 3.3 取消

取消只影响当前/待处理请求。已经成功提交的实例保留，并在摘要中说明。所有迟到回调必须按
`batchId + generation` 拒绝。

### 3.4 UI

```text
“导入模型预览”重命名为“导入模型”；
文件对话框使用 getOpenFileNames；
状态区只显示批次进度摘要，详细失败进入可展开列表/底部日志；
“切片当前场景”作为始终可见的主动作；
旧“导入模型并切片”在模型工作区不再与主动作并列，兼容入口移入高级工具。
```

## 4. 文件所有权

计划新增：

```text
apps/slicer_debug_ui/controllers/SceneBatchImportController.h/.cpp；
apps/slicer_debug_ui/widgets/SceneActionBar.h/.cpp；
tests 或 Qt self-test 对应 batch controller 覆盖。
```

计划修改：

```text
apps/slicer_debug_ui/MainWindow.h/.cpp；
apps/slicer_debug_ui/services/UiSmokeTestRunner.h/.cpp；
apps/slicer_debug_ui/CMakeLists.txt。
```

当前工作树中的 13C-03 `TiffLayerLoadWorker/MaterialPreviewImageAdapter/LayerPreviewPanel` 修改不属于
本任务，实施和提交时必须分离。

## 5. 自动化验收

至少覆盖：

```text
三文件全部成功且顺序稳定；
两成功一失败且后续继续；
21+2 容量整体阻断；
取消后已成功实例保留；
批次只触发一次自动排版；
迟到回调不污染新批次；
主动作存在且禁用原因可读；
原单模型预览能力不回归。
```

计划命令：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scene-batch-import-three
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scene-batch-import-partial-failure
.\scripts\run_ci_quick.ps1
git diff --check
```

## 6. 停止条件

出现以下任一情况停止：

```text
必须绕过 SceneDocument revision 才能导入；
只能通过并发加载维持功能；
失败项会留下半提交实例；
必须启用旧单模型切片按钮冒充场景入口；
需要修改生产 TIFF 协议。
```
