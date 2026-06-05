# CODEX_PROMPT_02_03_v0.2_支撑与协议固化执行指令

> 文档版本：v0.2  
> 用途：复制给 VS Code Codex  
> 建议提交目录：`docs/slicer/`

---

## 指令

请先阅读：

```text
docs/slicer/REPORT_01_Relief当前实现状态.md
docs/slicer/DOC_REVIEW_02_03_业务逻辑审查与修订结论.md
docs/slicer/ROADMAP_v0.4_REPORT01后续执行计划_支撑与协议强化版.md
docs/slicer/PRD_02_支撑生成孤岛检测与SupportType扩展_v0.2.md
docs/slicer/DEV_02_支撑孤岛检测与SupportType设计_v0.2.md
docs/slicer/DEMO_02_支撑与孤岛检测验证方案_v0.2.md
docs/slicer/TASKS_02_支撑孤岛检测任务清单_v0.2.md
```

当前优先任务是 PRD_02：

```text
支撑生成、孤岛检测与 SupportType 扩展
```

请不要执行彩色纹理、OpenVDB、Qt UI。

必须保持：

```text
RGBWSV
uint8
0 = 打印
255 = 不打印
black_is_print
R G B W S V
Model > Support > Empty
SupportType 不进入 TIFF 通道
```

优先实现：

```text
1. support.mode = unsupported_only
2. support.mode = bottom_projection_plus_unsupported
3. connected component island detection
4. layer-to-layer overlap 判断
5. project_to_build_plate unsupported support
6. SupportType metadata / supportTypeStats
7. support_report / slice_report 增强
8. support 样例配置与回归
9. REPORT_02_支撑与孤岛检测当前实现状态.md
```

PRD_03 v0.3 可以先阅读，但本轮先不要大改协议实现。
