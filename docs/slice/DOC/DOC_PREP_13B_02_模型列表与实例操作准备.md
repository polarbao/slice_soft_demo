# DOC_PREP 13B-02 模型列表与实例操作准备

> 文档状态：READY BY DEPENDENCY / SCHEDULE AFTER 13A-05
> 日期：2026-07-27
> 前置：13B-01、13A-02 COMPLETE
> 推荐顺序：先完成 13A-03..05，再开始 13B-02

## 1. 任务目标

把 13A 单模型 SceneDocument 扩展为 1..22 个实例的场景草稿，并提供模型列表中的添加、复制、删除、
选择、显示和锁定操作。13B-02 不实现规则排版、碰撞、联合切片或生产写包。

## 2. 核心语义

```text
ModelSource 表示一个源模型和独立 ResourceScope；
ModelInstance 表示一次摆放；
复制实例复用 modelId/只读源资源，但生成新 instanceId；
再次导入同一路径可复用 source hash，但是否复用 modelId 由显式导入策略决定；
列表顺序是场景稳定顺序，不依赖文件系统枚举；
删除最后一个实例后才允许清理未引用 ModelSource；
隐藏不等于删除，锁定不等于隐藏；
隐藏实例不参与显示，但仍保留配置；
锁定实例可选择和查看，禁止变换/删除，解锁需显式操作；
最多 22 个实例，第 23 个 fail-closed。
```

## 3. 状态架构

扩展：

```text
SceneDocument：MultiModelScene、model repository、view snapshots、dirty/stale；
SceneSelectionModel：单选 instanceId，删除后确定性选择相邻项或清空；
ModelTopViewLoader：按 modelId 缓存导入，按 instanceId/revision 构建投影；
ModelTopViewWidget：绘制多个实例并按 instanceId 命中；
ModelListPanel：只通过 SceneDocument command API 修改场景。
```

建议命令：

```text
AddModelSourceCommand；
AddModelInstanceCommand；
DuplicateInstanceCommand；
DeleteInstanceCommand；
SetInstanceVisibilityCommand；
SetInstanceLockedCommand；
SelectInstanceCommand。
```

每个命令携带 expected sceneRevision，先完整校验再原子提交。失败不产生半个 ModelSource、
ResourceScope 或列表项。

## 4. Identity 和资源

ID 必须稳定、可审计且不使用 UI 行号：

```text
modelId：导入事务生成；
instanceId：每次实例创建生成；
resourceScopeId：与 ModelSource 绑定；
displayName：可重复，仅用于显示；
sceneRevision：任何实例集合/顺序/可见/锁定改变后 +1；
transformRevision：复制时新实例从 0 开始。
```

OBJ 的 MTL/贴图只能在自己的目录 scope 解析；3MF 资源只能在自己的 package/part scope 解析。同名
贴图和 MTL 不得跨 modelId 共享。几何/纹理缓存 key 必须包含 source/resource hash，不能只用文件名。

## 5. UI

`ModelListPanel` 每行至少显示：

```text
显示名；
modelId/instanceId 简短标识；
格式；
XY 尺寸；
可见；
锁定；
准入摘要；
加载/错误状态。
```

操作使用图标按钮和工具提示：添加、复制、删除、显示、锁定。列表和画布选择双向同步；任何时候只有
一个 current instance。长中文名称使用 elide 和完整 tooltip，不能扩大侧栏导致画布遮挡。

## 6. 异步和内存

```text
模型导入在 Worker；
同一 modelId 多实例共享只读 SceneModel；
每个实例投影带 generation/revision；
删除/隐藏/关闭窗口取消相关任务；
批量导入失败时逐项报告，但事务策略必须明确：P0 单次选择中的 required 资产任一失败则整批回滚；
不得复制全部纹理像素或三角网格给每个实例；
13B-02 记录 1/11/22 实例 UI 内存，不设置虚构生产预算。
```

## 7. Session 保存

保存复用 13B-01 Scene Effective Config：

```text
subjectType=scene；
sceneId/sceneRevision；
models/resourcescopes/instances；
requested/derived/effective transform；
visible/locked/admission；
scene_profile_only；
buildVolume 可保持 unresolved；
原子写入、回读、hash、取消、stale 和源保护。
```

本任务只保存场景草稿，不把 buildVolume unresolved 标记为 production ready。

## 8. 错误码

至少覆盖：

```text
SCENE_INSTANCE_LIMIT_EXCEEDED；
SCENE_MODEL_IMPORT_FAILED；
SCENE_RESOURCE_SCOPE_INVALID；
SCENE_MODEL_ID_DUPLICATE；
SCENE_INSTANCE_ID_DUPLICATE；
SCENE_INSTANCE_MODEL_REFERENCE_MISSING；
SCENE_INSTANCE_LOCKED；
SCENE_INSTANCE_NOT_FOUND；
SCENE_REVISION_STALE；
SCENE_SAVE_FAILED。
```

优先复用 13B-01 稳定错误，新增错误必须进入单测和中文映射。

## 9. 测试

计划 target：

```text
scene_document_unit_tests
```

必测：

```text
添加不同模型；
同源复制；
删除和未引用源清理；
显示/隐藏；
锁定和显式解锁；
列表/画布选择同步；
1/11/22 正向，第 23 个拒绝；
稳定列表顺序和 ID；
同名 OBJ/MTL/texture scope 隔离；
3MF package scope；
快速导入/删除 stale；
批量失败回滚；
保存/回读/取消/篡改；
单模型 13A 回归。
```

UI Smoke：

```text
--ui-smoke-test --case multi-model-list
```

## 10. 准入结论

13A-02 和 13B-01 已关闭技术依赖，因此 13B-02 在依赖上可开发。单贡献者固定顺序仍建议先完成
13A-03/04/05，使模型列表复用稳定的变换 Panel、Controller 和 post-transform admission，避免两套
SceneDocument 同时演进。开始 13B-02 前再生成独立 CODEX_PROMPT，并以 13A-05 实际 API 校正文档。
