# CODEX_PROMPT 13A-05 模型俯视与变换阶段收口执行指令

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
docs/slice/DOC/DOC_PREP_13A_05_模型俯视与变换阶段收口准备.md
docs/slice/REPORT/REPORT_13A_04_镜像与变换后预检当前状态.md
```

本次只执行 13A-05：

```text
1. 审计 13A-01..04 的需求、实现、测试和阶段边界；
2. 运行全部 13A core 定向测试；
3. 运行 Qt self-test；
4. 运行 model-top-view、model-top-view-transform、model-transform-preflight 三项 UI Smoke；
5. 使用 strict-PASS、纹理正向和 blocked 反向资产复核；
6. 仅修复上述验证暴露的 13A 范围回归；
7. 更新用户操作说明、正式索引、任务看板和 ai_workspace 上下文；
8. 生成 REPORT_13A_模型俯视工作区与实例变换当前状态.md；
9. 给出 M13-1 候选结论；
10. 停止，不实现 13B-02。
```

禁止：

```text
新增多模型列表、排版、联合写包或生产 scene 消费；
自动修复 confirmed self-intersection；
新增 3D 视口、gizmo、Z 编辑或非均匀缩放；
修改 TIFF/RGBWSV、uint8、black_is_print 或材料策略；
把 Legacy PASS 推导为 Global PASS；
把 blocked/stale/cancelled 场景放行到生产。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui transformed_model_preflight_unit_tests scene_transform_controller_unit_tests scene_view_geometry_unit_tests model_transform_unit_tests
ctest --test-dir build -C Debug -R "^(model_transform_unit_tests|scene_view_geometry_unit_tests|scene_transform_controller_unit_tests|model_preflight_admission_unit_tests|model_preflight_service_unit_tests|transformed_model_preflight_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view-transform
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-transform-preflight
.\scripts\run_ci_quick.ps1
git diff --check
```
