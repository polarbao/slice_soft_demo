# CODEX_PROMPT_07B_UI自动化SmokeTest与配置编辑器收口执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议目录：`docs/slicer/`

请先阅读：

```text
docs/slicer/REPORT_07A_Qt参数编辑与Profile可视化当前实现状态.md
docs/slicer/DOC_DECISION_07B_REPORT07A后进入UI自动化SmokeTest与配置编辑器收口.md
docs/slicer/PRD_07B_UI自动化SmokeTest与配置编辑器收口.md
docs/slicer/DEV_07B_UI自动化与配置编辑器收口设计.md
docs/slicer/DEMO_07B_UI自动化SmokeTest验证方案.md
docs/slicer/TASKS_07B_UI自动化SmokeTest与配置编辑器收口任务清单.md
```

当前阶段：

```text
07B：UI 自动化 Smoke Test 与配置编辑器收口
```

目标：

```text
1. 增加 UI smoke test mode；
2. 覆盖 Save As / chart-load / overlay-load / compare-profiles；
3. 增加 Save 覆盖确认；
4. 增加 ConfigDiffPanel；
5. 将关键枚举字段改为下拉；
6. 固化 preview_report schema；
7. 保持 07A 已有 UI 功能；
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
生产级任务系统
完整 3D viewport
支撑形态算法修改
```

完成后生成：

```text
docs/slicer/REPORT_07B_UI自动化SmokeTest与配置编辑器收口当前实现状态.md
```
