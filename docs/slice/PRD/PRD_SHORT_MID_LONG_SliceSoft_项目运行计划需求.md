# PRD_SHORT_MID_LONG_SliceSoft_项目运行计划需求

> 文档版本：v0.1
> 文档状态：Formal PRD / Operating Plan
> 生成日期：2026-07-01

---

## 1. 目标

建立 SliceSoft 的短期、中期、长期项目运行计划，让后续开发不再只依赖临时对话判断。

计划必须支持：

```text
1. 明确当前阶段优先级；
2. 让 09P-R2 / 10 / 11 的文档和任务能排队执行；
3. 避免把 RIP / 设备工作误并入本项目；
4. 明确每个阶段的进入条件、退出条件、交付物和风险；
5. 为后续按月修订计划提供文档入口。
```

---

## 2. 用户角色

| 角色 | 需要 |
|---|---|
| 项目负责人 | 知道当前做什么、下个月做什么、哪些暂缓 |
| 开发工程师 | 能从计划映射到 TASKS 和验证命令 |
| UI/调试人员 | 知道 11 阶段何时进入以及要做哪些 UI 能力 |
| 算法工程师 | 知道 OpenVDB、mesh repair、texture fidelity 的优先级 |
| 下游 RIP 工程师 | 知道本项目何时交付 output contract，而不是 RIP 实现 |

---

## 3. 计划范围

短期：

```text
文档真源收束；
09P-R2 hardening；
mesh repair 是否插入的判断；
OpenVDB OFF / ON CI；
REPORT_09P_R2。
```

中期：

```text
09P-R3 / 09P-R4；
10 output contract；
11 UI layer preview / interactive config；
多模型能力决策。
```

长期：

```text
批处理；
多模型 build volume；
材料 profile versioning；
真实模型 release gate；
性能、大模型、安装和维护文档。
```

---

## 4. 非目标

```text
不承诺具体日历排期；
不替代每个阶段的 PRD / DEV / TASKS；
不实现 RIP 半色调；
不实现设备通信；
不默认启用 OpenVDB；
不把多模型 production 切片提前承诺。
```

---

## 5. 验收标准

本计划完成后应满足：

```text
1. ROADMAP 有短期 / 中期 / 长期表格；
2. PRD 明确用户和目标；
3. DEV 明确执行机制和目录关系；
4. README / DOC_INDEX 能找到计划文档；
5. 后续每个阶段能从计划跳转到 TASKS。
```

