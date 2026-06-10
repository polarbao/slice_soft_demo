# CODEX_PROMPT_08_支撑形态与工艺优化执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 阶段：08  
> 建议目录：`docs/slicer/`

请先阅读：

```text
docs/slicer/REPORT_R2_配置报告测试CI工程化当前状态.md
docs/slicer/DOC_DECISION_08_R2后进入支撑形态与工艺优化阶段.md
docs/slicer/PRD_08_支撑形态与工艺优化.md
docs/slicer/DEV_08_支撑形态与工艺优化设计.md
docs/slicer/DEMO_08_支撑形态与工艺优化验证方案.md
docs/slicer/TASKS_08_支撑形态与工艺优化任务清单.md
```

当前阶段：

```text
08：支撑形态与工艺优化
```

目标：

```text
1. 新增 SupportShapePolicy；
2. 新增支撑组件分析；
3. 新增小组件过滤；
4. 新增支撑 dilation / closing / bridge gap；
5. 新增 support_shape_report；
6. 新增 support_shape_smoke 样例；
7. 接入 schema/golden/ci quick；
8. 保持 RGBWSV 协议不变。
```

必须保持：

```text
p0.rgbwsv.2 输出协议不变
R G B W S V 通道顺序不变
8-bit / black_is_print 极性不变
Model > Support > Empty 优先级不变
SupportType 不进入 TIFF channel
MaterialPolicy / MaterialRoleMapping / MaterialProcessProfile 语义不变
```

不要做：

```text
surface_shell_texture
compensated_varnish
OpenVDB / SDF
设备通信
RIP 半色调
ICC / CMYK
树状支撑
生产级 Qt UI
```

必须执行验证：

```powershell
cmake --build build --config Debug
.\build\Debug\slicer_cli.exe --config samples\configs\support\support_shape_smoke.json
.\build\Debug\rip_reader_test.exe --package output\SupportShapeSmoke --summary
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\scripts\run_ci_quick.ps1
```

完成后生成：

```text
docs/slicer/REPORT_08_支撑形态与工艺优化当前状态.md
```
