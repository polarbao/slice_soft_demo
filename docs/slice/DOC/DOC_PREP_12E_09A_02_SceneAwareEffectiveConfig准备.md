# DOC_PREP 12E-09A-02 Scene-aware Diagnostic Effective Config 准备

> 文档状态：IMPLEMENTED / COMPLETE
> 日期：2026-07-27
> 前置：09A-01、13A-01、13B-01 COMPLETE
> 实现提交：`5d67ea8 feat(12E-09A-02): 建立场景感知诊断生效配置合同`
> 状态报告：`docs/slice/REPORT/REPORT_12E_09A_02_SceneAwareDiagnosticEffectiveConfig当前状态.md`
> 后续：13A-02 READY；09A-03 按 Stage 13 依赖顺序执行

## 1. 本任务只做什么

```text
建立 Diagnostic Effective Config 无 Qt 合同；
兼容 subjectType=single_model|scene；
scene 绑定 current model/instance/revision；
保存 requested/derived/effective；
原子保存、回读、回退、取消、完整性和 stale；
新增正负 fixture 与定向单测。
```

## 2. 本任务不做什么

```text
不新增 Qt 控件；
不启动异步分析；
不实现同层预览；
不执行排版或联合切片；
不写 production package/TIFF；
不修改 09B Production Effective Config；
不修改 MultiModelScene 或生产 Profile。
```

## 3. 复用合同

```text
09A-01 TextureFillPartitionDiagnosticFacade；
13A-01 ModelInstance/transformRevision；
13B-01 MultiModelScene/sceneRevision/sceneHash/ResourceScope；
现有 SHA-256 与 Json；
现有 Production Effective Config 的事务原则，但不共用 schema 或文件名。
```

## 4. 输出

```text
schema=slicesoft.diagnostic_effective_config.12e_09a.1；
output/ui_sessions/<session>/slice_config.diagnostic.effective.json；
无 Qt core API；
diagnostic_effective_config_unit_tests；
single_model/scene/bad fixtures；
REPORT_12E_09A_02_SceneAwareDiagnosticEffectiveConfig当前状态.md。
```

## 5. RED/GREEN 顺序

```text
1. RED：single_model 与 scene round-trip；
2. GREEN：DTO、codec、validator；
3. RED：scene current instance/revision/profile mismatch；
4. GREEN：identity 和 stable errors；
5. RED：save/readback/tamper/cancel/source overwrite；
6. GREEN：原子事务与 stale；
7. 回归 13B-01 和 production effective config。
```

## 6. 稳定错误

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

## 7. 文件边界

建议所有权：

```text
src/slicer_core/pipeline/DiagnosticEffectiveConfig.h
src/slicer_core/pipeline/DiagnosticEffectiveConfig.cpp
tests/unit/diagnostic_effective_config/Main.cpp
samples/configs/diagnostic_effective/
CMakeLists.txt
```

不得修改 `apps/slicer_debug_ui`；UI 接线属于 09A-03。

## 8. 验证

```powershell
cmake --build build --config Debug --target diagnostic_effective_config_unit_tests production_effective_config_unit_tests multimodel_scene_contract_unit_tests
ctest --test-dir build -C Debug -R "^(diagnostic_effective_config_unit_tests|production_effective_config_unit_tests|multimodel_scene_contract_unit_tests)$" --output-on-failure
git diff --check
```

## 9. Gate 与结果

13B-01 已提供 scene identity，09A-02 已完成实现和验证。设备 buildVolume、22 实例性能预算和
13C TIFF Preview 未阻断本任务；后续 UI 控件、Worker 和同层预览仍按各自依赖推进。
