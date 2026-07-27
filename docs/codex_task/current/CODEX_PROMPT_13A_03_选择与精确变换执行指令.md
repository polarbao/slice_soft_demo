# CODEX_PROMPT 13A-03 选择与精确变换执行指令

请先阅读：

```text
AGENTS.md
.agents/AGENTS.md
.agents/docs/architecture-boundary.md
.agents/docs/build-and-test.md
.agents/docs/code-standards.md
docs/slice/PRD/PRD_13A_模型俯视工作区与实例变换.md
docs/slice/DEV/DEV_13A_模型俯视渲染与变换架构设计.md
docs/slice/DEMO/DEMO_13A_模型俯视与变换验证方案.md
docs/slice/DOC/DOC_PREP_13A_03_选择与精确变换准备.md
docs/slice/REPORT/REPORT_13A_02_模型俯视渲染当前状态.md
docs/slice/REPORT/REPORT_13B_01_MultiModelScene与EffectiveConfig当前状态.md
```

本次只执行 13A-03：

```text
1. 先写 controller/document failing tests 并验证 RED；
2. 建立只读 SceneModel repository/cache，不在 UI 主线程重复导入；
3. 扩展 SceneDocument 的单实例、revision、dirty/stale 和 session 状态；
4. 实现 SceneTransformController；
5. 实现 X/Y、rotateZ、uniformScale、场景原点居中和重置；
6. locked、非法值、stale revision fail-closed；
7. 异步重投影只接受最新 generation/revision；
8. 单实例场景配置原子保存、回读、取消和回滚；
9. 新增 ModelTransformPanel 和 model-top-view-transform UI Smoke；
10. 回归 13A-01/02、13B-01、09A-02 和一键切片；
11. 生成 REPORT_13A_03_选择与精确变换当前状态.md；
12. 停止，不实现 13A-04。
```

禁止：

```text
mirrorX/mirrorY；
post-transform preflight；
Z 平移或非均匀缩放；
鼠标 gizmo/拖拽变换；
多模型列表、自动排版或联合切片；
覆盖源 Profile、samples/configs 或模型资产；
修改生产 TIFF/RGBWSV 协议；
在 UI 主线程导入或重投影大模型。
```

验证：

```powershell
cmake --build build --config Debug --target scene_transform_controller_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(model_transform_unit_tests|scene_view_geometry_unit_tests|multimodel_scene_contract_unit_tests|diagnostic_effective_config_unit_tests|scene_transform_controller_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view-transform
.\scripts\run_ci_quick.ps1
git diff --check
```
