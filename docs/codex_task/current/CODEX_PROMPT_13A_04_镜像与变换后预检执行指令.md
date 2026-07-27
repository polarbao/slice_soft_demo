# CODEX_PROMPT 13A-04 镜像与变换后预检执行指令

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
docs/slice/DOC/DOC_PREP_13A_04_镜像与变换后预检准备.md
docs/slice/REPORT/REPORT_13A_03_选择与精确变换当前状态.md
```

本次只执行 13A-04：

```text
1. 先写 transformed preflight/controller failing tests 并验证 RED；
2. 在 slicer_core 新增无 Qt 的 transformed model preflight 入口；
3. 输入必须是只读 SceneModel、有效 ModelInstance、options、identity/revision 和 cancellation；
4. 复用 TransformedModelAdapter、既有拓扑诊断和 EvaluateModelPreflightAdmissions；
5. 输出 source/transformed 结果、Legacy/Global admission、hash 和稳定 blocker；
6. 在 SceneDocument 增加 generation/revision 绑定的 transformed preflight 状态；
7. 实现 mirrorX/mirrorY 命令，不修改源模型；
8. 镜像后异步重投影并异步重新预检，只接受最新 generation/revision；
9. 变换后 PENDING/FAILED/BLOCKED/stale 时生产动作 fail-closed；
10. ModelTransformPanel 增加 X/Y 镜像和源/变换后状态；
11. 新增 transformed_model_preflight_unit_tests 与 model-transform-preflight UI Smoke；
12. 回归 13A-01/02/03、13B-01、09A-02 和 Quick CI；
13. 生成 REPORT_13A_04_镜像与变换后预检当前状态.md；
14. 停止，不实现 13A-05 或 13B。
```

13A-03 实际可复用 API：

```text
SceneModelRepository::Find -> shared_ptr<const SceneModel>；
SceneDocument::Instance/SceneRevision/SourceCacheKey/CommitInstance；
SceneTransformController::SetTransform；
ModelTopViewLoader::RequestProjection；
SceneProjectionRequest；
ModelTransformPanel；
SceneTransformSaveRequest/SaveSceneEffectiveConfig。
```

禁止：

```text
修改源 OBJ/STL/3MF；
自动修复 confirmed self-intersection；
从屏幕投影或旧 SliceConfig 推断有效几何；
把 Legacy PASS 推导为 Global PASS；
blocked 时写生产 package；
多模型列表、排版、联合切片、3D 视口；
修改 TIFF/RGBWSV、uint8、black_is_print 或材料策略。
```

验证：

```powershell
cmake --build build --config Debug --target transformed_model_preflight_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(model_transform_unit_tests|scene_view_geometry_unit_tests|scene_transform_controller_unit_tests|model_preflight_admission_unit_tests|model_preflight_service_unit_tests|transformed_model_preflight_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-transform-preflight
.\scripts\run_ci_quick.ps1
git diff --check
```
