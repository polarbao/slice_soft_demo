# DEV_SHORT_MID_LONG_SliceSoft_项目运行计划执行方案

> 文档版本：v0.1
> 文档状态：Formal DEV / Operating Plan
> 生成日期：2026-07-01

---

## 1. 技术目标

将短期、中期、长期计划转换为可执行的文档和任务机制。

核心规则：

```text
ROADMAP 负责阶段排布；
PRD 负责说明为什么排；
DEV 负责说明如何执行和维护；
TASKS 负责拆分具体任务；
REPORT 负责阶段结束状态。
```

---

## 2. 文档关系

```text
docs/slice/ROADMAP/ROADMAP_SHORT_MID_LONG_SliceSoft_项目运行计划.md
  -> docs/slice/PRD/PRD_SHORT_MID_LONG_SliceSoft_项目运行计划需求.md
  -> docs/slice/DEV/DEV_SHORT_MID_LONG_SliceSoft_项目运行计划执行方案.md
  -> docs/codex_task/current/TASKS_<stage>.md
  -> docs/slice/REPORT/REPORT_<stage>.md
```

当前 `docs/slice` 已按 `PRD` / `DEV` / `DOC` / `DEMO` / `ROADMAP` / `REPORT` 分目录保存，后续新增文档应继续放入对应子目录。

---

## 3. 执行机制

每个阶段开始前必须满足：

```text
PRD 存在；
DEV 存在；
DEMO 存在；
TASKS 存在；
CODEX_PROMPT 存在；
阶段红线清楚；
验证命令清楚。
```

每个阶段结束时必须生成：

```text
REPORT；
已运行验证；
未运行验证及原因；
是否进入下一阶段；
是否需要插入专项阶段。
```

---

## 4. 计划维护节奏

建议：

```text
每完成一个阶段，更新 ROADMAP_SHORT_MID_LONG；
每月做一次计划复核；
每次引入新阶段，补齐 PRD / DEV / DEMO / TASKS / CODEX_PROMPT；
每次阶段范围变化，补 DOC_DECISION。
```

---

## 5. 风险控制

| 风险 | 控制方式 |
|---|---|
| 计划过细导致频繁失效 | 阶段任务细，日期只做建议范围 |
| 任务越界到 RIP/设备 | DOC_DECISION_10 固定边界 |
| 09P 未 hardening 就做 UI 产品化 | 11 阶段进入条件依赖 09P/10 |
| 多模型过早 production | 11 只做能力决策 |
| 文档目录变动导致引用失效 | README / DOC_INDEX 统一维护入口 |
