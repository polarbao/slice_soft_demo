# DOC_PREP 13A-02 模型俯视渲染准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-27
> 前置：13A-01、13B-01、12E-09A-02 COMPLETE
> 后续：13A-03、13B-02 WAIT 13A-02

## 1. 任务目标

建立切片前的 +Z 俯视工作区，使用户能够在不启动切片的情况下确认当前模型的 XY 占地、方向、
包围盒、身份和准入状态。

本任务只建立“看得见、选得准、坐标一致”的基础，不实现数值变换编辑。

## 2. 当前代码事实

```text
MainWindow 当前中心区域只有 PreviewWorkspace 和配置页；
现有“一键切片”导入后直接进入生产流程，没有独立模型查看入口；
ModelPreflightController 已提供异步准入状态，但不向 UI 暴露三角网格；
SceneModel/ModelReport 已保存当前自动姿态后的 triangles、bbox 和纹理统计；
13A-01 已实现 ModelInstance、ModelTransform 和 TransformedModelAdapter；
13B-01 已实现 MultiModelScene、ModelSource、ResourceScope 和 scene revision；
09A-02 已实现 single_model/scene/current-instance 的诊断配置身份。
```

因此 13A-02 不应从报告或 TIFF 反推几何，也不应把 `slicer.cpp` 临时对象传给 Qt。

## 3. 本任务范围

```text
无 Qt core SceneViewGeometry DTO 和 builder；
+Z 投影的三角形/轮廓、XY bbox、modelId/instanceId/revision；
Qt ModelTopViewWidget，使用 QPainter；
Camera2D 的毫米到屏幕坐标映射；
+X 向右、+Y 向上，Qt 屏幕 Y 轴只在 view adapter 中翻转；
毫米网格、X/Y 轴、轮廓、包围盒、选择和 blocked 样式；
适应视图和 1:1 毫米比例状态；
独立“导入模型预览”入口，不自动启动切片；
复用当前配置、自动姿态和 preflight，不修改模型源文件；
model-top-view UI smoke。
```

## 4. 非目标

```text
不实现 X/Y、rotateZ、scale、mirror 编辑，它们属于 13A-03/04；
不实现模型复制、删除和 22 实例列表，它们属于 13B-02；
不实现自动排版、碰撞或联合切片；
不引入 VTK、Qt3D 或新的第三方依赖；
不实现透视 3D、轨道相机或 gizmo；
不修改生产 TIFF、Profile、材料策略或 OpenVDB 默认状态；
不在 UI 主线程执行模型加载和高三角投影构建。
```

## 5. 核心合同

建议新增：

```text
src/slicer_core/scene/SceneViewGeometry.h
src/slicer_core/scene/SceneViewGeometry.cpp
```

核心 DTO 至少包含：

```text
sceneId/modelId/instanceId；
sceneRevision/transformRevision；
projected triangles 或 contour segments；
worldBoundsMm；
source/effective bbox；
visible/locked/admissionStatus；
geometryHash/transformHash。
```

builder 输入为已经导入和完成 SourceTransform 的 `SceneModel` 与 `ModelInstance`。core 不接收
`QString/QImage/QPainter`，也不拥有文件对话框。

## 6. Qt 边界

建议新增：

```text
apps/slicer_debug_ui/models/SceneDocument.h
apps/slicer_debug_ui/models/SceneDocument.cpp
apps/slicer_debug_ui/models/SceneSelectionModel.h
apps/slicer_debug_ui/models/SceneSelectionModel.cpp
apps/slicer_debug_ui/services/ModelTopViewLoader.h
apps/slicer_debug_ui/services/ModelTopViewLoader.cpp
apps/slicer_debug_ui/widgets/ModelTopViewWidget.h
apps/slicer_debug_ui/widgets/ModelTopViewWidget.cpp
```

允许按当前代码最小落点调整文件数量，但必须保持：

```text
core DTO 不依赖 Qt；
模型加载/投影构建在 Worker；
只有最新 generation/revision 可更新画布；
窗口关闭、重新导入或配置变化可取消旧结果；
MainWindow 只编排，不承载投影数学；
自定义信号 Sig...，槽 On...，函数指针 connect。
```

## 7. 坐标和显示规则

```text
世界坐标：+X 右、+Y 上、单位 mm；
屏幕坐标：screenX=centerX+(worldX-centerWorldX)*scale；
screenY=centerY-(worldY-centerWorldY)*scale；
fit 使用有效 viewport 减去稳定 padding；
零尺寸 bbox、空网格和非有限坐标 fail-closed；
缩放视图不修改 ModelTransform；
resize、选择和状态文字不得改变 world geometry；
轮廓和 bbox 颜色必须与 blocked/selected 状态可区分。
```

## 8. 首批测试

计划 target：

```text
scene_view_geometry_unit_tests
```

核心必测：

```text
+Z 投影保持 X/Y；
world bbox 与 SceneSummary 一致；
identity transform 不改变投影；
translate/rotate/scale 的既有 13A-01 结果能被只读显示；
空模型、非有限坐标和 stale revision 拒绝；
blocked 模型仍可只读显示；
输入 mesh/scene 不被修改。
```

UI smoke：

```text
--ui-smoke-test --case model-top-view
```

必须覆盖：

```text
未加载、加载中、可见、blocked、失败和取消；
1280x720、1440x900、1920x1080；
最长中文路径不遮挡；
+X/+Y 标签、毫米网格、适应视图；
导入预览不触发 slicer_cli；
现有一键切片入口保持可用。
```

## 9. 验证命令

```powershell
cmake --build build --config Debug --target scene_view_geometry_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(scene_view_geometry_unit_tests|model_transform_unit_tests|multimodel_scene_contract_unit_tests|diagnostic_effective_config_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view
.\scripts\run_ci_quick.ps1
git diff --check
```

## 10. Gate

13A-02 当前没有内部技术阻塞，可以在用户授权后开发。正式设备 buildVolume、22 实例性能预算、
联合切片和 13C TIFF 预览均不阻断本任务。

任务完成后：

```text
13A-03 可进入精确 X/Y/rotateZ/scale 编辑；
13B-02 可在同一 SceneDocument 上增加模型列表与实例操作。
```
