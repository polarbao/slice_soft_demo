# CODEX_PROMPT_07A_Qt参数编辑与Profile可视化执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议目录：`docs/slicer/`

请先阅读：

```text
docs/slicer/REPORT_07_Qt调试UI当前实现状态.md
docs/slicer/DOC_DECISION_07A_REPORT07后进入Qt参数编辑与Profile可视化增强.md
docs/slicer/ROADMAP_v1.4_REPORT07后续路线_Qt参数编辑与Profile可视化.md
docs/slicer/PRD_07A_Qt参数编辑与Profile可视化增强.md
docs/slicer/DEV_07A_Qt参数编辑与Profile可视化设计.md
docs/slicer/DEMO_07A_Qt参数编辑与Profile可视化验证方案.md
docs/slicer/TASKS_07A_Qt参数编辑与Profile可视化任务清单.md
```

当前阶段：

```text
07A：Qt 参数编辑与 Profile 可视化增强
```

目标：

```text
1. 新增 ConfigDocument / ConfigEditor；
2. 支持 materialProcessProfile 可视化编辑；
3. 支持 materialPolicy 可视化编辑；
4. 支持 materialRoleMapping rules 编辑；
5. 支持 Save / Save As；
6. 支持 per-layer RGB/W/V/S chart；
7. 支持 preview overlay；
8. 支持 profile compare 可视化；
9. 保持 07 已有 QProcess / report / preview 能力。
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

完成后生成：

```text
docs/slicer/REPORT_07A_Qt参数编辑与Profile可视化当前实现状态.md
```
