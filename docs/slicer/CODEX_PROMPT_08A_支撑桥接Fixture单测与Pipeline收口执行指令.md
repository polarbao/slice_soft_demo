# CODEX_PROMPT_08A_支撑桥接Fixture单测与Pipeline收口执行指令

> 文档版本：v0.1
> 用途：复制给 VS Code Codex
> 适用阶段：08A
> 建议提交目录：`docs/slicer/`

请先阅读：

```text
docs/slicer/REPORT_08_支撑形态与工艺优化当前状态.md
docs/slicer/DOC_DECISION_08A_REPORT08后进入支撑桥接Fixture与单测收口.md
docs/slicer/PRD_08A_支撑桥接Fixture单测与真实模型Profile收口.md
docs/slicer/DEV_08A_支撑桥接Fixture单测与Pipeline收口设计.md
docs/slicer/DEMO_08A_支撑桥接Fixture与单测验证方案.md
docs/slicer/TASKS_08A_支撑桥接Fixture单测与真实模型Profile任务清单.md
```

当前阶段：

```text
08A：支撑桥接 Fixture、单元测试与真实模型 Profile 收口
```

目标：

```text
1. 新增 bridge gap 专用 fixture；
2. 让 bridgedGaps 在 golden 中稳定非零；
3. 新增 support_shape_unit_tests；
4. 将 SupportShapeOptimizer 进一步封装到 support/pipeline wrapper；
5. 新增至少一个真实 3MF support shape profile；
6. 保持 RGBWSV 协议不变。
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
.\build\Debug\support_shape_unit_tests.exe
.\build\Debug\slicer_cli.exe --config samples\configs\support\support_bridge_gap_smoke.json
.\build\Debug\rip_reader_test.exe --package output\SupportBridgeGapSmoke --summary
.\scripts\run_support_shape_tests.ps1
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\scripts\run_ci_quick.ps1
```

完成后生成：

```text
docs/slicer/REPORT_08A_支撑桥接Fixture单测与真实模型Profile当前状态.md
```

报告最后判断是否进入：

```text
09：OpenVDB / SDF 几何内核预研
```
