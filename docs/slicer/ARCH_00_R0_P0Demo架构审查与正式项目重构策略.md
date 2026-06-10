# ARCH_00_R0_P0Demo架构审查与正式项目重构策略

> 文档版本：v0.1  
> 文档状态：Architecture Draft  
> 适用阶段：R0  
> 建议提交目录：`docs/slicer/`

---

## 1. R0 目标

R0 的目标是把 P0 Demo 从功能探索项目，转换为正式项目架构蓝图。

R0 不直接大规模改代码，而是完成：

```text
1. 当前代码资产盘点；
2. Demo 临时代码识别；
3. 可保留模块识别；
4. 正式模块边界设计；
5. pipeline 分层设计；
6. config/report/test schema 设计；
7. R1/R2 重构任务拆分。
```

---

## 2. 当前 Demo 资产

当前已具备：

```text
OBJ/MTL/Texture
3MF basematerial / ColorGroup / Texture2DGroup
3MF deflate / validation / bad package
MaterialRoleMapping
MaterialPolicy
MaterialProcessProfile
Support
RGBWSV TIFF writer
RIP reader
Reports
Regression
Qt Debug UI
UI smoke test
```

这些能力应作为正式项目的功能资产保留，但代码结构需要重新分层。

---

## 3. 当前风险

```text
model.cpp 承载过多 importer/3MF/material 逻辑；
slicer.cpp 承载过多 pipeline/material/support/report 逻辑；
config schema 无统一版本；
reports 多但缺少统一 schema 风格；
测试主要集中在脚本级；
UI 与配置字段强耦合；
策略概念尚未完全对象化。
```

---

## 4. R0 重构原则

```text
先定接口，再移动代码；
先冻结协议，再扩功能；
先保持回归，再拆模块；
先分层 reports，再优化性能；
策略对象优先于 if/else 分支。
```

---

## 5. R0 主要输出

```text
ARCH_01_正式项目模块边界与目录结构设计.md
ARCH_02_pipeline执行链路与策略插入点设计.md
ARCH_03_config_schema与迁移策略设计.md
ARCH_04_reports_diagnostics_logging统一设计.md
ARCH_05_tests_regression_ci分层设计.md
PRE_R0_DECISION_纹理壳层与光油几何策略约束.md
ROADMAP_R0_R1_R2_正式项目重构路线.md
TASKS_R0_架构审查与重构设计任务清单.md
CODEX_PROMPT_R0_正式项目架构审查与重构设计指令.md
```

---

## 6. R0 成功标准

```text
1. 明确哪些模块保留；
2. 明确哪些模块重构；
3. 明确正式目录结构；
4. 明确 pipeline step；
5. 明确 config/report schema；
6. 明确 R1/R2 任务边界；
7. 不破坏当前 quick regression。
```
