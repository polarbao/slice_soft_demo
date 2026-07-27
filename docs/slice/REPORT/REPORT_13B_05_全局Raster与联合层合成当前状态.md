# REPORT 13B-05 全局 Raster 与联合层合成当前状态

> 状态：FIXTURE COMPLETE
> 日期：2026-07-27
> 前置：13B-04 / 13B-04A COMPLETE
> 下一任务：13B-06 单一 package 与 scene report

## 1. 阶段结论

13B-05 已建立无 Qt、无文件输出的多模型联合层合成链路。通过 13B-04 准入的可见实例可由
Legacy 或 Global adapter 转换为统一的局部 Raster，再映射到一个共享 XY/Z Grid，最终按层输出
唯一的 writer-ready RGBWSV 内存 buffer。

本结论仅达到功能 Fixture Gate。正式设备 `buildVolume/origin/axes` 仍未冻结，因此不能把本阶段
描述为多模型生产 GO。

## 2. 已实现合同

### 2.1 Scene Raster 公共类型

新增：

```text
SceneRasterGrid；
SceneInstanceRaster / SceneInstanceRasterLayer；
SceneRasterIdentity；
SceneLayerComposeRequest / SceneLayerComposeResult；
SceneRasterAdapterResult；
SceneRasterErrorCode；
逐实例和场景统计。
```

固定协议保持：

```text
schema=p0.rgbwsv.2；
channelOrder=R,G,B,W,S,V；
bitDepth=8；
polarity=black_is_print；
printValue=0；
emptyValue=255。
```

每个 Raster 绑定 `sceneId/modelId/instanceId`、scene revision、transform revision 和 transform
hash。碰撞准入结果保留独立 transform 证据，Orchestrator 会拒绝重复实例身份和过期 Raster。

### 2.2 Legacy / Global Adapter

Legacy adapter：

```text
复用现有 run_slicer 计算，不复制切片数学；
通过 callback 取得 Grid、最终 RGBWSV 字节和材料 ownership；
通过 ModelInstance override 把已准入 XY/rotateZ/scale/mirror 变换作用到真实几何；
校验配置模型路径与 sourceTransformIdentity 一致；
关闭 TIFF、preview、report 和 package 写入；
保留 model、model-owned V、outer varnish、support ownership。
```

Global adapter：

```text
包装现有 GlobalSurfaceShellProductionLayerAdapterResult；
要求 source 已 closure PASS 且没有写生产输出；
校验完整层序、尺寸、mask 和固定协议；
畸形字节或 mask 返回结构化 LayerSizeInvalid，不向调用方抛异常。
```

场景内仍只允许一个 `effectivePipelineMode`，禁止 Legacy/Global 混合。

### 2.3 共享 Grid

共享 Grid 规则已实现：

```text
所有实例 dpiX/dpiY/layerThickness 必须一致；
XY 原点取已准入可见局部 Raster 的最小对齐原点；
Z 原点固定为打印床 0.0 mm；
局部 XY/Z 必须量化为整数像素/层 offset；
负 Z、非整数 offset 和 int extent 溢出 fail-closed；
全局宽、高、层数取对齐后的可见实例并集；
隐藏实例不进入联合 Raster。
```

### 2.4 联合层合成

每个全局层以六通道 `255` 初始化，并按稳定实例顺序映射局部层。材料优先级为：

```text
Model > OuterVarnish > Support > Empty
```

其中 model-owned V 与 outer-varnish V 使用独立 ownership，V-only 模型材料可合法输出；模型覆盖
外光油时不会把 outer V 带入最终模型像素。不同实例模型重叠返回 `InstanceOverlap`，模型与另一
实例支撑/外光油冲突返回 `MaterialConflict`，不会依赖顺序静默覆盖。

成功结果逐层执行 closure；失败清空结果层，不返回部分 writer-ready 输出，也不修改输入 Raster。

## 3. 稳定错误

当前提供：

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
SCENE_RASTER_REVISION_STALE；
SCENE_RASTER_PIPELINE_MODE_MISMATCH；
SCENE_RASTER_PRODUCER_FAILED。
```

## 4. 验证证据

本阶段实际运行：

```powershell
cmake --build build --config Debug --target multi_model_layer_composer_unit_tests
cmake --build build --config Debug --target scene_layer_adapters_unit_tests scene_collision_admission_unit_tests multi_model_layer_composer_unit_tests
ctest --test-dir build -C Debug -R "^(multi_model_layer_composer_unit_tests|scene_layer_adapters_unit_tests|scene_collision_admission_unit_tests|grid_layout_policy_unit_tests|global_surface_shell_production_layer_adapter_unit_tests|material_channel_composer_unit_tests)$" --output-on-failure
cmake --build build --config Debug
.\scripts\run_ci_quick.ps1
```

结果：

```text
6/6 定向测试通过；
Debug 全量构建通过；
Quick CI 通过，包含 Golden、UI self-test 和 overlay-load-real；
Legacy adapter 的 package 目录前后快照一致；
Legacy XY transform、source identity、Global bad layer/protocol、bed Z=0、extent overflow、
重复 admission identity 和 stale transform evidence 均有自动化覆盖。
```

## 5. 当前边界与残余风险

```text
13B-05 不写 TIFF、manifest、report 或 package；
13B-05 不实现跨实例联合支撑；
13B-05 不永久布尔合并多个 mesh；
fixture 请求仍可保留全部实例层，生产流式 producer 的峰值内存优化属于后续工程化；
SceneRasterAdapterResult::IsValid 负责结构完整性，最终材料语义 closure 由 SceneLayerComposer 执行；
极端内存分配失败目前仍可能由 std::bad_alloc 上抛，后续生产 Orchestrator 应转为稳定资源错误；
Legacy 源路径不一致目前映射为 ProducerFailed，后续可细化为 InstanceIdentityInvalid；
sourceTransformIdentity 当前验证路径等价性，尚未加入文件内容 fingerprint 和 TOCTOU 防护；
旋转、缩放、镜像的核心几何变换已有既有单测，本阶段 Legacy adapter 新增的是实际 XY 变换绑定回归；
正式设备 buildVolume/origin/axes 和 22 实例资源预算继续阻断 production GO。
```

Legacy 仍为默认生产引擎；Global/OpenVDB 仍为显式 opt-in，默认关闭。生产协议、六通道顺序、位深
和极性均未修改。

## 6. 下一步

进入 13B-06 前，应先补齐独立的单 package 与 scene report 执行准备，明确：

```text
共享 writer 的输入和原子发布边界；
manifest 保持 p0.rgbwsv.2 的兼容策略；
scene/instance 可审计 report schema；
每层唯一 TIFF、失败无伪成功 package；
RIP Reader strict 验证；
fixture PASS 与 production GO 的区分。
```
