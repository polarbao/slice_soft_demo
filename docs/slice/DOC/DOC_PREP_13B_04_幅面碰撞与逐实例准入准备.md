# DOC_PREP 13B-04 幅面、碰撞与逐实例准入准备

> 文档状态：READY FOR FIXTURE DEVELOPMENT / PRODUCTION INPUT OPEN
> 日期：2026-07-27
> 前置：13B-03 COMPLETE
> 当前任务：13B-04 fixture 级幅面、碰撞和逐实例准入

## 1. 目标

在 13B-03 的 requested/derived/effective transform 与稳定格位上建立无 Qt 场景准入服务，回答：

```text
每个可见实例的 effective bbox 是否在显式 buildVolume 内；
任意两个可见实例是否发生真实 XY 投影重叠；
每个实例的 transformed admission 是否满足场景要求；
失败是否能定位 modelId/instanceId 和稳定错误码；
fixture 与 production buildVolume 是否被严格区分。
```

本任务输出诊断和准入结果，不创建全局 Raster，不切片，不写 TIFF/package。

## 2. 输入与坐标

核心请求包含：

```text
SceneBuildVolume；
SceneValidationPurpose；
sceneId/sceneRevision；
contactEpsilonMm；
稳定有序的 SceneCollisionItem；
每项 ModelInstance、SceneInstanceAdmissionStatus 和 SceneViewGeometry。
```

buildVolume 规则：

```text
LowerLeft：X=[0,width]，Y=[0,height]；
Center：X=[-width/2,width/2]，Y=[-height/2,height/2]；
软件坐标继续使用 +X 右、+Y 上；
FunctionalFixture 只接受 source=Fixture 且 isFixture=true 的显式值；
Production 只接受 source=DeviceProfile 且 isFixture=false 的正式值；
Unresolved、缺 width/height/origin/axis 或非正尺寸一律 fail-closed；
fixture PASS 不得升级为 production PASS。
```

正式设备 width/height/origin/axes 当前仍 OPEN，因此本任务只能完成 fixture 功能 Gate。

## 3. 实例参与规则

```text
visible=true：参与幅面、碰撞和逐实例 admission；
visible=false：保留身份并输出 skipped_hidden，不参与生产占用；
locked：不改变准入语义，只影响 13B-03 排版；
admission=Blocked/Unknown：场景 fail-closed，并携带 instanceId；
geometry 的 sceneId/instanceId/revision/transformRevision 必须与实例一致；
任何 required geometry 缺失或 stale 均 fail-closed。
```

P0 场景要求所有可见实例均为 admitted；不允许用部分实例成功代替整场景成功。

## 4. 两阶段碰撞

第一阶段使用 effective XY AABB：

```text
分离距离大于 contactEpsilonMm -> 无碰撞，不进入精确阶段；
重叠宽度和高度均大于 contactEpsilonMm -> 精确候选；
仅边界接触且不超过 epsilon -> 允许；
无效或非有限 bbox -> INSTANCE_BOUNDS_INVALID。
```

第二阶段使用 `SceneViewGeometry.triangles` 的 XY 投影：

```text
检测三角形边相交和相互包含；
AABB 重叠但投影三角形不相交 -> 允许；
投影三角形相交 -> INSTANCE_OVERLAP_BLOCKED；
任一候选缺少有效投影几何 -> PROJECTION_GEOMETRY_INVALID。
```

13B-04 不使用 bbox 重叠直接冒充精确碰撞，也不在 Qt 层复制几何算法。

## 5. 稳定错误与结果

至少提供：

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

结果至少包含：

```text
sceneId/sourceSceneRevision；
purpose/buildVolume/contactEpsilonMm；
sceneStatus；
per-instance visible/admission/inBounds/collisionIds/errors；
AABB candidate pair count；
exact tested pair count；
collision pair count；
scene bounds；
productionAllowed。
```

诊断可返回多个实例问题，但任何错误都使 `productionAllowed=false`。

## 6. 模块与文件

建议新增：

```text
src/slicer_core/layout/SceneCollisionService.h/.cpp；
tests/unit/scene_collision_admission/Main.cpp。
```

可复用：

```text
MultiModelScene/SceneBuildVolume；
ModelInstance；
SceneViewGeometry；
SceneInstanceAdmissionStatus；
SceneValidationPurpose；
ProductionAdmissionPolicy 的 fail-closed 原则。
```

Qt 只消费结果。若更新 `SceneLayoutPanel`，只能显示状态摘要和稳定中文错误，不得在 UI 层执行碰撞。

## 7. TDD 与验收矩阵

先写失败测试，再实现：

```text
显式 lower-left fixture 内单实例 PASS；
显式 center fixture 内单实例 PASS；
unresolved buildVolume FAIL；
fixture buildVolume 用于 Production FAIL；
越过左/右/上/下边界逐实例 FAIL；
边界内接触 PASS；
AABB 分离不进入精确阶段；
AABB 重叠但三角形投影分离 PASS；
三角形边相交 FAIL；
一个三角形包含另一个 FAIL；
blocked/unknown/stale/missing geometry FAIL；
hidden overlap 不阻断；
错误携带正确 instanceId；
输入失败不修改 SceneDocument/MultiModelScene；
相同输入结果确定。
```

## 8. 验证命令

```powershell
cmake --build build --config Debug --target scene_collision_admission_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(scene_collision_admission_unit_tests|grid_layout_policy_unit_tests|scene_document_unit_tests|scene_transform_controller_unit_tests|transformed_model_preflight_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case scene-grid-layout --repo-root .
.\scripts\run_ci_quick.ps1
git diff --check
```

## 9. 非目标

```text
不虚构正式设备幅面；
不输出 production GO；
不实现层 mask 碰撞、全局 Raster 或联合 layer composer；
不实现联合 TIFF/package/report；
不修改单模型生产入口；
不修改 Legacy/OpenVDB 默认关系；
不修改 RGBWSV/TIFF 协议；
不实现自动 nesting 或碰撞后自动避让。
```

## 10. 准备结论

13B-03 已提供稳定 effective transform、bbox、scene/transform revision 和 Qt 场景草稿。13B-04 的
输入、坐标、fixture/production 分界、参与规则、两阶段碰撞、错误、测试和停止条件已明确，可进入
fixture 功能开发。正式 production acceptance 继续等待设备/Profile 输入。
