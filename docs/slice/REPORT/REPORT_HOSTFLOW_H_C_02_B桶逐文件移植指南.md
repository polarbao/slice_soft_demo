# REPORT HOSTFLOW H-C-02 B 桶逐文件移植指南

> 状态：**IMPLEMENTATION COMPLETE / VALIDATION PASS**
> 日期：2026-08-08
> 计划 schema：`hostflow.migration_plan.1`

## 1. Current State

H-C-01 的 41 个 B 桶单元已经逐项映射到参考宿主或冻结 DTO。机器真源为
`docs/slice/REPORT/assets/hostflow_hc02_migration_plan.json`，每项包含工作流、迁移动作、替代实现
和相对复杂度；禁止把本报告当作复制 41 个旧文件的指令。

| 工作包 | 文件数 | 打印侧目标 | 建议人日 |
|---|---:|---|---:|
| `scene_authority` | 15 | 导入、场景权威、预检、ViewData、变换排版 | 12-18 |
| `host_profile` | 14 | Profile 目录、参数、材料与有效 Profile | 10-15 |
| `worker_job` | 2 | submit/poll/cancel/release | 3-5 |
| `package_result` | 10 | verify、摘要、层预览、报告与通道图 | 7-11 |
| 集成与回归 | - | 三车道、错误恢复、A/B、部署 | 6-10 |

总量建议按 **38-59 人日** 排期，而不是把 41 个文件的 S/M/L 逐项相加。参考宿主已经提供
可运行样板，打印侧应以工作包整合复用。

## 2. 迁移顺序

1. 先建立 `ModuleClient`、Profile catalog 和 session-owned scene context；
2. 完成 `scene_authority`，确保 Commit/revision/Stale 与 ViewData 成立；
3. 完成 `host_profile`，首次建场景前冻结 Profile/buildVolume；
4. 接入 `worker_job`，保持 UI 非阻塞和句柄单次释放；
5. 最后接入 `package_result`，只通过五项 package 能力读取结果。

`scene_authority` 与 `host_profile` 的 UI 草稿可并行，但首次 Commit 前必须汇合；`worker_job`
依赖有效场景/Profile；`package_result` 依赖成功包。

## 3. Action 解释

- `replace_with_reference_host`：旧实现的所有权或调用链错误，直接以 H-B 对应类为样板；
- `adapt_to_public_dto`：可保留展示层，但输入改为公开 DTO，禁止 core 类型穿透；
- `adapt_to_host_profile`：保留业务参数概念，写入宿主 Profile，不编辑内部 repo 配置。

## 4. 打印侧回签项

- QSettings 组织名与现有 PrintApp 设置迁移策略；
- buildVolume、设备 Profile 和输出目录的最终所有权；
- Worker 作业如何接入 PrintApp 现有队列，但不改变切片模块单作业合同；
- package named reports 哪些进入日常 UI，哪些只在高级诊断显示；
- A 桶视觉组件是否直接采用或按打印软件设计系统重写。

这些项目为打印侧集成选择，不阻断切片侧 H-C-02 交付，也不能写成打印侧 PASS。

## 5. 验证

```powershell
python scripts/ValidateHostflowMigrationInventory.py
python scripts/ValidateHostflowMigrationPlan.py
git diff --check
```

实际结果为 `HOSTFLOW_HC01_PASS total=77 A=6 B=41 C=30` 和
`HOSTFLOW_HC02_PASS total=41 host_profile=14 package_result=10 scene_authority=15 worker_job=2`；
三项检查均 PASS。

H-C-02 不修改 `apps/slicer_debug_ui`、参考宿主代码、ABI、Worker、RGBWSV 或 TIFF。
