# REPORT 13B-01 MultiModelScene 与 Scene Effective Config 当前状态

> 日期：2026-07-27
> 状态：COMPLETE
> 对应任务：13B-01
> 提交：`357bf6b feat(13B-01): 建立多模型场景与生效配置合同`；
> `3c365b1 test(13B-01): 补齐场景快照回退证据`
> 后续状态：12E-09A-02 已完成；当前下一任务为 13A-02 模型俯视渲染

## 1. 结论

13B-01 已完成多模型场景身份和 Scene Effective Config 的无 Qt 核心合同。当前可以表达并校验：

```text
sceneId / sceneRevision；
modelId / instanceId / transformRevision；
OBJ、3MF、STL 的 ResourceScope；
requested / derivedLayout / effective transform；
scene_profile_only 材料绑定；
unresolved / fixture / device_profile buildVolume；
draft / functional fixture / production 三种验证目的；
scene_config.effective.json 原子发布、回读、完整性和 stale。
```

本任务没有实现模型列表 UI、11x2 排版、碰撞检查、联合 Raster 或生产 package。设备幅面未知仍允许
scene draft，但生产验证保持 fail-closed。

## 2. 实现文件

```text
src/slicer_core/scene/MultiModelScene.h
src/slicer_core/scene/MultiModelScene.cpp
src/slicer_core/scene/SceneEffectiveConfig.h
src/slicer_core/scene/SceneEffectiveConfig.cpp
src/slicer_core/scene/ModelTransform.h
src/slicer_core/scene/ModelTransform.cpp
src/slicer_core/json_value.h
src/slicer_core/json_value.cpp
tests/unit/multimodel_scene_contract/Main.cpp
samples/configs/scene/fixture_two_model_scene.json
samples/configs/scene/bad/duplicate_model_id.json
CMakeLists.txt
```

## 3. 冻结合同

### 3.1 Scene schema

```text
slicesoft.multimodel_scene.13b.1
subjectType = scene
materialBindingMode = scene_profile_only
effectiveTransform = derivedLayoutTransform * requestedTransform
```

变换组合按仿射结果比较，允许 JSON 十进制与三角计算产生的微小浮点差，并接受数学等价的镜像表示。
场景和实例 revision 必须在 JSON 可精确表示的非负整数范围内。

### 3.2 Effective Config

```text
slicesoft.scene_effective_config.13b.1
固定文件名 scene_config.effective.json
临时文件 + rename 原子发布
SHA-256 configHash
禁止覆盖 scene draft
取消不保留部分输出
sceneId/revision/hash 或 transformRevision 改变后旧配置 stale
```

Scene Effective Config 记录 `dpiX`、`dpiY`、`layerHeightMm`、`slicePipelineMode`、Profile、
完整 scene config 和 per-instance revision。它不替代现有单模型生产 Effective Config。

### 3.3 稳定错误

已实现并测试的错误包括：

```text
SCENE_SCHEMA_UNSUPPORTED
SCENE_ID_EMPTY
SCENE_REVISION_STALE
SCENE_REVISION_INVALID
MODEL_ID_DUPLICATE
MODEL_SOURCE_INVALID
INSTANCE_ID_DUPLICATE
INSTANCE_MODEL_REFERENCE_MISSING
INSTANCE_TRANSFORM_INVALID
RESOURCE_SCOPE_ESCAPE
RESOURCE_SCOPE_MISSING
BUILD_VOLUME_UNDEFINED
BUILD_VOLUME_FIXTURE_NOT_PRODUCTION
SCENE_PROFILE_MISMATCH
SCENE_LAYOUT_INVALID
SCENE_EFFECTIVE_CONFIG_CANCELLED
SCENE_EFFECTIVE_CONFIG_INTEGRITY_FAILED
SCENE_EFFECTIVE_CONFIG_WRITE_FAILED
```

## 4. 兼容与安全边界

```text
旧单模型输入可投影为一个 model + 一个 instance 的临时 scene；
identity 实例不改变原有模型变换；
OBJ 资源只能在其目录 ResourceScope 内解析；
3MF 使用 package path + part identity；
STL 资源作用域固定为源文件；
fixture buildVolume 只能用于功能验证，不能作为设备生产证据；
不同 Profile 在 P0 中 fail-closed，不做隐式材料合并；
没有引入 Qt 或 OpenVDB 依赖。
```

生产协议保持不变：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
Legacy 默认
OpenVDB 默认关闭
```

## 5. TDD 与验证证据

RED 证据：

```text
首次构建 multimodel_scene_contract_unit_tests 因 MultiModelScene.h 不存在而失败；
等价十进制变换测试在精确浮点比较下失败；
等价 mirrorY 表达测试在字段比较下失败；
超出 JSON 精确整数范围的 revision 测试在错误码实现前编译失败。
```

GREEN 和回归实际运行：

```powershell
cmake --build build --config Debug --target multimodel_scene_contract_unit_tests production_effective_config_unit_tests model_transform_unit_tests
ctest --test-dir build -C Debug -R "^(multimodel_scene_contract_unit_tests|production_effective_config_unit_tests|model_transform_unit_tests)$" --output-on-failure
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_ci_quick.ps1
git diff --check
```

结果：

```text
三个定向 CTest：3/3 PASS；
Debug 全量构建：PASS；
Debug CTest：60/60 PASS；
Qt self-test：startup、experimental-report-summary PASS；
Quick CI：PASS，包含 regression、schema/golden、UI self-test 和 overlay-load-real；
git diff --check：PASS。
```

## 6. 未完成范围

```text
13B-02 模型列表与实例操作；
13B-03 11x2 规则排版；
13B-04 设备幅面、碰撞和逐实例准入；
13B-05 全局 Raster 与联合层合成；
13B-06 单 package、scene report 和 RIP strict；
13B-07 真实模型与性能矩阵；
Qt 对 Scene Effective Config 的编辑和显示。
```

## 7. 后续 Gate

13B-01 已解除 scene-aware 12E-09A-02 的身份依赖。下一步先执行 09A-02，使诊断 Effective Config
同时兼容 `single_model` 和 `scene`，并绑定 scene/instance/revision。之后再进入 13A-02 和 13B-02，
避免 UI 继续固化为单一 `modelPath`。

设备 `buildVolume.widthMm/heightMm/origin/axis` 仍未冻结，只阻断 13B-04 production 和
13B-07 production GO，不阻断 09A-02、13A-02 或 13B-02 的功能开发。
