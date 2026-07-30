# REPORT 13A-03 选择与精确变换当前状态

> 文档状态：COMPLETE
> 日期：2026-07-27
> 代码提交：`84bc2f7`
> 下一任务：13A-04 镜像与变换后预检

## 1. 任务结论

13A-03 已在 13A-02 单模型 +Z 俯视工作区上建立精确实例变换闭环。用户选择模型后可编辑
X/Y 平移、绕 Z 旋转和统一缩放，也可执行软件场景原点居中与 identity 重置。所有有效变换通过
`ModelInstance`、scene/transform revision、异步重投影和 session scene/effective config 传递，
不从屏幕坐标反推几何，也不修改源模型、源 Profile 或生产协议。

本任务没有开放镜像、Z 平移、非均匀缩放、鼠标 gizmo、多模型排版或变换后生产预检。加载了可编辑
场景后，“运行切片”保持阻断，避免旧单模型配置绕过尚未完成的 post-transform admission。

## 2. 核心实现

### 2.1 只读源模型仓库

新增 `SceneModelRepository`，以绝对模型路径、source transform identity、source hash 和 resource
hash 组成稳定 cache key，并保存 `shared_ptr<const SceneModel>`。首次导入仍在 Worker 完成；
后续变换只从不可变缓存异步重建 `SceneViewGeometry`，不重复读取 OBJ/STL/3MF。

纹理等相邻资源参与 resource hash。Worker 捕获不可变仓库条目而不是仓库裸指针，避免窗口关闭时的
后台生命周期风险。

### 2.2 SceneDocument 状态

`SceneDocument` 当前持有：

```text
sceneId/sceneRevision；
ModelInstance/transformRevision/locked；
source cache/source hash/resource hash；
最新 SceneViewGeometry；
dirty/geometryStale/error/generation；
scene draft/effective config 路径与 config hash。
```

有效变换同时使 sceneRevision 和 transformRevision 增加 1；等价变换不增加 revision。
重投影期间保留上一份可显示几何但标记 stale，只有 identity、generation 和两个 revision 均匹配的
最新结果可以提交。

### 2.3 变换控制器

新增 `SceneTransformController`，支持：

```text
SetTransform；
CenterAtSceneOrigin；
ResetTransform；
SaveSceneEffectiveConfig。
```

控制器统一校验选择、locked、有限值、正缩放、source cache 和 optimistic revisions。失败返回稳定
错误码；命令失败不修改实例。旋转在 core 合同中归一化到 `[-180, 180)`。

### 2.4 Session 配置事务

保存生成一个单实例 `MultiModelScene`，写入当前 UI session：

```text
scene_config.draft.json；
scene_config.effective.json。
```

保存后立即回读并核对 scene identity/revision/hash。取消、stale、生成失败或回读失败不会把 document
标记为已保存；若目标位置已有文件，失败路径恢复保存前快照。

### 2.5 Qt 精确变换面板

右侧上下文检查器“变换”页提供 `ModelTransformPanel`：

```text
X/Y 位置：-10000.00..10000.00 mm，步长 0.10 mm，可键入 0.01 mm；
平面旋转 Z：-180.00..180.00 度，步长 1 度；
统一缩放：0.0100..100.0000；
应用、原点居中、重置、保存场景配置；
scene/transform revision、locked、未保存和 stale 状态。
```

面板明确说明 Z 高度由自动定向与落台固定，X/Y 倾斜由自动定向负责；短期俯视排版只开放
X/Y 平移和绕 Z 旋转。模型加载完成后通过共享 `SceneSelectionModel` 选择实例，面板不维护
第二份选择状态。

## 3. 验证结果

新增：

```text
scene_transform_controller_unit_tests；
--ui-smoke-test --case model-top-view-transform。
```

覆盖源缓存不变性、X/Y/rotateZ/scale、等价 no-op、center/reset、locked、非法缩放、stale revision、
快速连续编辑仅最新 generation 生效、保存/回读/取消/失败回滚，以及三种窗口尺寸。

实际验证：

```text
scene_transform_controller_unit_tests：PASS；
定向 CTest 5/5：PASS；
Qt --self-test：PASS；
model-top-view：PASS；
model-top-view-transform：PASS；
scripts/run_ci_quick.ps1：PASS；
git diff --check：PASS。
```

## 4. 符合情况

| 13A-03 要求 | 结果 |
|---|---|
| 只读源模型 cache | 已实现 |
| X/Y、rotateZ、uniformScale | 已实现 |
| 软件场景原点居中与重置 | 已实现 |
| locked/非法值/stale fail-closed | 已实现 |
| 异步最新代重投影 | 已实现 |
| scene/transform revision 与 dirty/stale | 已实现 |
| 单实例 scene/effective config 保存回读 | 已实现 |
| 保存取消与失败回滚 | 已实现 |
| Qt 变换面板与 UI Smoke | 已实现 |
| mirrorX/mirrorY | 未实现，属于 13A-04 |
| post-transform preflight | 未实现，属于 13A-04 |
| 变换后直接生产切片 | 当前阻断，等待 13A-04 admission |

## 5. 剩余风险

```text
当前 admissionStatus 仍来自导入上下文，不能代表变换后生产准入；
当前保存的是 scene/effective config 合同，不代表 slicer_cli 已消费该场景；
重投影失败会保留旧几何并标记 stale，生产动作不得使用它；
源模型 confirmed self-intersection 不会被本阶段自动修复；
模型列表、多实例选择和空间索引属于 13B。
```

## 6. 下一步

13A-04 已具备实际 API 前置。下一步只实现：

```text
mirrorX/mirrorY 实例命令；
基于不可变 SceneModel + effective ModelInstance 的无 Qt transformed preflight；
source/transformed 双状态与 Legacy/Global 独立 admission；
generation/revision/cancel fail-closed；
变换后 blocked 仍可查看，但生产动作继续阻断；
model-transform-preflight UI Smoke。
```

不在 13A-04 实现自动修复、多模型排版、联合切片、3D 视口或 TIFF 预览。
