# DOC_PREP 13B-05 全局 Raster 与联合层合成准备

> 文档状态：READY FOR FIXTURE DEVELOPMENT
> 日期：2026-07-27
> 前置：13B-04 FUNCTIONAL FIXTURE COMPLETE
> 下一报告：`REPORT_13B_05_全局Raster与联合层合成当前状态.md`

## 1. 目标

把已通过 13B-04 准入的多个可见实例映射到一个共享 XY/Z Raster，并为每个 `layerIndex`
生成一个 writer-ready RGBWSV 内存 buffer。

本任务只建立纯内存联合层合成，不写 TIFF、manifest、report 或 package。

## 2. 当前代码事实

可复用：

```text
RgbwsvProductionLayer：writer-ready RGBWSV 字节；
RgbwsvProtocol：p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
MaterialChannelComposer：单层材料合成；
GlobalSurfaceShellProductionLayerAdapter：Global writer-ready layer；
TextureFillPartitionRasterGridSpec：显式 XY/Z raster 几何；
SceneCollisionService：buildVolume、越界、碰撞和逐实例准入。
```

当前缺口：

```text
Legacy 的 GridSpec/RasterResult/LayerDiagnostics 仍是 slicer.cpp 私有类型；
Legacy 没有公开“只生成内存 layer、不写 package”的 producer；
Global layer 有 writer-ready DTO，但缺少 scene origin/instance identity；
两个引擎没有统一的 per-instance raster-layer contract；
没有局部 raster 到场景 raster 的整数偏移校验；
没有跨实例材料占用冲突和场景 closure；
没有流式 scene layer composer。
```

因此 13B-05 不得直接复制 `slicer.cpp` 私有结构，也不得在 Qt 层拼接 TIFF。

## 3. 内部分步

### 13B-05A 公共 Raster/Layer 合同

新增无 Qt DTO，建议位于：

```text
src/slicer_core/pipeline/SceneRasterTypes.h/.cpp；
```

至少包含：

```text
SceneRasterGrid：width/height/layerCount、originX/Y/Z、pitchX/Y、layerThickness；
SceneInstanceRaster：scene/model/instance identity、revision、local grid；
SceneInstanceRasterLayer：layerIndex/zMm、RGBWSV channels、模型/支撑/外光油 ownership；
SceneLayerComposeRequest/Result；
稳定错误、逐实例统计和全局统计。
```

所有通道固定：

```text
channelOrder=R,G,B,W,S,V；
bitDepth=8；
polarity=black_is_print；
emptyValue=255；
任一 required 输入协议不一致立即 fail-closed。
```

### 13B-05B 引擎 Adapter

建立显式 adapter，不让 composer 依赖引擎内部对象：

```text
Legacy adapter：从现有单模型生产计算提取内存层；
Global adapter：包装 GlobalSurfaceShellProductionLayerAdapterResult；
两者补齐 scene/model/instance/revision/local grid/ownership；
同一场景只允许一个 effective pipeline mode；
adapter 不写 TIFF/package。
```

Legacy 首步必须把私有层输出提取为 public core DTO；禁止复制一套独立切片数学。

### 13B-05C 共享 Grid 与 SceneLayerComposer

共享 grid 规则：

```text
dpiX/dpiY/layerHeight 必须全场景一致；
global XY 范围来自 13B-04 已准入的可见实例范围；
global Z 从统一落台后的 0 层开始，层序由场景最高 maxZ 决定；
local origin 到 global origin 必须可表示为整数像素偏移；
local zMm 到 global zMm 必须可表示为整数层偏移；
量化误差超过固定 tolerance 时 fail-closed，不重采样、不插值；
hidden 实例不进入 Raster；
相同输入保持稳定 layerIndex 和字节输出。
```

合成规则：

```text
初始所有通道 255；
逐实例按稳定 instance list 顺序映射；
模型 ownership > 外侧光油 ownership > 支撑 ownership > Empty；
不同实例的模型 ownership 重叠直接阻断；
模型与另一实例支撑/光油占用冲突必须输出结构化证据；
不得用稳定顺序覆盖来掩盖真实冲突；
模型间净距区域六通道保持 255；
最终每层执行 scene closure；
成功结果每个 layerIndex 只有一个全局 RGBWSV buffer。
```

13B-05 不实现跨实例联合支撑，也不把多个 mesh 永久布尔合并。

## 4. 稳定错误

至少提供：

```text
SCENE_RASTER_ADMISSION_REQUIRED；
SCENE_RASTER_GRID_INVALID；
SCENE_RASTER_PROTOCOL_MISMATCH；
SCENE_RASTER_RESOLUTION_MISMATCH；
SCENE_RASTER_LAYER_SEQUENCE_MISMATCH；
SCENE_RASTER_OFFSET_NOT_INTEGRAL；
SCENE_RASTER_LAYER_SIZE_INVALID；
SCENE_RASTER_INSTANCE_IDENTITY_INVALID；
SCENE_RASTER_INSTANCE_OVERLAP；
SCENE_RASTER_MATERIAL_CONFLICT；
SCENE_RASTER_CLOSURE_FAILED；
SCENE_RASTER_REVISION_STALE。
```

错误必须携带 `sceneId/modelId/instanceId/otherInstanceId/layerIndex/field`。

## 5. 原子性与内存

```text
任一实例失败时不返回部分 writer-ready 成功层；
失败不得修改输入层；
输出只在全部层检查通过后标记 available；
按层合成，避免保留 instanceCount x allLayers 的第二份全局副本；
fixture 可保留完整输入用于单测，真实 pipeline 优先使用 producer/callback；
本任务记录 composeMs 和 peak working-set 观察值，不冻结 SLA。
```

## 6. TDD 矩阵

```text
单实例 identity 映射与现有 RGBWSV 字节一致；
两个分离实例映射到正确 XY offset；
不同 local bbox 尺寸；
共享层序和不同高度；
隐藏实例跳过；
净距区域保持全 255；
RGB/W/S/V 不串实例；
Model > OuterVarnish > Support > Empty；
边界接触不产生覆盖；
模型 ownership 重叠 fail-closed；
材料冲突包含双 instanceId 和 layerIndex；
dpiX/dpiY/layerHeight 不一致 fail-closed；
非整数像素/层偏移 fail-closed；
channelOrder/bitDepth/polarity 不一致 fail-closed；
缺层、重复 layerIndex、错误字节数 fail-closed；
stale scene/transform revision fail-closed；
同一输入结果确定；
失败不返回部分成功层；
单模型 Legacy/Global writer-ready 回归。
```

## 7. 计划文件

建议：

```text
src/slicer_core/pipeline/SceneRasterTypes.h/.cpp；
src/slicer_core/pipeline/SceneLayerComposer.h/.cpp；
src/slicer_core/pipeline/MultiModelSliceOrchestrator.h/.cpp；
src/slicer_core/pipeline/LegacySceneLayerAdapter.h/.cpp；
src/slicer_core/pipeline/GlobalSceneLayerAdapter.h/.cpp；
tests/unit/multi_model_layer_composer/Main.cpp。
```

开始实现时必须再次检查 `slicer.cpp` 与 Global pipeline 的实际接口，遵循现有命名和依赖方向。

## 8. 验证命令

```powershell
cmake --build build --config Debug --target multi_model_layer_composer_unit_tests
ctest --test-dir build -C Debug -R "^(multi_model_layer_composer_unit_tests|scene_collision_admission_unit_tests|grid_layout_policy_unit_tests|global_surface_shell_production_layer_adapter_unit_tests|material_channel_composer_unit_tests)$" --output-on-failure
.\scripts\run_ci_quick.ps1
git diff --check
```

13B-05 不运行 `rip_reader_test --summary` 作为主验收，因为尚不写 package；该验证属于 13B-06。

## 9. 停止条件

```text
需要复制 Legacy 切片数学而不是提取公共内存输出；
需要从 TIFF 反读以完成场景合成；
需要修改 p0.rgbwsv.2 或通道顺序；
需要允许混合 Legacy/Global；
需要把碰撞实例按顺序覆盖；
需要实现跨实例联合支撑；
需要提前写最终 package；
13B-04 admission 不是 PASS；
单模型 Legacy/Global 回归失败。
```

## 10. 准备结论

13B-05 的公共合同、现有复用点、Legacy 公共输出缺口、内部分步、共享 grid、材料优先级、错误、
原子性、测试和停止条件已明确，无新的外部产品输入阻断 fixture 开发。正式设备幅面仍阻断最终
production GO，但不阻断以显式 fixture grid 开发联合层合成。
