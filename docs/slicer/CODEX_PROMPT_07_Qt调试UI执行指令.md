# CODEX_PROMPT_07_Qt调试UI执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

请先阅读：

```text
docs/slicer/REPORT_05A_真实材料工艺参数验证当前实现状态.md
docs/slicer/DOC_DECISION_07_REPORT05A后进入Qt调试UI阶段.md
docs/slicer/ROADMAP_v1.3_REPORT05A后续路线_Qt调试UI.md
docs/slicer/PRD_07_Qt调试UI基础版.md
docs/slicer/DEV_07_Qt调试UI架构设计.md
docs/slicer/DEMO_07_Qt调试UI验证方案.md
docs/slicer/TASKS_07_Qt调试UI任务清单.md
```

当前阶段是：

```text
07：Qt 调试 UI 基础版
```

目标：

```text
1. 新增 apps/slicer_debug_ui；
2. 使用 Qt 5.15 Widgets；
3. QProcess 包装 slicer_cli / rip_reader_test / run_regression / compare_material_profiles；
4. 查看 manifest / reports / preview；
5. 查看 material_process_report summary；
6. 支持 profile compare；
7. 支持 quick regression 按钮；
8. 不修改 slicer_core 输出协议。
```

必须保持：

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
black_is_print
Model > Support > Empty
MaterialRoleMapping 语义不变
MaterialPolicy 语义不变
MaterialProcessProfile 语义不变
S support 仍由 Support pipeline 独立生成
```

不要做：

```text
设备通信
喷头 bitstream
RIP 半色调
ICC / CMYK
OpenVDB
新的切片算法
复杂 3MF 材料语义
生产级任务系统
完整 3D viewport
```

优先实现：

```text
1. Qt target 可构建；
2. MainWindow；
3. ProcessRunner；
4. Run Slicer / Run RIP；
5. PackageLoader / ReportLoader；
6. MaterialProcessPanel；
7. PreviewPanel；
8. Profile Compare；
9. Quick Regression；
10. REPORT_07_Qt调试UI当前实现状态.md。
```
