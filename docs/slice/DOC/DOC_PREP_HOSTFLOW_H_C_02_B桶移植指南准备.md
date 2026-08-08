# DOC_PREP_HOSTFLOW H-C-02 B 桶移植指南准备

> 状态：**IMPLEMENTATION COMPLETE / VALIDATION PASS**
> 日期：2026-08-08
> 任务：`H-C-02`
> 前置：`H-C-01 COMPLETE`

## 1. 输入与目标

H-C-01 已冻结当前 77 个主干 UI 头文件，其中 B 桶 41 个。H-C-02 必须逐项说明打印宿主应
“替换、适配还是淘汰旧调用”，不得把 B 桶理解为 41 个文件原样复制。

## 2. 迁移工作流

41 个单元按公开边界归入四条工作流：

- `scene_authority`：模型、场景、预检、变换和排版；
- `host_profile`：宿主 Profile、参数、材质和配置校验；
- `worker_job`：提交、进度、取消和终态；
- `package_result`：包校验、逐层预览、报告和通道图。

每项记录 `replacement`、`action` 和相对复杂度。复杂度只用于同一工作流内排序，不可把 41 项
逐项人日相加；共享 DTO/Controller 会同时替代多个旧单元。

## 3. 冻结边界

- 只使用 SPI v1、11 导出、15 项能力和冻结 DTO；
- 禁止打印宿主 include/link `slicer_core`；
- 禁止读取 `slicer_scenarios.json`、内部 scene JSON 或直接遍历生产包推断协议；
- 正常 Commit 采用 operation 响应，只有 Stale/恢复才 snapshot；
- 相机、鼠标移动、视图切换保持零 DLL 调用。

## 4. 产物与验收

- `hostflow_hc02_migration_plan.json`：41 项机器计划；
- `ValidateHostflowMigrationPlan.py`：与 H-C-01 B 桶做双向集合校验；
- H-C-02 人工指南：工作包顺序、风险、估算和打印侧回签项。

验收要求：41/41 有 action、replacement、workstream、effort；无 A/C 项混入；无新增接口建议。

## 5. 准备结论

H-C-01 输入稳定，工作流、估算口径、停止条件和机器门禁明确。实现后 41/41 B 桶单元已闭合，
机器门禁确认 `scene_authority=15 / host_profile=14 / worker_job=2 / package_result=10`。
