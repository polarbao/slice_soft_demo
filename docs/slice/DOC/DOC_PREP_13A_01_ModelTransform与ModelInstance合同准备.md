# DOC_PREP_13A-01 ModelTransform 与 ModelInstance 合同准备

> 文档状态：READY FOR DEVELOPMENT
> 版本：v1.0
> 日期：2026-07-27
> 对应任务：13A-01

## 1. 目标

在 `slicer_core` 建立不依赖 Qt 的实例变换合同，为单模型俯视、镜像、post-transform preflight
和 13B 多模型场景提供唯一身份与数学语义。本任务不实现 Qt 画布、排版或联合切片。

## 2. 当前代码事实

当前模型链路为：

```text
源顶点
  -> modelTransform.unit/scale/rotationDeg/translationMm
  -> load OBJ/STL/3MF
  -> autoOrient 选择 identity 或 X/Y 直角旋转
  -> autoOrient 被应用时把 minZ 归一到 0
  -> SceneModel(ModelReport)
  -> preflight / Legacy / Global。
```

当前 `SceneModel` 是 `ModelReport` 的别名；项目没有 `ModelTransform`、`ModelInstance` 或
`transformRevision`。现有 `modelTransform` 属于模型源配置，不等于 Stage 13 的实例摆放。

## 3. 冻结边界

### 3.1 两级变换

```text
SourceTransform：
  现有 modelTransform + autoOrient；
  负责单位、源模型姿态和当前 Z 落台语义；
  兼容旧单模型配置。

InstanceTransform：
  Stage 13 新增；
  在 SourceTransform 之后应用；
  只开放 XY 平移、rotateZ、uniformScale、mirrorX/mirrorY；
  不开放 Z 平移、rotateX、rotateY 和非均匀缩放。
```

不得把 Stage 13 实例变换直接写回 `SliceConfig::transform`，否则会改变 autoOrient 的输入和旧配置
语义。

### 3.2 Pivot 与 Z 语义

实例变换 pivot 固定为 SourceTransform 后包围盒：

```text
pivotX = (minX + maxX) / 2；
pivotY = (minY + maxY) / 2；
pivotZ = minZ。
```

uniformScale 围绕该 pivot 执行；mirrorX/mirrorY 只反射 X/Y；rotateZ 不改变 Z；translate 只作用于
X/Y。因此 identity、旋转、镜像和缩放都保持 SourceTransform 的当前 minZ 基准，不新增“自动再落台”
副作用。

### 3.3 变换次序

固定次序：

```text
T(translateXY)
* T(pivot)
* Rz
* MirrorXY
* UniformScale
* T(-pivot)
```

向量和矩阵采用右手坐标；角度单位为度；序列化不得保存平台相关浮点格式。

## 4. 合同对象

建议新增：

```text
src/slicer_core/scene/ModelTransform.h/.cpp；
src/slicer_core/scene/ModelInstance.h/.cpp；
src/slicer_core/geometry/TransformedModelAdapter.h/.cpp。
```

核心 DTO：

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

`ModelInstance` 至少包含：

```text
instanceid；
modelid；
transform；
visible；
locked；
transformrevision；
sourcebboxmm；
effectivebboxmm。
```

结构体数据成员保持小写；类和函数使用 PascalCase；Public API 提供 Doxygen；C++ 使用 Allman。

## 5. 身份、revision 与 hash

```text
instanceId 不是数组下标，不因排序改变；
transformRevision 从 0 开始；
只有 effective transform 实际变化时递增；
重复写入相同值不得递增；
reset 回到 identity，但仍属于一次实际变更；
transform hash 包含 schema id、规范化数值、镜像字段和 SourceTransform identity；
NaN、Inf、uniformScale<=0 必须在 hash 前被拒绝。
```

浮点规范化仅用于稳定序列化和 hash，不得降低实际矩阵计算精度。

## 6. 几何适配

`TransformedModelAdapter`：

```text
不修改 SceneModel 源三角形；
按需生成 transformed triangle/bbox；
UV 随顶点索引保持不变；
奇数次镜像时反转三角形 winding；
法线如需输出则从变换后几何重算；
返回 determinantSign/mirrored/revision 供 preflight 和报告审计。
```

13A-01 只建立 adapter 和数学测试，不接入生产切片路由。13A-04 才把 post-transform preflight
接到 Qt 和生产 Gate。

## 7. 校验与稳定错误

至少冻结：

```text
MODEL_TRANSFORM_NON_FINITE；
MODEL_TRANSFORM_SCALE_NON_POSITIVE；
MODEL_TRANSFORM_SOURCE_MISSING；
MODEL_INSTANCE_ID_EMPTY；
MODEL_INSTANCE_MODEL_ID_EMPTY；
MODEL_TRANSFORM_REVISION_STALE。
```

错误对象包含 instanceId/modelId/field/message；核心不返回中文 UI 文案。

## 8. 单模型兼容

未提供 Stage 13 scene/instance 时：

```text
构造一个内存中的 identity instance；
不得改写源配置；
变换后 triangle/bbox 与当前 SceneModel 位级等价或在冻结浮点容差内等价；
当前 Legacy/Global、preflight hash 和 TIFF 输出不因 13A-01 自动改变。
```

13A-01 不新增用户可见开关，不改变一键切片默认行为。

## 9. 实现落点与测试

建议新增测试 target：

```text
model_transform_unit_tests
```

必测：

```text
identity 不变性；
translateX/translateY；
rotateZ 90/180/360；
uniformScale；
mirrorX/mirrorY/双镜像；
矩阵次序；
pivot 与 minZ 保持；
winding 反转；
UV 索引保持；
bbox；
revision/stale；
NaN/Inf/scale<=0；
源 SceneModel 不被修改。
```

## 10. 验证命令

实现任务至少运行：

```powershell
cmake --build build --config Debug --target model_transform_unit_tests
ctest --test-dir build -C Debug -R model_transform_unit_tests --output-on-failure
cmake --build build --config Debug --target model_preflight_service_unit_tests
ctest --test-dir build -C Debug -R "model_preflight_service_unit_tests|slice_pipeline_router_unit_tests" --output-on-failure
git diff --check
```

本准备任务只生成文档，未运行上述未来代码验证。

## 11. 完成标准

```text
DTO、矩阵、pivot、Z、不变性、revision 和错误码均由单测冻结；
Qt 未进入 slicer_core；
未改变现有配置和生产输出；
13B-01 可以只依赖 Public scene DTO，不依赖 model.cpp 私有函数。
```

