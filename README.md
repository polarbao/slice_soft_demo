# slice_soft_demo

这是 UV 3D 打印切片软件的 P0 Demo 仓库初始化包。

当前目标不是完整全彩切片器，而是先跑通最小数据闭环：

```text
STL / OBJ 单材料模型
→ C++ slicer_cli
→ 体素/层采样
→ 下表面投影支撑
→ RGBWSV 六通道 uint16 tiled TIFF
→ manifest.json
→ rip_reader_test 验证
```

## Codex 接手顺序

请在 VS Code Codex 中先让 Codex 阅读：

1. `AGENTS.md`
2. `docs/slicer/CODEX_HANDOFF_切片软件开发上下文.md`
3. `docs/slicer/TASKS_00_P0切片Demo任务清单.md`
4. `docs/slicer/ROADMAP_后续PRD_DEV文档生成计划.md`

第一条 Codex 指令建议：

```text
请先阅读 AGENTS.md 和 docs/slicer 下的所有文档，不要修改代码。
请总结当前 P0 切片 Demo 的目标、输入、输出、固定协议、非目标范围、工程结构和第一批实现任务。
```

确认总结正确后，再让 Codex 按 Milestone 分批实现。
