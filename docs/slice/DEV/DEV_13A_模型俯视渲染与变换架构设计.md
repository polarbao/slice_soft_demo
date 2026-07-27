# DEV_13A 模型俯视渲染与实例变换架构设计

> 文档版本：v0.1
> 文档状态：Formal DEV / PREPARED
> 生成日期：2026-07-24

## 1. 设计目标

在不把 Qt 引入 `slicer_core` 的前提下，建立可被单模型和多模型复用的实例变换合同，并在 Qt 工作台
增加短期俯视显示。

## 2. 模块边界

建议新增或扩展：

```text
src/slicer_core/scene/ModelTransform.*
src/slicer_core/scene/ModelInstance.*
src/slicer_core/scene/SceneViewGeometry.*
src/slicer_core/geometry/TransformedModelAdapter.*

apps/slicer_debug_ui/models/SceneDocument.*
apps/slicer_debug_ui/models/SceneSelectionModel.*
apps/slicer_debug_ui/widgets/ModelTopViewWidget.*
apps/slicer_debug_ui/widgets/ModelTransformPanel.*
apps/slicer_debug_ui/controllers/SceneTransformController.*
```

依赖方向：

```text
UI -> scene public DTO；
scene transform -> geometry DTO；
pipeline -> transformed scene；
slicer_core 不依赖 Qt；
render widget 不直接读取 slicer.cpp 内部临时对象。
```

## 3. 核心 DTO

建议合同：

```cpp
struct ModelTransform
{
    double translatexmm{0.0};
    double translateymm{0.0};
    double rotatezdeg{0.0};
    double uniformscale{1.0};
    bool mirrorx{false};
    bool mirrory{false};
};
```

正式实现时遵守项目代码规范，Public 接口提供 Doxygen 注释。结构体数据成员保持项目已冻结的
小写规则；类和函数使用 PascalCase。

`ModelInstance` 至少包含：

```text
instanceId；
modelId；
ModelTransform；
visible；
locked；
sourceAdmission；
transformedAdmission；
transformRevision。
```

## 4. 变换次序

短期变换固定为：

```text
1. 导入模型并完成现有默认姿态/自动朝向；
2. 计算 SourceTransform 后的 pivot：bbox XY 中心和 bbox.minZ；
3. uniformScale；
4. mirrorX/mirrorY；
5. rotateZ；
6. translateX/translateY；
7. 保持 SourceTransform 的 minZ 基准，不新增 Z 平移或二次自动落台；
8. transformed preflight；
9. 生成 view geometry 和切片输入。
```

等价矩阵次序：

```text
T(layout/user) * T(pivot) * Rz * Mirror * Scale * T(-pivot)
```

其中 `pivot=(bboxCenterX,bboxCenterY,bboxMinZ)`。`modelTransform + autoOrient` 是 SourceTransform，
实例变换在其后应用；不得把实例变换写回旧 `SliceConfig::transform`。

镜像使用显式布尔字段。奇数次镜像使矩阵行列式为负时，三角形 winding 必须反转，法线重新计算，
UV 仍跟随顶点；不得把负 scale 原样泄漏给后续拓扑判断。

完整执行合同见 `DOC_PREP_13A_01_ModelTransform与ModelInstance合同准备.md`。

## 5. 俯视渲染

短期渲染数据由核心输出轻量只读 DTO：

```text
projected triangles or contour segments；
XY bounding box；
model/instance identity；
selection/blocked flags；
material/texture display hint；
revision。
```

当前实现进一步冻结：

```text
SceneViewTriangle 保留变换后 Z、UV 和 materialIndex；
SceneViewGeometry 保留 display-only material appearance；
后台投影任务生成最大边长 768 像素的 display-only RGBA SurfacePreview；
SurfacePreview 使用逐像素 +Z Z-buffer 和配置一致的 UV 采样参数；
Qt paintEvent 只绘制内存 SurfacePreview，不读取贴图文件；
显示资源不得反向进入生产材料合成。
```

Qt 侧首版可使用 `QPainter` 或 `QOpenGLWidget`：

```text
只绘制 XY 投影；
坐标换算集中在 Camera2D；
明确显示 X/Y 正方向；
保持毫米比例；
大量三角形时允许使用轮廓/抽样 LOD；
选择命中优先使用包围盒 + 三角形投影二阶段判断。
```

## 6. 状态与命令

所有 UI 操作通过命令对象修改 `SceneDocument`：

```text
MoveInstanceCommand；
RotateInstanceCommand；
ScaleInstanceCommand；
MirrorInstanceCommand；
ResetInstanceTransformCommand。
```

R1 可只保留有限撤销栈或先实现 reset；R3 再补完整 `QUndoStack`。Controller 负责校验并发出自定义
`Sig...` 信号；槽函数使用 `On...` 命名和函数指针 connect。

## 7. 与配置和切片的连接

UI 不直接改源 Profile。变换写入 session scene/effective config：

```text
requestedTransform；
effectiveTransform；
transformRevision；
sourceModelHash；
sceneRevision。
```

切片开始前必须比较当前画布 revision 与 effective config revision；不一致时重新生成配置，不得使用
stale transform。

## 8. 3D 后端候选

### VTK 9.x

```text
CMake：find_package(VTK CONFIG REQUIRED COMPONENTS CommonCore RenderingCore RenderingOpenGL2 GUISupportQt)；
vcpkg：使用 manifest feature 锁版本，不修改共享 vcpkg 安装；
许可证：BSD-3-Clause；
优点：相机、拾取、裁切、网格显示成熟；
风险：依赖图、部署大小、Qt 版本和启动时间。
```

### Qt3D 5.15

```text
CMake：Qt5::3DCore/3DRender/3DInput/3DExtras；
vcpkg：通常跟随现有 Qt 安装，不单独复制二进制；
许可证：跟随 Qt 5.15 商业或开源许可，发布前需法务确认；
优点：Qt 对象和输入系统集成直接；
风险：模块生命周期、工业拾取精度和后续 Qt 迁移。
```

### QOpenGLWidget

```text
CMake：现有 Qt/OpenGL target；
无新增第三方许可证；
优点：依赖最少、可为切片场景定制；
风险：相机、拾取、gizmo、裁切、LOD 和驱动兼容均需自研。
```

R2 Spike 使用同一真实 OBJ/3MF，在三种窗口尺寸测：

```text
首次显示时间；
交互帧率；
峰值内存；
选择延迟；
纹理正确性；
部署增量；
许可证和维护结论。
```

## 9. 性能和线程

```text
模型导入和投影构建在 Worker 线程；
QImage/QPainter 最终 UI 更新回主线程；
变换拖动期间使用 LOD，释放后重建精确投影；
缓存 key 至少包含 model hash + transform revision + display mode；
追加导入后使用当前 SceneLayout 自动排版，画布 camera 使用全部 visible 实例的联合 bounds；
切换模型或关闭窗口时取消 Worker；
不得悬挂 QObject 或复用 stale geometry。
```

## 10. 测试

```text
ModelTransform 数学单测；
mirror winding/normal/UV 单测；
transform revision/stale 单测；
落台不变性单测；
view bounds 与 core bounds 对比；
选择隔离、重置、失败状态 UI self-test；
三窗口尺寸 UI smoke；
默认单模型切片回归。
```

## 11. 风险

```text
旋转中心不一致导致 UI 和切片结果偏移；
镜像未修 winding 导致 strict admission 误判；
显示坐标 Y 翻转导致视觉与切片坐标不一致；
缩放改变工艺厚度解释；
高三角模型主线程卡顿；
过早锁定大型 3D 库增加部署负担。
```
