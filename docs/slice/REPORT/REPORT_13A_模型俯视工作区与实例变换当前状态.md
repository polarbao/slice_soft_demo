# REPORT 13A 模型俯视工作区与实例变换当前状态

> 文档状态：COMPLETE / M13-1 CANDIDATE PASS
> 日期：2026-07-27
> 收口验证提交：`0a1169e`
> 覆盖任务：13A-01..05
> 下一任务：13B-02 模型列表与实例操作

## 1. 阶段结论

13A 已完成单模型切片前工作区的 P0 范围：不可变源模型、+Z 俯视、选择、X/Y 平移、绕 Z 旋转、
统一缩放、原点居中、重置、X/Y 镜像、session scene/effective config 保存，以及 source/transformed
双预检和 Legacy/Global 独立准入。

M13-1 候选验证通过。被阻断模型仍可查看和调整，但 `PENDING/FAILED/BLOCKED/stale/cancelled` 均
fail-closed。当前 scene effective config 尚未被生产 `slicer_cli` 消费，因此加载可编辑场景后生产按钮
继续禁用，不能把本阶段解释为多模型生产切片已经完成。

## 2. 已实现能力

### 2.1 Core

```text
ModelTransform / ModelInstance；
TransformedModelAdapter；
SceneViewGeometry；
MultiModelScene / ModelSource / ResourceScope 基础合同；
SceneModelRepository；
SceneTransformController；
TransformedModelPreflightService。
```

源 `SceneModel` 使用共享只读缓存。实例变换不修改 OBJ/STL/3MF；奇数轴镜像修正 winding 和 UV 顶点
顺序。sceneRevision、transformRevision、generation、source/resource hash 和 transform hash 构成异步
结果与准入证据的稳定身份。

### 2.2 Qt

```text
独立“导入模型预览”入口；
ModelTopViewWidget +Z 俯视画布；
SceneDocument / SceneSelectionModel；
ModelTopViewLoader；
ModelTransformPanel；
TransformedModelPreflightLoader。
```

模型页显示毫米网格、+X/+Y、包围盒、选择和预检状态。精确数值、镜像、居中和重置均异步刷新几何；
只有最新 generation/revision 可以提交。源模型、变换后模型、Legacy 和 Global 状态分别显示。

### 2.3 Session 配置

单实例场景可保存：

```text
scene_config.draft.json；
scene_config.effective.json。
```

保存包含 scene/model/instance identity、requested/effective transform、revision 和 hash，并执行原子写入、
回读和失败回滚。该配置当前是场景草稿/诊断合同，不是生产写包入口。

## 3. 收口验证

### 3.1 自动化结果

```text
Debug 目标构建：PASS；
13A 定向 CTest：6/6 PASS；
Qt --self-test：PASS；
model-top-view：PASS；
model-top-view-transform：PASS；
model-transform-preflight：PASS；
scripts/run_ci_quick.ps1：PASS；
git diff --check：PASS。
```

`model-transform-preflight` 已补齐 1280x720、1440x900、1920x1080 三种窗口尺寸，确认变换面板不遮挡
俯视画布，blocked 模型保持可见。

### 3.2 真实资产

| 资产 | 作用 | 结果 |
|---|---|---|
| `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj` | strict-PASS 彩色 OBJ 正向 | source/transformed mirror PASS |
| `model/obj/yecan/3.obj` | 独立 OBJ 模型族正向 | source/transformed mirror PASS |
| `samples/models/3mf/texture2d_checker_cube.3mf` | Texture2D/资源/UV 正向 | source/transformed mirror PASS |
| open triangle fixture | 开放拓扑反向 | Legacy warning / Global blocked，仍可查看 |
| missing texture fixture | 资源反向 | Legacy/Global 均 blocked |

## 4. 需求符合情况

| 13A P0 要求 | 结果 |
|---|---|
| 切片前模型 +Z 俯视 | 已实现 |
| X/Y、rotateZ、uniformScale | 已实现 |
| 原点居中和重置 | 已实现 |
| mirrorX/mirrorY | 已实现 |
| 镜像 winding/UV 保持 | 已实现并回归 |
| 变换后重新预检 | 已实现 |
| Legacy/Global 独立准入 | 已实现 |
| session 保存和回读 | 已实现 |
| blocked 可见、生产阻断 | 已实现 |
| 三窗口 UI Smoke | 已实现 |
| 用户操作说明 | 已更新 |

## 5. 明确未实现

```text
多模型列表、复制、删除、隐藏和锁定；
11x2 规则排版；
碰撞和 buildVolume production gate；
多模型联合切片与单 package；
scene effective config 生产消费；
3D 相机、鼠标 gizmo、Z 编辑和非均匀缩放；
自动 mesh repair；
TIFF 原生统一预览。
```

## 6. 固定边界

```text
Legacy 保持默认；
OpenVDB 保持显式候选且默认关闭；
不允许 silent fallback；
不自动修复 confirmed self-intersection；
不修改 p0.rgbwsv.2；
不修改 R G B W S V、uint8 或 black_is_print；
不把 Legacy PASS/WARNING 推导为 Global PASS。
```

## 7. 下一步

13B-02 可在 M13-1 后开发，目标仅为 1..22 实例的模型列表和场景草稿操作：

```text
添加、复制、删除；
显示/隐藏、锁定/解锁；
列表与画布单选同步；
同源资源共享和不同 modelId 资源隔离；
稳定 identity/revision/order；
第 23 个实例 fail-closed；
场景草稿保存/回读。
```

13B-02 不实现规则排版、碰撞、联合切片或生产 package。
