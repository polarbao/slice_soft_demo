# DOC_AUDIT_00_08_历史阶段文档缺口与补齐清单

> 文档版本：v0.1
> 文档状态：Document Audit / 00-08 Formal Supplement
> 生成日期：2026-07-01
> 证据等级：B/C 混合，当前正式汇总为 B，历史归档为 C

---

## 1. 结论

00-08 阶段并不是没有文档。旧阶段 PRD、DEV、DEMO、REPORT、TASKS 已经归档到：

```text
docs/archive/2026-06-30_slicer_legacy
docs/codex_task/archive
```

真正缺少的是当前正式文档体系中的汇总入口：

```text
00-08 功能基线汇总；
00-08 技术基线汇总；
00-08 验证与回归基线；
00-08 历史阶段任务入口；
短期 / 中期 / 长期项目运行计划。
```

因此本轮不把所有旧文档搬回 `docs/slice`，而是新增正式汇总文档，保留 archive 作为历史证据。

---

## 2. 阶段覆盖矩阵

| 阶段 | 历史归档覆盖 | 当前正式缺口 | 本轮补齐方式 |
|---|---|---|---|
| 00 / 00A / 00B / 00C | PRD / DEV / DEMO / REPORT / TASKS | 缺正式汇总入口 | 纳入 00-08 PRD / DEV / DEMO 汇总 |
| 01 | PRD / DEV / DEMO / REPORT / TASKS | 缺正式汇总入口 | 纳入 2.5D relief 基线 |
| 02 | PRD / DEV / DEMO / REPORT / TASKS | 缺正式汇总入口 | 纳入 support / island / SupportType 基线 |
| 03 / 03B / 03C | PRD / DEV / DEMO / REPORT / TASKS | 缺正式汇总入口 | 纳入 RGBWSV protocol / storage / regression 基线 |
| 04 / 04A | PRD / DEV / DEMO / REPORT / TASKS | 缺正式汇总入口 | 纳入 OBJ/MTL texture / fallback 基线 |
| 05 / 05A | PRD / DEV / DEMO / REPORT / TASKS | 缺正式汇总入口 | 纳入 MaterialPolicy / MaterialProcessProfile 基线 |
| 06 / 06A / 06B | PRD / DEV / DEMO / REPORT / TASKS | 缺正式汇总入口 | 纳入 3MF / Texture2D / ColorGroup 基线 |
| 07 / 07A / 07B | PRD / DEV / DEMO / REPORT / TASKS | 缺正式汇总入口 | 纳入 Qt Debug UI / config / smoke 基线 |
| R0 / R1 / R2 | ARCH / DEV / DEMO / REPORT / TASKS | 缺正式汇总入口 | 作为 07 后到 08 前的工程化基线 |
| 08 / 08A | PRD / DEV / DEMO / REPORT / TASKS | 缺正式汇总入口 | 纳入 support shape / bridge fixture / real profile 基线 |

---

## 3. 新增正式补齐文档

本轮补齐：

```text
docs/slice/PRD/PRD_00_08_Demo阶段功能基线汇总.md
docs/slice/DEV/DEV_00_08_Demo阶段技术基线汇总.md
docs/slice/DEMO/DEMO_00_08_Demo阶段验证与回归基线.md
docs/codex_task/current/TASKS_00_08_历史阶段文档补齐任务清单.md
docs/codex_task/current/CODEX_PROMPT_00_08_历史阶段文档补齐执行指令.md
docs/slice/ROADMAP/ROADMAP_SHORT_MID_LONG_SliceSoft_项目运行计划.md
```

---

## 4. 使用规则

```text
1. 00-08 汇总文档用于理解 demo 已形成的能力基线。
2. 判断是否已实现时，仍以当前代码、脚本、测试和最新 REPORT 为 A 级证据。
3. archive 中旧 PRD/DEV/DEMO/TASKS 不恢复为当前执行入口。
4. 后续开发任务优先从 09P-R2、10、11 的 current task 入口继续。
5. 00-08 只在发现回归、协议疑问、历史设计疑问时回查。
```
