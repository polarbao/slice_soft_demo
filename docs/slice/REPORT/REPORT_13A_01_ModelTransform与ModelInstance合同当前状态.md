# REPORT 13A-01 ModelTransform 与 ModelInstance 合同当前状态

> 状态：COMPLETE
> 日期：2026-07-27
> 范围：仅 13A-01，不包含 Qt 俯视、多模型场景或生产切片路由

## 1. 完成内容

新增无 Qt 的 Stage 13 实例变换合同：

```text
ModelTransform：XY 平移、Z 轴旋转、统一缩放、X/Y 镜像；
ModelInstance：instance/model/source identity、visible/locked、revision、source/effective bbox；
TransformedModelAdapter：按固定次序生成变换后三角形、UV、bbox 和审计字段；
SHA-256：为 transform identity 提供稳定的 lowercase 64 位摘要；
稳定错误：非有限值、非正缩放、源缺失、实例/模型 ID 缺失和 stale revision。
```

## 2. 冻结语义

```text
SourceTransform 仍由既有 modelTransform + autoOrient 负责；
InstanceTransform 在其后应用，不写回旧配置；
pivot = source bbox 的 XY 中心与 minZ；
次序 = T(XY) * T(pivot) * Rz * MirrorXY * UniformScale * T(-pivot)；
不支持 Z 平移、rotateX/rotateY 或非均匀缩放；
奇数镜像反转 winding，并同步交换 UV 顶点索引；
identity 不修改源 SceneModel，也不改变既有生产路由。
```

## 3. 代码落点

```text
src/slicer_core/scene/ModelTransform.h/.cpp；
src/slicer_core/scene/ModelInstance.h/.cpp；
src/slicer_core/geometry/TransformedModelAdapter.h/.cpp；
src/slicer_core/system/Sha256.h/.cpp；
tests/unit/model_transform/Main.cpp；
CMakeLists.txt。
```

## 4. 测试证据

TDD RED：

```text
cmake --build build --config Debug --target model_transform_unit_tests
结果：失败，缺少 slicer_core/geometry/TransformedModelAdapter.h。
```

实现后验证：

```text
cmake --build build --config Debug --target model_transform_unit_tests
结果：PASS。

ctest --test-dir build -C Debug -R
"model_transform_unit_tests|model_preflight_service_unit_tests|slice_pipeline_router_unit_tests"
--output-on-failure
结果：3/3 PASS。
```

覆盖 identity、XY 平移、90/180/360 度旋转、统一缩放、单/双镜像、pivot/minZ、bbox、
winding/UV、source 不变性、hash、非法值和 stale revision。

## 5. 未实现范围

```text
Qt 模型俯视和交互控件；
MultiModelScene/ResourceScope/Scene Effective Config；
post-transform production preflight 接线；
排版、碰撞、联合 Raster 和联合 package；
TIFF 原生统一预览。
```

## 6. 后续准备审计

13A-01 已解除两个前置依赖：

```text
13B-01：READY FOR DEVELOPMENT，建议作为下一原子任务先完成 scene identity；
13A-02：功能依赖已解除，但按单贡献者路线排在 identity wave 后。
```

`DOC_PREP_13B_01` 已补充实际 Public DTO 依赖和 requested/derived/effective transform 的组合边界。
目前不需要继续扩写通用 Stage 13 设计文档；设备 buildVolume、机器轴和 22 实例性能预算仍只阻断
13B 后续 production Gate，不阻断 13B-01。
