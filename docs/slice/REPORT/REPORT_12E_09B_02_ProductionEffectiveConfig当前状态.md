# REPORT 12E-09B-02 Production Effective Config 当前状态

> 状态：COMPLETE
> 日期：2026-07-24
> 下一原子任务：12E-09B-03 中文模式/Profile 选择器与状态

## 1. 任务目标

本任务把 09B-01 冻结的产品模式、Production Profile 和能力目录写入 session-scoped
Effective Config。生成结果继续使用核心正式字段，不创建第二套切片配置协议：

```text
slicePipeline.mode；
materialProcessProfile.target；
uiAudit.production（只读审计投影，不参与核心准入）。
```

## 2. 已实现内容

```text
Legacy 默认显式写入 slicePipeline.mode=legacy；
Legacy 保留现有 materialProcessProfile.target，不清除 Profile 自有配置；
Global 必须显式请求能力目录中的 restricted 或 material-parity Profile；
Global 请求必须与只读源 Profile 的 mode/target 一致，否则 fail-closed；
Global 的纹理壳层、Model Fill、材料 Profile、支撑、光油、材料闭环和实验段按只读源 Profile 锁定；
禁用或不支持的 stale override 在 session copy 中恢复，并记录 disabledOverrides[]；
requested/effective mode/Profile、capability lock、模型、模板、session 和生成时间写入审计投影；
生成路径统一为 output/ui_sessions/<session>/slice_config.effective.json；
继续使用 QSaveFile 原子提交，错误发生时不替换已有 session 文件；
samples/configs Profile fixture 保持只读。
```

## 3. 审计合同

审计对象固定为：

```text
uiAudit.production.schema
uiAudit.production.sourceProfileId
uiAudit.production.requestedPipelineMode
uiAudit.production.effectivePipelineMode
uiAudit.production.requestedProductionProfileId
uiAudit.production.effectiveProductionProfileId
uiAudit.production.capabilityLockVersion
uiAudit.production.disabledOverrides[]
uiAudit.production.sourceModelPath
uiAudit.production.sourceTemplatePath
uiAudit.production.sessionId
uiAudit.production.generatedAtUtc
```

`uiAudit.production` 只用于 UI、测试和问题追踪。核心 Router 和生产准入仍只消费
`slicePipeline`、`materialProcessProfile` 及既有正式配置字段。

## 4. 代码落点

```text
apps/slicer_debug_ui/services/EffectiveConfigGenerator.h/.cpp
apps/slicer_debug_ui/services/ConfigValidator.cpp
apps/slicer_debug_ui/MainWindow.cpp
apps/slicer_debug_ui/services/UiSmokeTestRunner.cpp
apps/slicer_debug_ui/CMakeLists.txt
tests/unit/production_effective_config/Main.cpp
```

`ConfigValidator` 同步认可 08D 正式使用的
`modelFill.scope=complement_of_global_texture_shell`，不改变 Legacy 既有 scope。

## 5. 负向行为

以下情况在写文件前阻断：

```text
未知产品模式；
Global 未显式选择 Production Profile；
未知 Global Production Profile；
请求模式与能力目录 Profile 不匹配；
请求 Profile 与只读源 Profile mode/target 不匹配；
既有配置校验失败；
尝试覆盖源 Profile fixture。
```

阻断后不生成半文件，也不替换已有 session Effective Config。

## 6. 验证结果

2026-07-24 实际执行：

```text
cmake --build build --config Debug --target
  slicer_debug_ui
  production_mode_catalog_unit_tests
  production_effective_config_unit_tests
  slice_pipeline_router_unit_tests
  global_surface_shell_production_pipeline_unit_tests
PASS

ctest --test-dir build -C Debug -R
  (production_mode_catalog|production_effective_config|
   slice_pipeline_router|global_surface_shell_production_pipeline)
4/4 PASS

build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe --self-test
PASS

build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe
  --ui-smoke-test --case generated-effective-config
PASS

ctest --test-dir build -C Debug --output-on-failure
52/52 PASS

scripts/run_ci_quick.ps1
PASS
```

单测覆盖 Legacy passthrough、Global restricted stale S/V 清除、Global material-parity
合同恢复、未知 Profile、Profile mismatch、原子写入和 fixture 只读。

## 7. 当前边界

本任务没有增加可见模式选择器，也没有把一键切片路由到 Global。可见中文选择器、能力禁用提示属于
09B-03；实际一键路由、session admission 和 no-fallback 属于 09B-04。
