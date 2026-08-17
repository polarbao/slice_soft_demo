# DOC DECISION HOSTFLOW H-B-03-R1 三轴旋转合同受控修订

> 状态：**IMPLEMENTED / VERIFIED**
> 日期：2026-08-17
> 范围：封装版宿主的模型实例 X/Y/Z 三轴旋转、显式触底以及生产几何闭环。

## 1. 背景与结论

D 盘旧版 UI 已具有绕 X、Y、Z 轴旋转能力，但封装版宿主此前只通过公开
`scene.apply_operation` 提交 `rotateZ`。只复制 Qt 控件会导致界面显示与生产切片几何不一致，
因此本次采用受控合同修订：在保持 SPI v1、11 个导出、15 项能力和 RGBWSV/TIFF 协议不变的
前提下，以加法方式增加 `rotateX`、`rotateY`、`landOnBuildPlate` 操作及对应 canonical transform 字段。

## 2. 冻结语义

- 旋转采用右手坐标系，规范顺序固定为 `X -> Y -> Z`。
- 旋转枢轴仍为源包围盒 `centerX / centerY / minZ`。
- `landOnBuildPlate` 由模块沿 Z 计算偏移，使结果 `minZ=0`；触底状态随实例持久化，
  后续 X/Y 旋转、镜像或缩放仍重新贴合平台。
- 自动落地偏移必须同时作用于碰撞包围盒、生产切片几何和 ViewData `worldMatrix`。
- 参数编辑保持宿主本地零 DLL 调用；点击提交后，对全部选中实例执行一次原子 Commit，
  场景 revision 只递增一次。
- 封装宿主默认在导入和变换提交时启用触底，并在“变换与排版”中提供可见开关及
  “将选中实例触底”命令；任意 Z 平移不对操作员开放。
- 旧 `rotateZ` 请求、旧 scene JSON 和已有 Profile 继续兼容；旧文档缺少 X/Y 字段时按 0 读取。

## 3. 合同变更

`contracts/slicer_capability_dtos.*` 从 v1.11 升为 v1.12：

```text
operations[].type += rotateX | rotateY
operations[].type += landOnBuildPlate
initialTransform += rotateXDeg | rotateYDeg | landOnBuildPlate
rotateX/rotateY required = instanceId + degrees
landOnBuildPlate required = instanceId; target = z_equals_zero
```

内部 `slicesoft.multimodel_scene.13b.1` schema 仅增加可选字段，不提升 scene schema 名称；这是为了
继续读取历史场景文档，不允许借此改变旧字段语义。

## 4. 实现与验证边界

实现必须覆盖：

1. `ModelTransform` 规范化、哈希、组合和序列化；
2. `TransformedModelAdapter` 三轴顶点变换与受控触底；
3. Scene Facade 操作解析、原子提交、碰撞和 ViewData 矩阵；
4. 封装版 Qt UI 的 X/Y/Z 精确角度控件；
5. 合同测试、核心单测和宿主流程测试。

验收时至少证明 X/Y 非有限值失败即拒绝、360 度等价归一化、触底后 `minZ=0`、两实例
三轴旋转加触底只有一次 revision，以及 Debug/Release 封装版宿主可构建。

## 5. 非目标

- 不增加自由旋转操纵器或鼠标轨迹球编辑；
- 不开放任意 Z 轴平移，只允许模块计算的 `minZ=0` 触底操作；
- 不修改自动定向的 SourceTransform；
- 不修改切片材料、支撑、纹理、TIFF 位深和极性。

## 6. 实施与验证结果

- 封装版 UI 已提供绕 X、Y、Z 三轴角度输入，并通过一次原子 Commit 应用于全部选中实例；
- X/Y 倾斜会把几何自动落回源模型 `minZ`，显式 `landOnBuildPlate` 则落到 `Z=0`；
- 碰撞几何、生产几何、ViewData `worldMatrix` 和变换哈希采用同一三轴变换；
- `slicer_capability_dtos` 已升级到 v1.12，SPI v1、11 个导出和 15 项能力保持不变；
- 2026-08-17 已完成 Debug/Release 构建及合同、核心、Facade、ViewData、排版和宿主流程回归。
