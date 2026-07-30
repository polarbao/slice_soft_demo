# DOC_DECISION_13 模型场景、排版联合切片与 TIFF 原生预览专项拆分

> 文档版本：v0.2
> 文档状态：APPROVED TARGET DESIGN / 13A-01、13B-01 IMPLEMENTED
> 生成日期：2026-07-24
> 更新日期：2026-07-27
> 决策范围：模型显示与变换、多模型排版与联合切片、TIFF 原生统一预览

## 1. 决策摘要

新增正式 Stage 13，并拆分为三个相互依赖但可独立验收的专项：

```text
13A：模型俯视工作区、选择与实例变换；
13B：MultiModelScene、规则排版与多模型联合切片；
13C：RGBWSV TIFF 原生统一预览与预览 IO 收口。
```

Stage 13 不并入 12E-10。12E-10 继续承担单模型双引擎生产基线的最终证据收口，Stage 13
承担模型场景与多模型产品能力。

## 2. 当前事实

当前实现是单模型生产路径：

```text
配置强制要求单一 input.modelPath；
SceneModel 仍是单模型报告的轻量边界；
manifest/source 和 layer stats 没有 modelId/instanceId；
Qt 工作台没有模型画布、模型列表和实例变换编辑器；
没有 build volume、排版、碰撞和联合切片生产合同。
```

当前统一预览已经有三种 UI 模式，但数据源并未统一：

```text
生产 RGB 与像素探针可读取 RGBWSV TIFF；
W/S/V 和材料叠加主要依赖 preview PNG；
原始调试预览依赖可选 preview 文件；
PreviewOverlayPanel 仍维护第二套图片索引和合成逻辑。
```

因此，Stage 13 是正式产品能力扩展，不是简单增加几个按钮。

## 3. 13A 决策

### 3.1 短期能力

俯视方向固定为从 `+Z` 看向 XY 平面，并显示 X/Y 轴、毫米网格、模型轮廓和包围盒。

短期实例变换固定为：

```text
translateX / translateY；
rotateZ；
uniformScale；
mirrorX / mirrorY；
Z 平移不开放；
rotateX / rotateY 不开放；
模型落台和 Z 基准继续使用当前切片流程。
```

旋转和缩放中心默认使用模型完成当前自动姿态后的 XY 包围盒中心，pivot Z 使用当前 bbox.minZ。
实例变换发生在现有 `modelTransform + autoOrient` 之后，不回写源变换。镜像不得用不透明的负
scale 代替，必须作为显式字段，并修正三角形 winding、法线和纹理坐标解释。

### 3.2 中长期能力

中期增加真实 3D 相机、透视/正交切换、轨道旋转、缩放、选择、拾取、包围盒和截面检查。

长期增加 3D 打印软件常见的移动、旋转、缩放、镜像、复制、删除、对齐、吸附、撤销/重做和
变换 gizmo。自动朝向、自动嵌套和支撑编辑不属于 13A 短期范围。

## 4. 13B 决策

### 4.1 场景与实例

多模型采用 `modelId + instanceId`：

```text
modelId：唯一标识模型几何、材质和纹理资源；
instanceId：唯一标识一个摆放实例；
同一个 modelId 可创建多个 instanceId；
每个实例保存自身 transform、visible、locked 和 admission 状态。
```

### 4.2 P0 排版规则

```text
最大列数：11；
最大行数：2；
最大实例数：22；
默认列间净距：10.00 mm；
默认行间净距：10.00 mm；
UI 步长：0.01 mm；
排列顺序：row_major；
间距语义：模型变换后 XY 包围盒之间的边到边净距，不是中心距。
```

规则排版完成后允许用户继续手动调整 X/Y、rotateZ、uniformScale 和镜像。模型重叠、越界或
未通过几何准入时，生产切片必须 fail-closed。

### 4.3 联合切片

“多个模型一起切片、排版”定义为：

```text
所有实例进入同一场景和同一全局 XY raster；
共享 layerHeight 和 layerIndex/zMm；
每个实例保留独立资源作用域和准入结果；
每层输出一个 RGBWSV TIFF；
整个场景输出一个 p0.rgbwsv.2 package；
报告可按 modelId/instanceId 追踪统计。
```

P0 不做跨模型联合支撑优化。每个实例先按现有策略生成自身材料和支撑，再映射到全局层画布。
P0 禁止模型重叠，因此不定义重叠像素的材料混合策略。

### 4.4 打印幅面

正式生产必须从设备/Profile 获得 `buildVolume.widthMm/heightMm`。在设备尺寸尚未确认前，只能用
明确标记为 fixture 的测试幅面，不得把动态场景包围盒冒充设备可打印范围。

未知幅面在 schema 中使用 `source=unresolved` 和 optional width/height 表达，不使用 `0.0` 冒充。
产品尚未确认多实例不同材料 Profile 前，P0 使用 `scene_profile_only`，不同 Profile 请求
fail-closed。

## 5. 13C 决策

生产预览的唯一权威像素源改为 package 中的 RGBWSV TIFF。UI 在内存中从同一层 TIFF 派生：

```text
RGB 真彩/生产值检查；
R/G/B 单通道；
W 白墨伪彩；
S 支撑伪彩；
V 光油伪彩；
RGB + W；
RGB + S；
RGB + V；
RGB + S + W + V；
占用与真实空白。
```

伪彩仅用于显示，不修改 TIFF。默认颜色继续由配置/报告提供，像素探针始终显示真实
`R/G/B/W/S/V` 数值。

生产切片默认不再为上述视图重复生成逐通道 PNG。以下信息无法仅从 TIFF 反推，可继续作为可选诊断数据：

```text
Texture Surface 与 Model Fill 在同一物理通道内的语义分区；
closure gap；
fallback、拓扑和支撑类型诊断；
原始算法中间 mask。
```

诊断数据必须按需生成或由 report/semantic mask 提供，不得和生产 TIFF 混为一套协议。

## 6. 与 12E-09A 和 12E-10 的顺序

新需求会改变 09A Effective Config 的身份范围，也会改变 09A-05/12E-10A 的预览数据源。因此：

```text
先完成 Stage 13 文档和 scene/instance/config identity 设计准备；
再执行 13A-01、13B-01 的基础 DTO/配置合同；
随后执行 12E-09A-02，并使其兼容 single_model/scene；
13C 必须在 12E-09A-05 前完成 TIFF 原生数据源和合成器；
12E-10 仍在 09A-05 后执行，并明确为单模型双引擎基线收口。
```

完整多模型联合切片不作为 12E-10 的隐式验收项；它由 Stage 13B 自己的生产矩阵验收。

## 7. 第三方 3D 显示决策

短期俯视图不新增第三方依赖，优先复用 Qt 5.15 与项目 Scene DTO。

中期必须先做技术 Spike，不直接锁库：

| 方案 | CMake/vcpkg | 许可证 | 优点 | 主要风险 |
|---|---|---|---|---|
| VTK 9.x + Qt | vcpkg `vtk`/Qt feature，CMake `find_package(VTK CONFIG)` | BSD-3-Clause | 拾取、相机、网格、裁切和大模型能力完整 | 依赖和部署体积大，Qt 版本匹配复杂 |
| Qt3D 5.15 | Qt 安装组件，CMake `Qt5::3DCore/3DRender/3DInput/3DExtras` | 依 Qt 商业或开源许可 | 与 Qt Widgets 集成成本较低 | Qt3D 生命周期、精细拾取和工业可维护性风险 |
| QOpenGLWidget 自研 | 无新增第三方，使用 Qt/OpenGL | 随 Qt/OpenGL | 依赖最少、渲染和交互可完全控制 | 选择、gizmo、裁切和性能优化开发量最大 |

当前推荐：

```text
短期：Qt 原生 2D/轻量 OpenGL 俯视图；
中期：VTK 与 QOpenGLWidget 做同模型、同功能、同部署条件 Spike；
只有 Spike 的启动时间、交互帧率、部署大小和许可证评审通过后才冻结正式 3D 后端。
```

## 8. 固定红线

```text
不修改 schema=p0.rgbwsv.2；
不修改 R G B W S V；
不修改 uint8 / black_is_print；
不让 Qt 类型进入 slicer_core；
不让 UI 直接访问 slicer.cpp 临时结构；
不让 OpenVDB 成为模型显示依赖；
不允许模型变换绕过 ProductionAdmissionPolicy；
不允许多模型失败后静默改成逐模型切片并伪装联合切片成功。
```

## 9. 未决产品输入

开始 13B 生产实现前仍需由设备/Profile 固化：

```text
实际打印幅面 widthMm/heightMm；
场景原点和机器 X/Y 正方向；
是否允许不同模型使用不同材料 Profile；
场景最大三角形数、内存预算和可接受切片时间；
自动排版是否只做规则网格，还是未来需要真实 nesting。
```

未决输入不具有相同阻断范围：buildVolume/轴方向阻断 13B-04 production；性能预算阻断
13B-07 GO；多 Profile 在 P0 由 `scene_profile_only` 保守规则承接；3D 后端只阻断 13A-R2/R3。
完整 Gate 见 `DOC_CHECKLIST_13_未决产品输入与阶段Gate.md`。
