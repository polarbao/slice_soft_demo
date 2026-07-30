# DOC_DECISION 13B-04A 多模型纹理俯视统一展示

> 文档状态：IMPLEMENTED
> 日期：2026-07-27
> 插入位置：13B-04 完成后、13B-05 开始前

## 1. 问题

原俯视画布虽然遍历 `SceneDocument::Items()`，但新导入实例默认落在相同 XY 原点，因此多个模型会
完全重叠，视觉上近似只显示一个模型。画布同时只使用固定青绿色填充，`SceneModel` 中已经解析的
材质、UV 和贴图资源没有进入显示 DTO。

这不是联合切片结果问题，而是切片前场景检查能力不完整。若不先修正，用户无法确认多模型排版和
纹理资源是否正确，也会降低 13B-05 联合 Raster 的可解释性。

## 2. 决策

```text
俯视画布始终显示 SceneDocument 中全部 visible 实例；
追加模型成功后，自动应用当前 11x2 row-major 排版规则；
默认列/行净距均为 10 mm，用户仍可在“排版”页修改或恢复；
SceneViewGeometry 保留每个三角形的变换后 Z、UV 和材质索引；
SceneViewGeometry 保留材质 diffuse RGB、贴图路径和资源可用性；
后台投影任务生成最大 768 像素边长的 RGBA 俯视 SurfacePreview；
SurfacePreview 使用逐像素 +Z Z-buffer 和 UV 采样，顶层表面覆盖底层表面；
UV 采样遵循配置中的 sampler、uvAddressMode 和 flipV；
采样参数进入 UI source cache identity，避免同模型不同采样配置相互覆盖；
Qt 画布只消费已生成的 RGBA 预览，不在 paintEvent 解码贴图或遍历高密度纹理三角形；
无贴图时使用材质 diffuse RGB，无材质时使用稳定回退色；
blocked 状态保留纹理可见性，并叠加红色诊断提示。
```

## 3. 边界

```text
俯视纹理只用于切片前显示，不参与生产 RGBWSV 合成；
自动排版只发生在追加导入后，不修改模型源文件；
隐藏实例不显示，但仍遵循现有排版占位规则；
不引入 Qt3D、VTK 或 OpenGL 新依赖；
SurfacePreview 是显示级有损缩放结果，不等同于生产 TIFF；
不改变 p0.rgbwsv.2、TIFF、Legacy/Global 或材料优先级；
13B-05 仍负责生产级共享 Raster 与联合层合成。
```

## 4. 失败处理

追加导入成功但自动排版失败时保留已导入场景，不伪装排版成功，并通过
`SigAutoLayoutFailed` 在状态栏和日志中给出稳定错误。用户可进入“排版”页修正参数后重新执行。
场景存在尚未完成重投影的实例时，禁止继续追加或排版，避免把旧几何重新标记为新 transform identity。

## 5. 验收

```text
两个追加导入实例自动进入不同列，XY 包围盒不重叠；
画布适应全部可见实例的联合 bounds；
材质 diffuse RGB 可见；
有效贴图路径和 UV 可在俯视图显示实际贴图颜色；
选择、隐藏、锁定、删除和三种窗口尺寸保持回归；
SceneViewGeometry identity/hash/revision 合同保持有效。
```
