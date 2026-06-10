# CODEX_PROMPT_R2_配置报告测试CI工程化执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 适用阶段：R2  
> 建议提交目录：`docs/slicer/`

请先阅读：

```text
docs/slicer/REPORT_R1_核心模块边界重构当前状态.md
docs/slicer/DOC_DECISION_R2_R1后进入配置报告测试CI工程化固化阶段.md
docs/slicer/PRD_R2_配置报告测试CI工程化固化.md
docs/slicer/DEV_R2_配置报告测试CI工程化设计.md
docs/slicer/DEMO_R2_配置报告测试CI验证方案.md
docs/slicer/TASKS_R2_配置报告测试CI工程化任务清单.md
```

当前阶段：

```text
R2：配置、报告、测试与 CI 工程化固化
```

R2 目标：

```text
1. 新增 config schema / migration skeleton；
2. 支持 slicer.config.1 样例；
3. 保持 legacy config 兼容；
4. 新增 report base helper；
5. 小幅增强 diagnostics；
6. 新增 schema tests；
7. 新增 golden tests；
8. 新增 run_ci_quick.ps1；
9. 保持 quick regression / UI smoke 通过。
```

必须保持：

```text
p0.rgbwsv.2 输出协议不变；
R G B W S V 通道顺序不变；
8-bit / black_is_print 极性不变；
slicer_cli / rip_reader_test 基本调用方式不变；
MaterialPolicy / MaterialRoleMapping / MaterialProcessProfile 语义不变。
```

不要做：

```text
surface_shell_texture 实现；
compensated_varnish 实现；
OpenVDB；
设备通信；
RIP 半色调；
ICC / CMYK；
生产级任务系统；
大规模重写 parser；
删除 legacy config 支持。
```

必须执行验证：

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -Mode quick
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
.\scripts\run_ci_quick.ps1
```

完成后生成：

```text
docs/slicer/REPORT_R2_配置报告测试CI工程化当前状态.md
```
