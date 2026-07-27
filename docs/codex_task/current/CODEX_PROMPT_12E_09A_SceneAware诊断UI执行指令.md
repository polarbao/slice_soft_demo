# CODEX_PROMPT 12E-09A Scene-aware 诊断 UI 执行指令

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
docs/slice/PRD/PRD_12E_09A_SceneAware诊断UI.md
docs/slice/DEV/DEV_12E_09A_SceneAware诊断UI设计.md
docs/slice/DEMO/DEMO_12E_09A_SceneAware诊断UI验证方案.md
docs/slice/DOC/DOC_PREP_12E_09A_02_SceneAwareEffectiveConfig准备.md
docs/slice/REPORT/REPORT_12E_09A_01_只读DiagnosticFacade与UIDTO当前状态.md
docs/slice/REPORT/REPORT_13B_01_MultiModelScene与EffectiveConfig当前状态.md
docs/codex_task/current/TASKS_12E_09A_诊断UI任务清单.md
```

## 当前只执行 09A-02

```text
1. 先写 failing tests 并验证 RED；
2. 实现无 Qt Diagnostic Effective Config；
3. subjectType 支持 single_model/scene；
4. scene 必须绑定 current modelId/instanceId/sceneRevision/transformRevision；
5. 记录 requested/derived/effective，未评估派生值使用 null；
6. 原子写 slice_config.diagnostic.effective.json；
7. 实现回读、hash、stale、cancel 和禁止覆盖源文件；
8. 回归 13B Scene Effective Config 和 09B Production Effective Config；
9. 生成 09A-02 状态报告；
10. 停止，不实现 09A-03。
```

## 禁止范围

```text
不修改 Qt 控件；
不实现 Worker 或 Preview；
不执行排版、联合切片或生产写包；
不修改 production Profile；
不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
不启用 OpenVDB；
不允许 silent fallback。
```

## 验证

```powershell
cmake --build build --config Debug --target diagnostic_effective_config_unit_tests production_effective_config_unit_tests multimodel_scene_contract_unit_tests
ctest --test-dir build -C Debug -R "^(diagnostic_effective_config_unit_tests|production_effective_config_unit_tests|multimodel_scene_contract_unit_tests)$" --output-on-failure
git diff --check
```
