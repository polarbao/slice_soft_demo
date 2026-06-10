# CODEX_PROMPT_R1_核心模块边界重构执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 适用阶段：R1  
> 建议提交目录：`docs/slicer/`

---

请先阅读：

```text
docs/slicer/REPORT_R0_正式项目架构审查与重构设计当前状态.md
docs/slicer/DOC_DECISION_R1_R0后进入核心模块边界重构阶段.md
docs/slicer/DEV_R1_核心模块边界重构设计.md
docs/slicer/TASKS_R1_核心模块边界重构任务清单.md
docs/slicer/DEMO_R1_重构守门验证方案.md
docs/slicer/R2_BOUNDARY_配置报告测试CI工程化边界说明.md
```

当前阶段：

```text
R1：核心模块边界重构
```

R1 目标：

```text
1. 建立正式模块目录；
2. 建立 scene/importers/pipeline/materials/support/raster/output/reports/diagnostics 边界；
3. 将 model.cpp / slicer.cpp 的职责逐步 wrapper 化；
4. 先 wrapper，再 move，最后才 rewrite；
5. 保持 quick regression 通过；
6. 不新增大型功能。
```

必须保持：

```text
p0.rgbwsv.2 输出协议不变；
R G B W S V 通道顺序不变；
8-bit / black_is_print 极性不变；
slicer_cli / rip_reader_test / slicer_debug_ui 可构建；
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
重写全部 parser；
一次性清空 model.cpp / slicer.cpp。
```

执行顺序：

```text
1. 新建目录与空模块；
2. 建立 SceneModel 边界；
3. 建立 Obj/Mtl/ThreeMf importer wrapper；
4. 建立 PipelineContext / PipelineStepResult / SlicePipeline；
5. 建立 Materials 策略模块；
6. 建立 Support/Raster/Output/Reports wrapper；
7. 标记 legacy 留存职责；
8. 每个阶段运行 quick regression；
9. 生成 REPORT_R1。
```

必须执行验证：

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -Mode quick
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
```

完成后生成：

```text
docs/slicer/REPORT_R1_核心模块边界重构当前状态.md
```
