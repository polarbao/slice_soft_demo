# REPORT 13A-02 模型俯视渲染当前状态

> 文档状态：COMPLETE
> 日期：2026-07-27
> 代码提交：`cc6e40d`
> 下一任务：13A-03 选择与精确变换

## 1. 任务结论

13A-02 已完成切片前单模型 +Z 俯视工作区。用户可通过 Qt 左侧的“导入模型预览”独立加载
OBJ、STL 或 3MF，在不启动 `slicer_cli`、不创建生产 package 的情况下查看模型 XY 投影、毫米网格、
方向、包围盒、身份和状态。

本任务没有实现变换编辑、多模型列表、自动排版或 3D 视口，也没有修改生产 TIFF、材料策略、
Legacy 默认路径或 OpenVDB 默认状态。

## 2. 核心实现

### 2.1 无 Qt 场景视图合同

新增：

```text
src/slicer_core/scene/SceneViewGeometry.h
src/slicer_core/scene/SceneViewGeometry.cpp
```

`BuildSceneViewGeometry` 接收已经完成 SourceTransform/自动姿态的 `SceneModel` 和一个
`ModelInstance`，通过既有 `TransformedModelAdapter` 应用实例变换，再从 +Z 投影到 XY。

输出保留：

```text
sceneId/modelId/instanceId；
sceneRevision/transformRevision；
projected triangles；
source/effective bbox；
worldBoundsMm；
visible/locked/admissionStatus；
source/textured triangle count 和 material count；
geometryHash/transformHash。
```

空网格、零面积 XY 包围盒、非有限坐标、非法身份和 stale revision 均 fail-closed。core DTO 不依赖
Qt，不修改源 `SceneModel`。

### 2.2 Qt 状态和异步边界

新增：

```text
apps/slicer_debug_ui/models/SceneDocument.*
apps/slicer_debug_ui/models/SceneSelectionModel.*
apps/slicer_debug_ui/services/ModelTopViewLoader.*
```

状态机为：

```text
Unloaded -> Loading -> Ready/Blocked/Failed/Cancelled
```

模型导入、自动姿态和投影构建在 `QThreadPool` Worker 中执行。每次请求分配递增 generation；
重新导入、配置变化、显式取消或窗口销毁会使旧结果失效，只有最新 generation 可以发布到
`SceneDocument`。

当前 `SceneDocument` 只保存一个模型的只读投影快照。13A-03 将扩展实例变换状态，13B-02 再扩展为
多实例列表。

### 2.3 Qt 俯视工作区

新增：

```text
apps/slicer_debug_ui/widgets/ModelTopViewWidget.*
```

中心工作区新增“模型”页和“适应视图”按钮。画布规则为：

```text
世界坐标 +X 向右、+Y 向上，单位 mm；
屏幕 Y 轴显式反向映射；
按模型包围盒保持物理比例适应画布；
显示自适应毫米网格、原点轴、投影、bbox 和身份/revision；
点击先经过 bbox 快筛，再做投影三角形精确命中；
blocked 使用独立红色样式，并显示“仅可查看，不代表生产准入”；
超过 100000 个三角形时仅绘制确定性 LOD，core 仍保留完整投影。
```

左侧新增“导入模型预览”。它使用当前配置中的 transform/autoOrient 作为导入上下文，但在 Worker
中覆盖所选模型路径，不修改源配置文件，不启动切片进程。

## 3. 自动化验证

新增 target：

```text
scene_view_geometry_unit_tests
```

核心单测覆盖：

```text
稳定错误码；
+Z 投影保持 X/Y；
scene/model/instance identity 和 revision；
source/effective bbox；
texture/material display hints；
translate/rotate/scale 既有结果；
stale scene/transform revision；
空、非有限和零宽几何；
blocked/locked 只读显示；
源模型不被修改。
```

新增 UI Smoke：

```text
--ui-smoke-test --case model-top-view
```

Smoke 覆盖未加载、加载、可见、blocked、失败、取消、最新 generation、长中文路径、精确选择，以及
1280x720、1440x900、1920x1080 三种尺寸的非空渲染。

实际结果：

```text
scene_view_geometry_unit_tests：PASS；
定向 CTest 4/4：PASS；
Qt --self-test：PASS；
model-top-view UI Smoke：PASS；
Debug 全量构建：PASS；
scripts/run_ci_quick.ps1：PASS；
git diff --check：PASS。
```

## 4. 与需求符合情况

| 要求 | 结果 |
|---|---|
| 导入 OBJ/STL/3MF 后可查看 | 已实现 |
| +Z 俯视、+X 右、+Y 上 | 已实现 |
| 毫米网格、bbox、适应视图 | 已实现 |
| 模型身份和 revision | 已实现 |
| 选择状态 | 已实现单模型精确命中 |
| blocked 仍可查看但不得伪装 PASS | 已实现显示合同和 Smoke |
| 不在 UI 线程加载大模型 | 已实现 |
| 预览入口不启动切片 | 已实现 |
| X/Y、rotateZ、scale 编辑 | 未实现，属于 13A-03 |
| mirror 和 post-transform preflight | 未实现，属于 13A-04 |
| 多模型列表 | 未实现，属于 13B-02 |

## 5. 剩余风险

```text
当前实际导入预览的 admissionStatus 默认为 Unknown；它不会显示为生产 PASS。
把当前实例变换后准入结果回写到画布属于 13A-04。
当前投影精确命中为线性扫描；单模型可用，多实例空间索引属于 13B-02/04。
当前 QPainter LOD 只解决首版显示，不替代 13A-R2 的 3D 后端 Spike。
当前 SceneDocument 未保存可编辑 ModelInstance 和 session scene draft，必须在 13A-03 补齐。
```

## 6. 下一步

13A-03 已具备开发前置。下一步只实现：

```text
单模型选择与 X/Y、rotateZ、uniformScale 精确编辑；
居中、重置；
ModelInstance/scene revision；
异步重投影；
单实例 session scene/effective config 保存和回读；
locked、stale 和非法输入 fail-closed。
```

不在 13A-03 提前实现 mirror、post-transform preflight、多模型列表、自动排版或联合切片。
