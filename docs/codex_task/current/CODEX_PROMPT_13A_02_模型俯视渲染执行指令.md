# CODEX_PROMPT 13A-02 模型俯视渲染执行指令

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
docs/slice/DOC/DOC_PREP_13A_02_模型俯视渲染准备.md
docs/slice/REPORT/REPORT_12E_09A_02_SceneAwareDiagnosticEffectiveConfig当前状态.md
docs/slice/REPORT/REPORT_13B_01_MultiModelScene与EffectiveConfig当前状态.md
docs/codex_task/current/TASKS_13_模型场景排版联合切片与TIFF预览任务清单.md
```

## 当前只执行 13A-02

```text
1. 先写 SceneViewGeometry failing tests 并验证 RED；
2. 实现无 Qt +Z 投影 DTO/builder；
3. 新增可取消、generation-aware 的 Qt loader；
4. 新增 ModelTopViewWidget 和独立“导入模型预览”入口；
5. 显示毫米网格、XY 轴、轮廓、bbox、identity、selection 和 blocked；
6. 确认导入预览不启动 slicer_cli；
7. 新增 model-top-view UI smoke；
8. 回归 13A-01、13B-01、09A-02 和现有一键切片；
9. 生成 REPORT_13A_02_模型俯视渲染当前状态.md；
10. 停止，不实现 13A-03。
```

## 禁止范围

```text
不实现模型移动、旋转、缩放或镜像编辑；
不实现模型列表、复制、删除、锁定或自动排版；
不引入 VTK/Qt3D/OpenVDB；
不在 UI 线程加载和投影大模型；
不修改生产 Profile/package/TIFF；
不修改 p0.rgbwsv.2、RGBWSV、uint8 或 black_is_print；
不允许 blocked 模型显示为 production PASS。
```

## 验证

```powershell
cmake --build build --config Debug --target scene_view_geometry_unit_tests slicer_debug_ui
ctest --test-dir build -C Debug -R "^(scene_view_geometry_unit_tests|model_transform_unit_tests|multimodel_scene_contract_unit_tests|diagnostic_effective_config_unit_tests)$" --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case model-top-view
.\scripts\run_ci_quick.ps1
git diff --check
```
