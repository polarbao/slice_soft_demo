# CODEX_PROMPT_01_00C后续路线执行指令

> 文档版本：v0.1  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

## 指令

请先阅读：

```text
docs/slicer/REPORT_00_P0_Demo当前实现状态.md
docs/slicer/DOC_DECISION_01_00C完成后的阶段路线调整.md
docs/slicer/ROADMAP_v0.2_00C后续PRD_DEV文档生成计划.md
docs/slicer/PRD_01_2_5D浮雕正式切片路线.md
docs/slicer/DEV_01_relief_heightfield正式切片设计.md
docs/slicer/DEMO_01_2_5D浮雕切片验证方案.md
docs/slicer/TASKS_01_2_5D浮雕正式路线任务清单.md
```

当前任务不是彩色纹理。

当前任务是：

```text
将 00C relief_heightfield 从 Demo 能力整理为正式 2.5D / Relief 路线。
```

必须保持：

```text
RGBWSV
uint8
0 = 打印
255 = 不打印
R G B W S V
V 光油
S 支撑
```

优先实现：

```text
1. 建立 samples/models/relief 和 samples/configs/relief；
2. 整理 relief 样例配置；
3. 增强 relief_report；
4. 标准化 printPixels 统计；
5. 增加 Relief 回归样例；
6. 新增或更新 REPORT_01_Relief当前实现状态.md。
```

不要实现：

```text
彩色纹理
UV 采样
MTL 真实材质映射
OpenVDB
Qt UI
完整光油覆盖策略
```
