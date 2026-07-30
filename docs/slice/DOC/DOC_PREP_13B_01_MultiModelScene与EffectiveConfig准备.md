# DOC_PREP_13B-01 MultiModelScene 与 Scene Effective Config 准备

> 文档状态：IMPLEMENTED / VERIFIED
> 版本：v1.1
> 日期：2026-07-27
> 对应任务：13B-01

## 1. 目标

冻结多模型场景身份、资源作用域、草稿/生产状态和 Effective Config 事务。13B-01 不实现模型列表、
排版、碰撞、联合 Raster 或写包。

## 2. 当前代码事实

```text
SliceConfig 只有一个 input.modelPath；
SceneModel 是单模型 ModelReport；
ConfigDocument/EffectiveConfigGenerator 使用单配置文档；
ProductionSliceRunSession 只记录 mode/profile/session/config/package；
manifest/report 没有 sceneId/modelId/instanceId；
OBJ 的 MTL/贴图按模型同级目录解析，3MF 资源位于包内。
```

因此不能仅把 `input.modelPath` 改成数组；必须先建立 scene identity 和资源隔离。

## 3. P0 决策

### 3.1 场景材料策略

在产品尚未确认“每个实例可否使用不同 MaterialProcessProfile”前，P0 采用保守规则：

```text
materialBindingMode = scene_profile_only；
场景内所有实例共享 slicePipeline.mode、layerHeight、dpiX、dpiY 和 MaterialProcessProfile；
每个实例仍记录 resolvedProfileId，便于审计和未来扩展；
请求不同 Profile 时 fail closed，不做隐式合并。
```

该规则解除 13B-01 schema 阻塞，但不代表未来永远禁止多 Profile。

### 3.2 Build Volume 未知态

设备幅面和轴方向尚未确认，schema 必须表达“未知”，不能用 `0.0` 冒充有效尺寸：

```text
source = unresolved | device_profile | fixture；
widthMm/heightMm = optional；
origin = unresolved | lower_left | center；
xDirection/yDirection = unresolved 或设备/Profile 明确值；
isFixture = true/false。
```

`unresolved` 允许编辑和保存 draft，禁止 production ready。fixture 幅面只用于测试，不得进入正式
设备报告。

## 4. Scene schema

建议冻结：

```json
{
  "schema": "slicesoft.multimodel_scene.13b.1",
  "subjectType": "scene",
  "sceneId": "scene_001",
  "sceneRevision": 1,
  "buildVolume": {
    "source": "unresolved",
    "widthMm": null,
    "heightMm": null,
    "origin": "unresolved",
    "xDirection": "unresolved",
    "yDirection": "unresolved",
    "isFixture": false
  },
  "layout": {
    "policy": "grid",
    "maxColumns": 11,
    "maxRows": 2,
    "columnGapMm": 10.0,
    "rowGapMm": 10.0,
    "spacingMode": "edge_clearance",
    "order": "row_major"
  },
  "materialBindingMode": "scene_profile_only",
  "models": [],
  "instances": []
}
```

`modelId` 和 `instanceId` 使用稳定字符串，不从数组下标生成。删除实例后不得复用旧 instanceId。

## 5. ModelSource 与 ResourceScope

`ModelSource` 至少包含：

```text
modelid；
sourcepath；
format；
resourcescopeid；
sourcehash；
resourcehash；
displayname。
```

`ResourceScope`：

```text
OBJ：源 OBJ 所在目录，MTL 与贴图必须在该作用域内按既有规则解析；
3MF：包路径 + 内部 part/resource identity；
STL：源文件自身；
不同 modelId 即使文件名相同也不得共用相对资源缓存键；
cache key 至少包含 sourceHash + resourceHash + importer contract。
```

路径序列化优先相对 scene 文件或 repository/session resource root；逃逸作用域必须报错。

## 6. ModelInstance

实例引用 13A-01 合同，并补充：

```text
instanceid/modelid；
requestedtransform；
derivedlayouttransform；
effectivetransform；
transformrevision；
visible/locked；
admissionstatus；
resolvedprofileid。
```

合成顺序固定为：

```text
effectiveTransform = derivedLayoutTransform * requestedTransform
```

13B-01 只冻结字段和 revision，不计算排版结果。

## 7. Scene Effective Config

Effective Config 至少记录：

```text
subjectType=scene；
sceneId/sceneRevision；
sourceScenePath/sourceProfileId；
model hashes/resource hashes；
instanceId/transformRevision；
requested/derived/effective buildVolume/layout/transform；
dpiX/dpiY/layerHeight/slicePipeline.mode；
materialBindingMode/resolvedProfileId；
generatedAtUtc/configHash。
```

事务规则：

```text
只写 output/ui_sessions/<session>/scene_config.effective.json；
临时文件 + rename 原子发布；
不覆盖 samples/configs、模型目录或 scene draft；
保存、回读、revert 都保持 sceneId；
sceneRevision 或任一 transformRevision 改变后，旧 effective config 必须 stale；
取消不留下部分文件。
```

## 8. 单模型兼容

旧单模型配置继续是合法输入。适配规则：

```text
subjectType 缺失时按 single_model；
运行时可投影为一个临时 scene/model/instance；
identity instance 不改变当前输出；
只有用户显式“保存为场景”时才写 scene schema；
09A-02 必须同时接受 single_model 和 scene，不强制迁移历史 fixture。
```

## 9. 稳定错误

至少冻结：

```text
SCENE_SCHEMA_UNSUPPORTED；
SCENE_ID_EMPTY；
SCENE_REVISION_STALE；
SCENE_REVISION_INVALID；
MODEL_ID_DUPLICATE；
INSTANCE_ID_DUPLICATE；
INSTANCE_MODEL_REFERENCE_MISSING；
RESOURCE_SCOPE_ESCAPE；
BUILD_VOLUME_UNDEFINED；
SCENE_PROFILE_MISMATCH；
SCENE_EFFECTIVE_CONFIG_WRITE_FAILED。
```

`BUILD_VOLUME_UNDEFINED` 在 draft 保存中是 blocking-for-production，不是编辑失败。

## 10. 测试与 fixture

建议新增：

```text
tests/unit/multimodel_scene_contract/Main.cpp；
tests/golden/expected/13b_scene_effective_config.json；
samples/configs/scene/fixture_single_model_scene.json；
samples/configs/scene/fixture_two_model_scene.json；
samples/configs/scene/bad/。
```

必测：

```text
schema round-trip；
scene/model/instance identity；
duplicate/missing reference；
resource scope escape；
unresolved build volume draft PASS / production BLOCKED；
fixture build volume 显式标记；
same-profile PASS / mixed-profile BLOCKED；
save/readback/revert/cancel；
stale revision；
旧单模型投影等价；
不覆盖 fixture。
```

## 11. 验证命令

实现任务至少运行：

```powershell
cmake --build build --config Debug --target multimodel_scene_contract_unit_tests
ctest --test-dir build -C Debug -R multimodel_scene_contract_unit_tests --output-on-failure
cmake --build build --config Debug --target production_effective_config_unit_tests
ctest --test-dir build -C Debug -R production_effective_config_unit_tests --output-on-failure
git diff --check
```

实现任务已于 2026-07-27 完成。实际验证包括定向三 target/CTest、Debug 全量构建、CTest 60/60、
Qt self-test、Quick CI 和 `git diff --check`，详见
`REPORT_13B_01_MultiModelScene与EffectiveConfig当前状态.md`。

## 12. Gate

13A-01 已于 2026-07-27 完成，以下已实现 Public 合同可直接复用：

```text
slicer_core::ModelTransform；
slicer_core::ModelInstance；
transformRevision 的 optimistic revision 规则；
ModelTransform stable hash/error；
TransformedModelAdapter 的 pivot、winding、UV 和 bbox 语义。
```

13B-01 不得复制上述 DTO，也不得把 requested/derived/effective transform 写回 SourceTransform。
场景扩展采用组合对象：`ModelInstance` 保留最终实例合同，scene 层另行保存 requested/derived，
并在生成 effective config 时计算 effective transform。

因此 13B-01 已完成并解除 scene-aware 12E-09A-02。以下事项不阻断 09A-02 或后续功能开发，
但会阻断多模型生产任务：

```text
设备 width/height/origin/axis：阻断 13B-04 生产准入；
正式多模型性能预算：阻断 13B-07 GO；
多 Profile 产品决策：P0 先按 scene_profile_only，未来变更另立兼容任务。
```
