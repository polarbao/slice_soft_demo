# DOC DECISION HOSTFLOW H-B-04-R1 Profile 发现协议缺口

> 状态：**PROPOSED / REQUIRES USER DECISION**
> 日期：2026-08-07
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

## Consequences

- HQ-08-A 关闭后，H-B-04 可在参考宿主内独立实现并继续 H-B-05..08。
- HQ-08-B 需要先建立与 14F-R1/R2/R3 同级的受控合同修订，再回到 H-B-04。
- 未决期间，H-B-04 为准备完成但实现阻断；不得以硬编码 Profile 或内部 JSON 伪造闭环。
