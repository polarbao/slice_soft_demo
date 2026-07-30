# REPORT 13B-02 模型列表与实例操作当前状态

> 文档状态：COMPLETE
> 日期：2026-07-27
> 对应提交：`2f29425 feat(13B-02): 建立多模型列表与实例操作闭环`
> 下一任务：13B-03 11x2 规则排版

## 1. 目标与结论

13B-02 已把 13A 的单实例 `SceneDocument` 扩展为可编辑的 1..22 实例场景草稿，并完成模型
列表、俯视画布、当前实例变换和 Scene Effective Config 之间的基本闭环。

当前结论：

```text
1..22 个实例可添加、复制、删除、选择、显示/隐藏和锁定/解锁；
第 23 个实例 fail-closed，失败不部分修改场景；
同源复制共享只读 source cache identity，不复制完整源模型资源；
列表、俯视画布和当前实例保持单选同步；
锁定实例可查看和选择，但不能删除或编辑变换；
场景草稿可按稳定实例顺序保存、校验、原子写入和回读；
13B-02 功能 Gate PASS，13B-03 可进入开发。
```

## 2. 实现内容

### 2.1 SceneDocument

`apps/slicer_debug_ui/models/SceneDocument.*` 新增：

```text
有序 SceneDocumentItem 集合；
currentInstanceId；
1..22 实例数量约束；
追加模型事务；
复制、删除、显隐、锁定命令；
expectedSceneRevision 乐观并发校验；
稳定错误码和中文错误信息；
删除当前实例后的确定性相邻项选择；
追加失败时保留原场景和原当前实例。
```

错误至少覆盖：

```text
SceneRevisionStale；
InstanceLimitExceeded；
InstanceIdDuplicate；
InstanceNotFound；
InstanceLocked；
SceneIdentityMismatch。
```

现有 `Instance()`、`Geometry()` 单实例访问接口继续映射到 current instance，保持 13A 调用方兼容。

### 2.2 模型导入与俯视显示

`ModelTopViewLoader` 增加 append-to-scene 请求语义。追加导入仍使用 generation/revision 丢弃规则，
不会在失败时清空已有场景。

`ModelTopViewWidget` 已支持：

```text
绘制全部可见实例；
按全部可见实例的 XY 并集适应视图；
按反向绘制顺序命中；
显示当前选择轮廓；
显示锁定和 blocked 状态；
隐藏实例不参与绘制但保留场景数据。
```

### 2.3 Qt 模型列表

新增 `widgets/ModelListPanel.*`，提供图标按钮和工具提示：

```text
添加；
复制；
删除；
显示/隐藏；
锁定/解锁。
```

列表显示名称、格式、XY 尺寸、实例 ID、准入、显隐和锁定状态。模型页使用“模型列表/变换”页签，
在 1280x720、1440x900 和 1920x1080 下不会用列表挤压俯视画布。

当前“添加”是一次文件选择追加一个模型；用户可连续添加。一次文件对话框多选和整批回滚不是本任务
已实现能力，不得标记为完成。

### 2.4 Scene Effective Config

`SceneTransformController` 已按当前 `SceneDocument` 构建多源、多实例 `MultiModelScene`：

```text
同源复制只生成一个 ModelSource；
不同导入源保持独立 ResourceScope；
instances 保持稳定列表顺序；
sceneId/sceneRevision/transformRevision 可追踪；
保存前校验，临时文件原子替换；
保存后回读并验证 effective config hash。
```

该输出仍是 `scene_profile_only` 场景草稿。buildVolume unresolved 时不得宣称 production ready，
也未接入多模型生产切片。

## 3. 测试与验证

### 3.1 定向构建和单测

```powershell
cmake --build build --config Debug --target slicer_debug_ui scene_document_unit_tests scene_transform_controller_unit_tests
ctest --test-dir build -C Debug -R "^(scene_document_unit_tests|scene_transform_controller_unit_tests|model_transform_unit_tests|scene_view_geometry_unit_tests|transformed_model_preflight_unit_tests)$" --output-on-failure
```

结果：

```text
构建 PASS；
定向 CTest 5/5 PASS；
覆盖 1/11/22、第 23 个拒绝、同源复制、显隐、锁定、删除、stale revision；
覆盖多源多实例保存、回读和稳定顺序。
```

### 3.2 Qt Smoke

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test --repo-root .
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view-transform
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-transform-preflight
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case multi-model-list
```

结果：全部 PASS。`multi-model-list` 覆盖实际异步追加、同源复制、显隐、锁定、删除、选择同步和
三种窗口尺寸。

### 3.3 回归

```powershell
.\scripts\run_ci_quick.ps1
git diff --check
```

结果：

```text
Quick CI PASS；
UI self-test、真实 overlay fixture、Golden 和 Stage 10 输出合同保持 PASS；
git diff --check PASS。
```

## 4. 未实现与风险

本任务明确没有实现：

```text
11x2 规则排版；
buildVolume、碰撞和越界准入；
多模型联合 Raster、联合层合成和生产 package；
一次文件对话框批量多选及整批失败回滚；
22 实例正式内存/耗时生产预算；
自动 nesting、跨模型支撑或 mixed-profile。
```

`SceneDocument` 当前持有 UI 侧场景快照，核心 `MultiModelScene` 仍是保存和后续生产配置的无 Qt
真源。13B-03 必须在现有 API 上增加确定性排版，不得新建第二套平行场景状态。

## 5. 下一 Gate

13B-03 只实现 11x2 确定性规则排版：

```text
row_major；
maxColumns=11；
maxRows=2；
columnGapMm=10.00；
rowGapMm=10.00；
edge_clearance；
UI 步长 0.01 mm；
requested/derived/effective layout 可序列化；
fixture/draft 允许 buildVolume unresolved，production ready 仍 fail-closed。
```

执行入口：

```text
docs/slice/DOC/DOC_PREP_13B_03_11x2规则排版准备.md
docs/codex_task/current/CODEX_PROMPT_13B_03_11x2规则排版执行指令.md
```
