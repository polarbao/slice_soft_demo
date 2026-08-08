# DOC_PREP_HOSTFLOW H-C-01 UI 文件三桶分类准备

> 状态：**IMPLEMENTATION COMPLETE / VALIDATION PASS**
> 日期：2026-08-08
> 任务：`H-C-01`
> 前置：`H-B-07 COMPLETE`；当前 `H-B-01..08 COMPLETE`

## 1. 范围校正

任务草案记录的是 68 个头文件。以当前 HEAD 为准，`apps/slicer_debug_ui/**/*.h` 实际共有
77 个；新增差额来自 Stage 13/15 和 14E 后续拆分。H-C-01 不冻结历史数字，必须覆盖当前 77 个
头文件，否则移植清单在生成时已经过期。

每个头文件连同同名 `.cpp` 作为一个迁移单元，按以下三桶唯一归类：

- A：可直接复用，不能直接或经本地头文件依赖 `slicer_core`；
- B：产品流程仍需要，但必须改为宿主 DTO/Facade/Worker/package 能力；
- C：切片调试、旧 CLI/原始 JSON 配置或已被参考宿主替代，不进入打印宿主。

## 2. 决策边界

“没有直接 include core”只是 A 桶必要条件，不是充分条件。仍读取内部配置、生产包目录、CLI
进度文本或诊断报告的文件不得因为 include 干净而误入 A；它们应进入 B 或 C。

H-C-01 只交付分类与机器门禁，不复制文件、不修改主干 UI，也不改变冻结 ABI。桶 B 的逐文件
改造方案属于 H-C-02；同模型/Profile 的行为差异属于 H-C-03。

## 3. 证据与产物

- 当前源码树的 77 个头文件及同名实现；
- 机器清单 `hostflow_hc01_migration_inventory.json`；
- `ValidateHostflowMigrationInventory.py`：检查全覆盖、唯一归类、路径存在和 A 桶依赖闭包；
- 人工报告：汇总数量、理由码和迁移边界。

## 4. 验收

1. 当前 77 个头文件无遗漏、无重复、无未知路径；
2. A 桶头/实现不直接 include `slicer_core`，且本地头依赖仍在 A 桶；
3. 每项都有稳定理由码；
4. `apps/slicer_debug_ui/**` 零修改；
5. `git diff --check` 和分类脚本 PASS。

## 5. 准备结论

范围、分类定义、机器门禁与后续任务边界均已明确。实现后机器门禁确认当前 77 个头文件
`A=6 / B=41 / C=30`，全覆盖、唯一归类和 A 桶依赖闭包均 PASS。
