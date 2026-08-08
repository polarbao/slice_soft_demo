# REPORT HOSTFLOW H-C-01 UI 文件三桶分类

> 状态：**IMPLEMENTATION COMPLETE / VALIDATION PASS**
> 日期：2026-08-08
> 清单 schema：`hostflow.migration_inventory.1`

## 1. Current State

当前 `apps/slicer_debug_ui` 包含 77 个头文件，不再是任务草案中的 68 个。H-C-01 已按当前
HEAD 全量归档，并把同名 `.cpp` 纳入同一迁移单元的依赖审查。

| 桶 | 含义 | 数量 | 结论 |
|---|---|---:|---|
| A | 可直接复制，Qt/数据依赖闭包干净 | 6 | 允许复用，但仍由打印侧决定 UI 风格 |
| B | 产品能力需要，必须改走 ABI/Profile/Worker/package | 41 | H-C-02 的主要输入 |
| C | 调试、原始 JSON、CLI 或已被宿主替代 | 30 | 不进入打印宿主 |

机器真源为
`docs/slice/REPORT/assets/hostflow_hc01_migration_inventory.json`；本报告不重复粘贴 77 行，
避免人工表和机器表双写漂移。

## 2. A 桶直接复用清单

- `models/SceneSelectionModel.h`
- `services/HelpTextProvider.h`
- `services/PreviewPhysicalScale.h`
- `widgets/LogPanel.h`
- `widgets/SceneActionBar.h`
- `widgets/SettingHelpPanel.h`

上述单元不直接 include `slicer_core`，其本地头文件依赖也闭合在 A 桶。它们只是“源码可直接
复制”，不代表打印侧必须采用原视觉样式。

## 3. B 桶改造方向

| 理由码 | 改造出口 |
|---|---|
| `abi_scene` | `model.*`、`scene.*`、ViewData 与宿主本地选择/Transient 状态 |
| `abi_profile` | 宿主 Profile 目录、有效 Profile 和能力求交；禁止读取内部 scenario/config |
| `abi_job` | Worker submit/poll/cancel/release；禁止启动 CLI 解析 stdout |
| `abi_package` | `package.verify/summary/layers/layer_preview/named_reports` |

无 core include 的文件也可能属于 B：例如纹理参数模型虽然是纯 Qt/JSON，但若直接复制会继续
写内部配置，而不是宿主 Profile，因此必须改造而不能放进 A。

## 4. C 桶排除边界

C 桶包括 OpenVDB/材料闭环诊断、诊断 PNG 预览、UI smoke 内部实现、原始配置编辑器、CLI
进度解析、项目工具 Dock，以及已被参考宿主 Profile/Workspace/Job/Result 实现替代的旧组件。
排除这些文件不会删除切片侧能力；`apps/slicer_debug_ui` 仍保留作为 A/B 参考基线。

## 5. 验证

已运行：

```powershell
python scripts/ValidateHostflowMigrationInventory.py
git diff --check
```

结果为 `HOSTFLOW_HC01_PASS total=77 A=6 B=41 C=30`，两项均 PASS。

分类清单不修改主干 UI、核心、模块、ABI 或生产协议。H-C-02 应读取机器清单生成 B 桶逐文件
改造指南，不得重新从任务草案的 68 个历史数字开始。
