# REPORT 13B-04 幅面、碰撞与逐实例准入当前状态

> 状态：FUNCTIONAL FIXTURE COMPLETE / PRODUCTION INPUT OPEN
> 日期：2026-07-27
> 代码提交：`db6a2cf`
> 下一任务：13B-05 全局 Raster 与联合层合成

## 1. 阶段结论

13B-04 已建立无 Qt 的场景准入核心，能够对显式 fixture 幅面执行逐实例越界、准入、修订一致性和
两阶段 XY 投影碰撞检查。相同输入输出稳定，检查过程不修改 `SceneDocument`、`MultiModelScene`
或调用方 DTO。

本阶段完成的是功能 Fixture Gate，不是正式设备生产准入。设备
`buildVolume.width/height/origin/xDirection/yDirection` 仍未提供，因此不能宣称 production GO。

## 2. 实现内容

新增：

```text
src/slicer_core/layout/SceneCollisionService.h；
src/slicer_core/layout/SceneCollisionService.cpp；
tests/unit/scene_collision_admission/Main.cpp；
scene_collision_admission_unit_tests。
```

核心输入：

```text
sceneId/currentSceneRevision/expectedSceneRevision；
SceneValidationPurpose；
SceneBuildVolume；
contactEpsilonMm；
稳定有序的 ModelInstance、SceneInstanceAdmissionStatus、SceneViewGeometry。
```

核心输出：

```text
sceneStatus；
逐实例 visible/skippedHidden/admission/boundsValid/inBounds；
逐实例 collisionIds 和结构化错误；
AABB 候选、精确检测、碰撞对统计；
sceneBounds；
functionalAllowed/productionAllowed。
```

## 3. 幅面与准入规则

已实现：

```text
LowerLeft：X=[0,width]，Y=[0,height]；
Center：X=[-width/2,width/2]，Y=[-height/2,height/2]；
FunctionalFixture 只接受 source=Fixture 且 isFixture=true；
Production 只接受 source=DeviceProfile 且 isFixture=false；
fixture 不得升级为 production PASS；
非有限、非正尺寸、来源与 fixture 标志冲突均 fail-closed；
visible=true 的实例必须是 Admitted；
hidden 实例保留身份，但跳过幅面、几何和碰撞占用；
scene/instance/model/revision/transformRevision/geometry identity 必须一致。
```

## 4. 两阶段碰撞

第一阶段使用 `ModelInstance.effectivebboxmm`：

```text
X 或 Y 重叠不大于 contactEpsilonMm：不进入精确阶段；
X、Y 重叠均大于 contactEpsilonMm：记录 AABB candidate。
```

第二阶段使用 `SceneViewGeometry.triangles`：

```text
以凸多边形裁剪计算三角形交叠面积；
只有正面积交叠才判定冲突；
边界接触不判冲突；
AABB 重叠但三角形投影分离时放行；
交叉和相互包含均阻断。
```

## 5. 稳定错误

```text
SCENE_BUILD_VOLUME_UNDEFINED；
SCENE_BUILD_VOLUME_INVALID；
SCENE_BUILD_VOLUME_FIXTURE_NOT_PRODUCTION；
SCENE_INSTANCE_BOUNDS_INVALID；
SCENE_INSTANCE_OUT_OF_RANGE；
SCENE_INSTANCE_ADMISSION_BLOCKED；
SCENE_PROJECTION_GEOMETRY_INVALID；
SCENE_INSTANCE_OVERLAP_BLOCKED；
SCENE_REVISION_STALE。
```

错误携带 `sceneId/modelId/instanceId/otherInstanceId/field/message`，可直接用于报告和 UI 翻译。

## 6. 验证证据

TDD RED：

```text
scene_collision_admission_unit_tests 首次构建因 SceneCollisionService.h 不存在失败。
```

最终验证：

```text
scene_collision_admission_unit_tests：PASS；
grid_layout_policy_unit_tests：PASS；
scene_document_unit_tests：PASS；
scene_transform_controller_unit_tests：PASS；
transformed_model_preflight_unit_tests：PASS；
scene-grid-layout UI Smoke：PASS；
scripts/run_ci_quick.ps1：PASS；
git diff --check：PASS。
```

单测覆盖：

```text
lower-left/center fixture；
unresolved/invalid/fixture-production buildVolume；
四向越界；
AABB 分离、边界接触、AABB 假阳性；
三角形交叉和包含；
blocked/unknown admission；
缺失、身份错误、scene/transform stale geometry；
无效 bbox；
hidden overlap；
稳定错误名、确定性和输入不变性。
```

## 7. 未完成与边界

```text
未获得正式设备幅面和机器轴输入；
未生成全局 Raster；
未连接 Legacy/Global 的逐实例 layer producer；
未执行联合材料 closure；
未写联合 TIFF、manifest、report 或 package；
未改变单模型生产入口；
未改变 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
未改变 Legacy 默认和 OpenVDB optional/OFF。
```

## 8. 下一步

13B-05 已补齐独立准备文档和执行指令。下一任务先建立引擎无关的 scene raster/layer DTO、共享
grid 和纯内存 `SceneLayerComposer`，再通过显式 Legacy/Global adapter 接入逐实例 writer-ready
layer。13B-05 不写 TIFF/package；13B-06 才负责单一 package 与 scene report。
