# DOC DECISION HOSTFLOW H-B-04-R1 Profile 发现协议缺口

> 状态：**ACCEPTED / USER AUTHORIZED**
> 日期：2026-08-08
> 决策编号：HQ-08
> 影响任务：H-B-04、H-B-05、H-B-06、H-B-07、H-B-08

## Context

H-B-04 要求参考宿主经公开 ABI 获得 Profile 与模块能力，并显示能力标签和生产安全等级。
现有 `pm_module_info` 没有 Profile 目录，公开 15 项能力也没有 Profile 查询能力；同时参考宿主
不得读取仓库内部 `slicer_scenarios.json`，也不得依赖 `slicer_core`。

## Decision Needed

必须在以下两种兼容路径中选择一条：

### HQ-08-A：Profile 目录归宿主（推荐）

- PrintApp/宿主提供 Profile 描述和生产安全等级；
- 切片模块通过 `pm_module_info` 自述能力；
- 宿主对 Profile required capabilities 与模块能力求交，得到“可用 Profile”；
- 不修改冻结 SPI、15 项能力、module_info schema 或 RGBWSV/TIFF。

需要修订 H-B-04 的文字语义，但不修改公开二进制合同。

### HQ-08-B：Profile 目录随模块自述

- 在 `pm_module_info` 增加 `profileCatalog`；
- 保持 SPI v1、11 导出、15 项能力不变；
- 必须受控修改 module_info schema、模块自述、合同测试与打印侧回签基线。

## Recommendation

采用 **HQ-08-A**。Profile 是打印应用的业务资产，模块只应声明能力并校验输入；该方案边界清晰、
可移植且不重新打开已冻结的 Stage 14 module_info 合同。只有当产品确认“Profile 必须随 DLL
发布”时才选择 HQ-08-B。

## Accepted Decision

2026-08-08，用户明确授权采用 **HQ-08-A**：

- Profile 目录与生产安全等级由 PrintApp/宿主拥有；
- 切片模块只通过现有 `pm_module_info` 结构化声明能力；
- 可用性由 `Profile.requiredCapabilities` 与模块 `provides` 求交得到；
- 不修改 `PM_SPI_VERSION=1`、11 个导出、15 项能力、module_info schema 或 RGBWSV/TIFF；
- H-B-04 原“Profile 经 ABI 查询”的语义受控修订为“宿主提供 Profile，模块能力经 ABI 查询”。

HQ-08-B、增加第 16 项能力以及读取仓库内部场景 JSON 均不采用。

## Consequences

- HQ-08-A 已关闭，H-B-04 可在参考宿主内独立实现并继续 H-B-05..08。
- HQ-08-B 需要先建立与 14F-R1/R2/R3 同级的受控合同修订，再回到 H-B-04。
- 宿主参考 Profile 是宿主自有 fixture，不得被解释为模块内置 Profile 或生产设备目录。

## Revision History

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-08 | R1 accepted | 用户授权 HQ-08-A；冻结宿主目录与 ABI 模块能力求交方案，关闭 H-B-04 实现阻断。 |
| 2026-08-07 | R1 proposed | 提出 HQ-08-A/HQ-08-B 两条兼容路径并推荐 HQ-08-A。 |
