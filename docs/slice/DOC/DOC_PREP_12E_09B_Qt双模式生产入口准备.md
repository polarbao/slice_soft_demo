# DOC_PREP 12E-09B Qt 双模式生产入口准备

> 状态：09B-02 COMPLETE / 09B-03 READY
> 日期：2026-07-24

## 1. 准备结论

12E-09B 的产品范围、架构边界、能力锁定、原子任务、验证矩阵和回滚条件已补齐。
`12E-09B-01/02` 已完成，下一原子任务为 `12E-09B-03`。不得跳过原子任务或将 Global 设为默认。

## 2. 当前代码复用点

```text
src/slicer_core/config/SlicePipelineConfig.*；
src/slicer_core/pipeline/SlicePipelineRouter.*；
src/slicer_core/output/rgbwsv/RgbwsvPackageWriter.*；
apps/slicer_debug_ui/services/ConfigDocument.*；
apps/slicer_debug_ui/services/EffectiveConfigGenerator.*；
apps/slicer_debug_ui/services/SlicePreflightCoordinator.*；
apps/slicer_debug_ui/services/ProcessRunner.*；
apps/slicer_debug_ui/widgets/QuickConfigPanel.*；
apps/slicer_debug_ui/widgets/ConfigEditorPanel.*；
apps/slicer_debug_ui/widgets/PreviewWorkspace.*；
apps/slicer_debug_ui/MainWindow.*。
```

当前代码已具备模式 Router、生产 writer、Effective Config 基础服务、一键切片和预检协调器；09B 只做
产品入口与能力锁定，不重写切片算法。

## 3. 原子任务

```text
09B-01：ProductionModeCatalog、UI DTO 与能力矩阵；
09B-02：Production Effective Config 与 capability lock；
09B-03：中文模式/Profile 选择器、状态和帮助；
09B-04：一键切片路由、准入、session identity 与 no-fallback；
09B-05：生产结果、preview/report、实际耗时与资源提示；
09B-06：self-test、UI smoke、真实模型、TIFF/RIP、文档与状态收口。
```

每个原子任务独立验证；是否提交遵循用户当前指令。

## 4. 依赖图

```text
08D-01..06 COMPLETE
  -> 09B-01
  -> 09B-02
  -> 09B-03
  -> 09B-04
  -> 09B-05
  -> 09B-06
  -> 12E-10

09A-02..04 可独立并行；
09A-05 -> 12E-10A；
09A-06 与 09B-06 的 smoke 结果分别记录，不互相冒充。
```

## 5. 准备 Gate

| 项目 | 状态 |
|---|---|
| 08D 双模式核心与 writer | COMPLETE |
| Global restricted candidate | GO / explicit opt-in |
| Global material parity candidate | GO / explicit opt-in |
| Legacy 默认 | FROZEN |
| no-fallback | FROZEN |
| 09B PRD/DEV/DEMO | COMPLETE |
| 09B TASKS/PROMPT | COMPLETE |
| 09B-01 开发 | COMPLETE |
| 09B-02 开发 | COMPLETE |
| 09B-03 开发 | READY |
| 09B-04..06 | PREPARED / sequential |

09B-02 已冻结 session Effective Config 审计对象、能力锁定、stale override 清理、fixture 只读和
原子写入。09B-03 可直接复用 `ProductionModeCatalog` 与
`ProductionEffectiveConfigSelection`，不需要再补新的核心配置 schema。

## 6. 后续原子任务准备审计

| 原子任务 | 输入与合同 | 验证方案 | 准备状态 |
|---|---|---|---|
| 09B-03 | Catalog、UI DTO、Effective Config selection、中文文案和能力矩阵 | self-test、最长中文、三窗口尺寸 | READY |
| 09B-04 | 现有 preflight/coordinator/ProcessRunner、session identity、no-fallback | Legacy/Global/blocked/invalid/retry 进程级测试 | PREPARED / WAIT 09B-03 |
| 09B-05 | 当前 session package、manifest mode、preview/report/timing | package identity、manifest、preview/report 同源 | PREPARED / WAIT 09B-04 |
| 09B-06 | Debug/Release、真实模型、RIP strict、Quick CI、状态文档 | 09B DEMO 完整矩阵 | PREPARED / WAIT 09B-05 |

上述任务的 PRD、DEV、DEMO、TASKS 和输出报告路径已确定。后续阻断是顺序依赖，不是文档缺失。

## 7. 风险守门

```text
复杂浮雕 strict blocker 不放宽；
Global 高时间/内存成本必须披露；
不把 OpenVDB backend 暴露为第三模式；
不覆盖 fixture；
不修改协议；
不允许 UI 值与 Effective Config/manifest 分叉；
不允许失败后加载旧 package。
```

## 8. 后续准备度

12E-10 的旧 R4/08D blocker 已解除。当前剩余依赖：

```text
10A 等待 09A-05 和 09B-05；
10B 等待 09B-06 后执行最终双模式矩阵；
10C 可复用 08D Release 证据，最终汇总等待 09B-06；
10D 等待 10A/10B/10C。
```

09B-06 后插入 12E-09C X/Y DPI 专项，再进入 09A 同层 preview 和 12E-10 最终收口。
