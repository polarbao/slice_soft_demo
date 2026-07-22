# TASKS_12E-08C-R4 模型导入预检与修复资产准入任务清单

> 文档状态：PREPARED
> 日期：2026-07-21
> 当前原子任务：R4-05 COMPLETE / R4-06 DEVELOPMENT AUTHORIZED / REAL FAMILY MATRIX BLOCKED
> 规则：每次只执行用户明确指定的一个原子任务；完成验证后再提交

## 1. 固定边界

```text
legacy 继续是默认生产模式；
global_surface_shell 继续 diagnostic-only，直到 08D GO；
不放宽 strict，不自动 fallback；
不实现通用复杂自相交重建；
不引入第三方库；
不修改 p0.rgbwsv.2/RGBWSV/uint8/black_is_print；
不提交或覆盖 model/obj 下用户未跟踪资产，除非用户明确要求。
```

## 2. R4-01 Preflight Contract

状态：COMPLETE。

范围：ModelPreflight DTO、stable code、mode admission DTO、cache key、report schema 和 generated unit/golden。

禁止：不接 UI，不启动切片，不修复模型，不写 package。

验证：新增定向 unit/schema/golden + `git diff --check`。

结果：新增 ModelPreflight DTO、8 个稳定错误码、双模式 admission DTO、确定性 cache key、
`slicesoft.model_preflight.12e_08c_r4.1` report/golden；定向测试与 Debug 全量构建通过。Quick CI 停在既有
`material_process_top2 widthPx expected=48 actual=226` baseline，与本合同任务无关。

## 3. R4-02 Two-stage Preflight Service

状态：COMPLETE。

范围：fast import check、最终 transform 后 full diagnostics、cache/stale、取消和 deterministic result。

完成标准：模型/resource/transform/options 任一变化都使 cache 失效；完整审计不足不返回 PASS。

结果：已实现两阶段同步服务、source/resource/transform/options identity、进程内 cache、stale/cancel、
完整审计 fail-closed 和真实 clean OBJ/3MF 正向验证。证据见
`docs/slice/DOC/DOC_EXEC_12E_08C_R4_02_TwoStagePreflightService结果.md`。

## 4. R4-03 Mode Admission and Pipeline Gate

状态：COMPLETE。

范围：legacy/global admission policy、CLI facade、两个 pipeline 入口的 fail-closed/no-fallback 守门。

完成标准：同一 self-intersection 模型得到 legacy warning/global blocked；global 核心和 writer 均未启动。

结果：已实现独立 admission policy、backend-neutral gate、legacy/global facade 守门和 CLI 接入；blocked 输入
不会启动所选 pipeline 或创建 package，且不发生 global -> legacy 回退。定向 4/4 CTest 与 Debug 构建通过；
Quick CI 仍停在既有 `material_process_top2 widthPx expected=48 actual=226` baseline。详见
`docs/slice/DOC/DOC_EXEC_12E_08C_R4_03_ModeAdmission与PipelineGate结果.md`。

## 5. R4-04 Qt Preflight UI

状态：COMPLETE。

范围：中文状态/问题列表、重新检测、stale、两个一键按钮、异步生命周期和 UI smoke。

完成标准：任何 UI 路径不能绕过 preflight；最长中文不遮挡；关闭/取消无崩溃。

准备结果：已冻结 Qt 线程池执行、generation/cancel/QPointer 生命周期、外部 OpenVDB capability probe、三条
切片入口统一 coordinator、中文 presenter/panel、stale/cache 规则和四组 UI Smoke。详见
`docs/slice/DOC/DOC_PREP_12E_08C_R4_04_QtPreflightUI准备.md`。

结果：已实现异步 controller、中文 presenter/panel、单 pending action coordinator、外部 CLI capability
schema 和四个入口接线；定向 CTest、Qt self-test、三组 preflight smoke、布局 smoke 与真实 OpenVDB ON
capability probe 通过。详见
`docs/slice/DOC/DOC_EXEC_12E_08C_R4_04_QtPreflightUI结果.md`。

## 6. R4-05 Clean Positive Matrix

状态：COMPLETE。

范围：闭合彩色 OBJ + Texture2D 3MF 的 0.10mm/intermediate/allTexture，Model Fill 材料解析和 report。

输入已冻结为：

```text
主 OBJ：model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj；
复核 OBJ：model/obj/yecan/3.obj；
3MF：samples/models/3mf/texture2d_checker_cube.3mf。
```

`model` 目录其他 6 个 strict PASS OBJ 可作扩展覆盖；目录内三个 3MF 均存在自相交/非流形问题，不得作
正向 fixture。模型资产预检证据见 `docs/slice/REPORT/REPORT_12E_08C_R4_模型资产预检清单.md`。

完成标准：互补/单调/终点不变量全部 PASS；正常模型结果不写入 required repair PASS 计数。

原子级准备已补齐：必跑/扩展模型边界、三点 width 计算、Model Fill 解析 DTO、汇总
schema、代码落点、验证命令和停止条件见
`docs/slice/DOC/DOC_PREP_12E_08C_R4_05_CleanPositiveMatrix准备.md`。

结果：三个必跑输入 preflight、width 互补/单调/终点和材料通道矩阵全部 PASS；未写生产 package，
`requiredRepairPassCount=0`。详见
`docs/slice/DOC/DOC_EXEC_12E_08C_R4_05_CleanPositiveMatrix结果.md`。

## 7. R4-06 Repaired Asset Intake

状态：DEVELOPMENT AUTHORIZED / REAL FAMILY MATRIX BLOCKED。

范围：爱神、玫瑰、梯田三个 required family 候选的 identity/provenance/attribute/post-strict 审计。

完成标准：每例具有原/新 hash、修复来源、属性 diff、完整自相交和 post-strict 证据。

原子级准备已补齐：required 身份、intake manifest、属性/姿态/完整自相交准入、stable blocker、
generated 合同测试和 R4-07/08 依赖见
`docs/slice/DOC/DOC_PREP_12E_08C_R4_06_RepairedAssetIntake准备.md`。正常 clean 模型只作控制组，
不得计入 required family PASS。当前三个 family 均为 0 PASS，因此服务实现可以开始，但 R4-07 不可启动。

## 8. R4-07 Four-case Release Gate

状态：PREPARED / WAIT R4-06。

范围：四 case global partition/texture/raster/full closure，Release time/memory，legacy TIFF/RIP/Quick CI。

完成标准：四 case 全部 admitted；预算冻结；production output 仍按 08D 边界处理。

## 9. R4-08 08D GO/NO-GO Refresh

状态：PREPARED / WAIT R4-07。

范围：更新 matrix/report/context；只作决策，不实现 adapter。

完成标准：全部 Gate PASS 才 GO；否则保留 NO-GO 和 blocker。

## 10. 提交要求

每个原子任务按项目中文详细模板独立提交，提交前必须运行定向验证、`git diff --check`、
`git status --short`，不得把无关模型资产混入提交。
