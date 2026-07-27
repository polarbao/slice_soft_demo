# REPORT 12E-09A-02 Scene-aware Diagnostic Effective Config 当前状态

> 日期：2026-07-27
> 状态：COMPLETE
> 提交：`5d67ea8 feat(12E-09A-02): 建立场景感知诊断生效配置合同`
> 下一任务：13A-02 模型俯视渲染

## 1. 结论

12E-09A-02 已完成无 Qt 的诊断生效配置合同。诊断链路现在可以用同一 schema 表达单模型和
多模型场景中的当前实例，并以稳定身份、revision 和 SHA-256 拒绝跨模型、跨实例或过期配置复用。

固定输出为：

```text
output/ui_sessions/<session>/slice_config.diagnostic.effective.json
schema=slicesoft.diagnostic_effective_config.12e_09a.1
```

本任务没有新增 Qt 控件、分析 Worker 或同层预览，也没有写入生产 package/TIFF。

## 2. 已实现能力

### 2.1 Subject 身份

```text
single_model：
  sessionId
  modelPath
  modelHash
  sourceConfigHash
  sourceProfileId

scene：
  sessionId
  sceneId
  sceneRevision
  sceneHash
  currentModelId
  currentInstanceId
  transformRevision
  sourceProfileId
```

scene 请求必须引用 13B-01 `MultiModelScene` 中真实存在的 current instance，并且
`currentModelId`、scene Profile、instance Profile 和 source Profile 必须一致。

### 2.2 诊断参数

配置分别保存：

```text
requested：
  textureSurfaceWidthMm
  modelFillMaterial
  diagnosticBackendRequest

derived：
  minimumWidthMm
  maximumWidthMm
  allTextureThresholdMm
  backendAvailability
  derivationSource

effective：
  textureSurfaceWidthMm
  modelFillMaterial
  diagnosticBackend
  resolvedProfileId
```

未评估的派生数值使用 JSON `null`，不以 `0` 冒充已有结果。有效宽度超出派生上下界时
fail-closed。

### 2.3 文件事务

```text
固定 session 文件名；
同目录 staging 写入；
staging 回读和 hash 复核；
原子 rename 发布；
失败时保留旧成功文件；
取消不写 staging/final；
禁止覆盖 source config 或单模型源文件；
不使用旧 slice_config.effective.json 文件名。
```

### 2.4 stale 判定

以下任一项变化后，旧诊断配置不可复用：

```text
subjectType/sessionId；
source Profile/sourceConfigPath；
单模型 path/hash/config hash；
sceneId/sceneRevision/sceneHash；
current model/instance/transformRevision；
requested/derived/effective；
configHash 完整性。
```

## 3. 稳定错误

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

## 4. 修改文件

```text
src/slicer_core/pipeline/DiagnosticEffectiveConfig.h
src/slicer_core/pipeline/DiagnosticEffectiveConfig.cpp
tests/unit/diagnostic_effective_config/Main.cpp
samples/configs/diagnostic_effective/single_model_request.json
samples/configs/diagnostic_effective/scene_instance_a_request.json
samples/configs/diagnostic_effective/bad/missing_instance.json
CMakeLists.txt
```

新增测试 target：

```text
diagnostic_effective_config_unit_tests
```

## 5. TDD 证据

实际 RED：

```text
首次构建因 slicer_core/pipeline/DiagnosticEffectiveConfig.h 不存在而失败；
新增 configHash-only tamper 用例后，stale 判定错误返回 false。
```

实际 GREEN：

```text
实现 DTO/schema/validator/codec/事务后定向测试通过；
stale 判定增加独立 hash 复核后篡改用例通过。
```

## 6. 实际验证

已运行：

```powershell
cmake --build build --config Debug --target diagnostic_effective_config_unit_tests production_effective_config_unit_tests multimodel_scene_contract_unit_tests
ctest --test-dir build -C Debug -R "^(diagnostic_effective_config_unit_tests|production_effective_config_unit_tests|multimodel_scene_contract_unit_tests)$" --output-on-failure
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\scripts\run_ci_quick.ps1
```

结果：

```text
定向 CTest：3/3 PASS；
Debug 全量构建：PASS；
Debug CTest：61/61 PASS；
Qt self-test：PASS；
Quick CI：PASS。
```

## 7. 协议与边界

保持不变：

```text
p0.rgbwsv.2；
R G B W S V；
uint8；
black_is_print；
Legacy 默认；
OpenVDB 默认关闭；
诊断配置不等于 production admission。
```

## 8. 下一步

按跨阶段固定顺序，下一原子任务是 `13A-02 模型俯视渲染`。它将消费 13A-01 的实例变换 DTO、
13B-01 的 scene identity 和本任务的诊断身份边界，但不在 09A-02 中夹带 Qt 功能。

09A-03 已解除 09A-02 功能依赖，但仍按单贡献者路线排在 13C-01..03 之后，以免诊断 UI 再次绑定旧
Preview PNG 数据链路。
