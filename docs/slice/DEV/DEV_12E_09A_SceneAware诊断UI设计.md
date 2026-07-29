# DEV 12E-09A Scene-aware 诊断 UI 设计

> 文档版本：v1.0
> 文档状态：Formal DEV / 09A-05 COMPLETE / 09A-06 READY
> 日期：2026-07-29

## 1. 架构边界

```text
slicer_core：诊断配置 DTO、schema、validation、identity/stale，不依赖 Qt；
apps/slicer_debug_ui/services：文件事务、当前 session/scene/instance 绑定；
apps/slicer_debug_ui/widgets：09A-03 中文控件；
apps/slicer_debug_ui/workers：09A-04 异步分析；
13C TiffLayerSource：09A-05 生产 TIFF 底图；
TextureFillPartitionDiagnosticFacade：只读业务结果，不拥有写包能力。
```

09A 不复制 09B 的 Production Effective Config，也不修改 `MultiModelScene`。诊断配置只引用 scene
identity，不内嵌一份可漂移的完整 scene。

## 2. Diagnostic Effective Config schema

建议冻结：

```text
schema = slicesoft.diagnostic_effective_config.12e_09a.1
subjectType = single_model | scene
sessionId
sourceProfileId
generatedAtUtc
requested
derived
effective
identity
configHash
```

`identity`：

```text
single_model：
  modelPath / modelHash / sourceConfigHash

scene：
  sceneId / sceneRevision / sceneHash
  currentModelId / currentInstanceId / transformRevision
```

`requested`：

```text
textureSurfaceWidthMm
modelFillMaterial
diagnosticBackendRequest
```

`derived`：

```text
minimumWidthMm
maximumWidthMm
allTextureThresholdMm
backendAvailability
derivationSource
```

`effective`：

```text
textureSurfaceWidthMm
modelFillMaterial
diagnosticBackend
resolvedProfileId
```

未评估的派生值使用 `null`，不能用 0 冒充结果。

## 3. 身份解析

新增无 Qt `DiagnosticSubjectIdentity`：

```text
subjecttype；
sessionid；
modelpath/modelhash/sourceconfighash；
sceneid/scenerevision/scenehash；
modelid/instanceid/transformrevision。
```

规则：

```text
single_model 禁止携带 scene/instance 字段；
scene 必须携带 scene、model 和 current instance 全部身份；
scene identity 使用 13B-01 的 ComputeMultiModelSceneHash；
revision 必须保持 JSON 精确整数范围；
切换 current instance 必须生成新 effective config；
旧结果不得仅凭 modelPath 命中。
```

## 4. 事务

输出固定为：

```text
output/ui_sessions/<session>/slice_config.diagnostic.effective.json
```

事务步骤：

```text
1. 校验 requested 和 subject identity；
2. 解析 Profile 与派生阈值；
3. 生成 canonical JSON；
4. 计算不含 configHash 字段的 SHA-256；
5. 写同目录 staging；
6. 回读并复核 schema/hash/identity；
7. rename 原子发布；
8. 失败或取消清理 staging，保留旧成功文件。
```

不得覆盖：

```text
samples/configs；
source Profile；
模型资源；
scene draft；
scene_config.effective.json；
09B production effective config。
```

## 5. stale 规则

任一条件变化即 stale：

```text
subjectType；
sessionId；
source Profile/hash；
model path/hash/config hash；
sceneId/revision/hash；
current modelId/instanceId/transformRevision；
requested diagnostic fields；
derived threshold source/version。
```

stale 只表示旧诊断配置不可复用，不修改生产 Profile，也不自动运行分析。

## 6. 稳定错误

至少冻结：

```text
DIAGNOSTIC_CONFIG_SCHEMA_UNSUPPORTED
DIAGNOSTIC_SUBJECT_INVALID
DIAGNOSTIC_SCENE_IDENTITY_REQUIRED
DIAGNOSTIC_INSTANCE_REFERENCE_MISSING
DIAGNOSTIC_REVISION_STALE
DIAGNOSTIC_PROFILE_MISMATCH
DIAGNOSTIC_REQUEST_INVALID
DIAGNOSTIC_DERIVATION_UNAVAILABLE
DIAGNOSTIC_CONFIG_CANCELLED
DIAGNOSTIC_CONFIG_INTEGRITY_FAILED
DIAGNOSTIC_CONFIG_WRITE_FAILED
```

## 7. 09A-03..05 预留接口

```text
UI Model 只消费稳定 DTO，不解析 JSON；
Worker request 持有 immutable identity/configHash；
Worker result 回 UI 前再次校验 identity/configHash；
Preview 使用真实 layerIndex/zMm 和 13C TIFF 数据源；
诊断 mask 只作叠加，不写回生产 TIFF；
窗口关闭或模型切换时取消并丢弃 stale result。
```

## 8. 测试设计

计划 target：

```text
diagnostic_effective_config_unit_tests
```

必测：

```text
single_model/scene 正向 round-trip；
scene current instance 绑定；
scene/instance/revision/Profile 不匹配；
null derived value；
hash 篡改；
save/readback/revert/cancel；
禁止覆盖 fixture/scene/production config；
source 无修改；
现有 production_effective_config_unit_tests 回归。
```

## 9. 文件所有权

09A-02 建议：

```text
src/slicer_core/pipeline/DiagnosticEffectiveConfig.*
tests/unit/diagnostic_effective_config/Main.cpp
samples/configs/diagnostic_effective/
CMakeLists.txt
```

Qt 接线留到 09A-03，不在 09A-02 夹带控件或 Worker。

## 10. 09A-03 实际 UI 落点

```text
DiagnosticSettingsPanel：拥有中文宽度、材料和只读诊断状态；
ContextInspector：在“切片设置”页托管面板并转发自定义信号；
MainWindow：绑定当前 scene/instance/revision、候选工具可用状态和 UI 会话 requested 值；
UiSmokeTestRunner：验证 0.01 mm、双向同步、最长中文、不可用状态和三窗口尺寸。
```

09A-03 不直接更新 `ConfigDocument`，避免 diagnostic 编辑污染生产 Profile。09A-04 启动分析前必须
调用 09A-02 的专用事务生成 `slice_config.diagnostic.effective.json`。

## 11. 09A-05/06 实施入口

```text
docs/slice/DOC/DOC_PREP_12E_09A_05_同层语义Preview准备.md
docs/slice/DOC/DOC_PREP_12E_09A_06_诊断UI阶段收口准备.md
docs/codex_task/current/CODEX_PROMPT_12E_09A_05_同层语义Preview执行指令.md
docs/codex_task/current/CODEX_PROMPT_12E_09A_06_诊断UI阶段收口执行指令.md
```

09A-05 只新增只读同层适配和 UI buffer；09A-06 只收口回归与文档。
