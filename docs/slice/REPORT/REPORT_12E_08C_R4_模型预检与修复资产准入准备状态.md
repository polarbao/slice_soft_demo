# REPORT_12E-08C-R4 模型预检与修复资产准入准备状态

> 文档状态：R4-08 EXECUTION COMPLETE / DECISION BLOCKED / PRODUCTION NOT ADMITTED
> 日期：2026-07-22

## 1. 阶段结论

R4 作为 R3-04 NO-GO 与 12E-08D 之间的正式插入专项，准备文档已经完整。该专项允许正常闭合模型继续
推进 Texture Surface/Model Fill 功能，但不降低爱神、玫瑰、梯田三个 required 真实模型族的生产 Gate。

## 2. 已完成准备

```text
可达性和模型治理分析；
插入专项 Decision；
PRD/DEV/DEMO；
分阶段 Roadmap；
R4-01..08 原子任务清单；
Codex 执行提示；
启动依赖与停止条件；
主 PRD/DEV/DEMO/Matrix/Roadmap/Report/Index 同步；
AI context handoff。
```

## 3. 当前允许工作

```text
R4-01 Preflight Contract；
R4-02 Two-stage Preflight；
R4-03 Mode Admission/Pipeline Gate；
R4-04 Qt Preflight UI；
R4-05 Clean Positive Matrix。
R4-06 Required Family Candidate Intake 候选审计（软件实现已完成）。
R4-07 Development Gate（development intake 与四 case 已完成）；
R4-08 GO/NO-GO Refresh（已执行，决策为 BLOCKED）。
```

这些任务不要求先取得三个 family PASS 模型，但不得写 global production package。

## 4. 当前阻断工作

```text
R4-06 真实矩阵：等待三个 required family 各一个 admitted candidate；
R4-07 最终验收：等待 required family matrix 3/3；
R4-08 GO：被真实族 four-case、生产预算、Quick CI 和授权阻断；
12E-08D：等待后续 R4-08 重跑输出 GO 和用户明确授权。
```

## 5. 固定产品参数

```text
Texture Surface base minimum = 0.10mm；
UI/config step = 0.01mm；
effective minimum = max(0.10mm, 2 * classificationResolutionMm)；
maximum = dynamic allTextureThresholdMm；
C/M/Y/K = MaterialProcessProfile roles，非新增 TIFF channels；
legacy 默认；global fail-closed；无 silent fallback。
```

## 6. 下一任务

`12E-08C-R4-08` 已完成当前证据下的正式刷新，输出 `DECISION BLOCKED`。当前可执行开发任务是
`12E-09A-01` diagnostic facade；生产准入路线仍需等待三个 family 的修复/重建候选进入 intake，并关闭
Quick CI、生产预算和授权 blocker。12E-08D 不可启动。

## 7. 模型资产准备结果

`model` 目录已完成 22 个 OBJ/3MF 的统一 Release 预检。7 个 OBJ strict PASS 且第二次完整审计结果稳定，
已满足 R4-01..05 的真实 OBJ 输入准备；1 个 OBJ 需人工修复，另外 11 个 OBJ 和 3 个 3MF 需重建。新增圣诞
中指 OBJ 的完整审计确认 571 组自相交，因此不能直接进入严格链。

当前目录没有 strict PASS 3MF，R4 正向 Texture2D 3MF 仍使用既有
`samples/models/3mf/texture2d_checker_cube.3mf`。爱神 5 个、玫瑰 3 个、梯田 1 个候选均未 strict PASS，
不得据此解除 required-family 最终 Gate、R4-08 或 12E-08D；但可按开发 Gate 用于 R4-07 diagnostic。

完整清单见 `REPORT_12E_08C_R4_模型资产预检清单.md`。

## 8. R4-01 实施结果

ModelPreflight DTO、稳定错误码、双模式 admission、cache identity/key、report schema、unit 和 golden 已实现。
定向测试、相关合同测试、Debug 全量构建及 Qt self-test 通过。Quick CI 仍被既有
`material_process_top2 widthPx expected=48 actual=226` golden baseline 阻断。

详细证据见 `../DOC/DOC_EXEC_12E_08C_R4_01_ModelPreflightContract结果.md`。

## 9. R4-02 准备结果

已冻结 importer/最终 transform 边界、fast/full 执行链、结果合并优先级、source/resource 双 hash stale
检测、进程内 cache、阶段边界取消、正向/负向 fixture 和验证命令。明确 `load_model_report` 返回的三角形
已应用 transform/autoOrient，后续服务不得重复变换；完整审计不足不得 PASS。

详细准备见 `../DOC/DOC_PREP_12E_08C_R4_02_TwoStagePreflightService准备.md`。

## 10. R4-02 实施结果

两阶段预检服务、内容身份、cache/stale/cancel 和完整审计 fail-closed 已实现。generated fixture、真实
`xiao_ma` OBJ、`yecan/3.obj` 与 Texture2D checker 3MF 均通过定向测试；服务不接 UI/pipeline/writer。

Debug 全量构建和相关 CTest 通过。Quick CI 仍停在既有
`material_process_top2 widthPx expected=48 actual=226` golden。详细证据见
`../DOC/DOC_EXEC_12E_08C_R4_02_TwoStagePreflightService结果.md`。

## 11. R4-03 准备结果

已冻结 shared fatal、legacy warning/global blocked 的拓扑差异、backend unavailable、未知 error fail-closed、
稳定 code 排序和 `productionOutputWritten=false` 语义；同时明确 CLI/pipeline gate 必须在 global core、staging
目录和 writer 之前阻断，且任何 global 失败不得自动回退 legacy。

R4-03 达到 `READY FOR DEVELOPMENT`。详细准备见
`../DOC/DOC_PREP_12E_08C_R4_03_ModeAdmission与PipelineGate准备.md`。

## 12. R4-03 实施结果

独立模式准入策略、backend-neutral pipeline gate、legacy/global facade 和普通 CLI 入口已经接通。shared fatal
同时阻断两种模式，完整拓扑问题保持 legacy warning/global blocked；blocked 输入不启动核心或 writer，且
不存在 global -> legacy 自动回退。

定向 4/4 CTest 与 Debug 全量构建通过。Quick CI 的切片/RIP/3MF 正负向/schema/support 已通过，仍在既有
`material_process_top2 widthPx expected=48 actual=226` golden baseline 失败。详细结果见
`../DOC/DOC_EXEC_12E_08C_R4_03_ModeAdmission与PipelineGate结果.md`。

## 13. R4-04 准备结果

Qt controller/presenter/coordinator/panel 边界、QThreadPool/generation/cancel/QPointer 生命周期、外部 OpenVDB
capability probe、三条切片入口统一守门、中文状态机和 UI Smoke 已冻结。R4-04 达到
`READY FOR DEVELOPMENT`。

R4-05 已完成；R4-06 的合同与软件实现已完成，三个 required family 当前均无 admitted candidate；
R4-07 development 已完成，R4-07 final/R4-08 仍不可提前执行。详细准备见
`../DOC/DOC_PREP_12E_08C_R4_04_QtPreflightUI准备.md`。

## 14. R4-04 实施结果

Qt 工作台已接入异步 `ModelPreflightController`、中文 `ModelPreflightPresenter/Panel` 和单 pending action
`SlicePreflightCoordinator`。导入传统切片、运行切片、OpenVDB 候选切片均在子进程启动前执行 fresh
preflight；传统拓扑 warning 需要明确确认，global blocker 不提供继续或 legacy fallback。只读 OpenVDB 诊断
同样先展示共享事实，但不把诊断误写成 global admitted。

新增 `slicer_cli --openvdb-capability-json` 固定 schema。本机默认 OFF executable 返回 unavailable，OpenVDB ON
candidate 返回 runtime available、version 12.0.1；probe 不加载模型、不写输出。Controller 使用 generation、
取消 token、受保护回调 bridge 和 120 秒 capability 超时，关闭窗口不等待 worker。

定向 4/4 CTest、Qt self-test、preflight state/gate/lifecycle smoke、布局 smoke 和真实 capability probe 通过。
Quick CI 在 300 秒窗口内未完成，独立 golden 仍确认既有
`material_process_top2 widthPx expected=48 actual=226` baseline。

详细证据见 `../DOC/DOC_EXEC_12E_08C_R4_04_QtPreflightUI结果.md`。

## 15. 模型依据同步确认

后续检测依据已同步到 `.agents/docs/project-profile.md`、R4 任务清单、R4 准备文档和模型资产清单：
`xiao_ma_wu_yu_new` 5 个 OBJ、`yecan/3.obj`、`yecan/4.obj` 为无需重建的 7 个 strict PASS OBJ。主正向模型
固定为 `MF_Xiao_ma_Damuzhi_ty02.obj`，独立复核固定为 `yecan/3.obj`；`yecan/4.obj` 当前仍是未跟踪用户资产，
只读使用且不纳入提交。正向 3MF 继续使用 `samples/models/3mf/texture2d_checker_cube.3mf`。

这些正常模型已用于 R4-05，并通过 `development_model_pool` 解锁 R4-07 开发；它们不能替代 required
family 解除最终 Gate。R4-06 可继续接收真实候选，R4-07 final、R4-08 和 08D 仍由真实 family matrix 阻断。

## 16. R4-05 原子级准备结果

R4-05 已补齐独立准备文档，冻结三个必跑输入、本地扩展输入的版本控制边界、
minimum/intermediate/allTexture 量化规则、互补/单调/终点不变量、Model Fill 材料解析 DTO、
R4-05 汇总 schema、计划代码落点和定向验证命令。

特别确认：C/M/Y/K 仅作为 MaterialProcessProfile role id，未注册时返回稳定不可用原因，
不新增 TIFF 通道，不硬编码未标定墨量。R4-05 现为 `COMPLETE`。

详细准备见 `../DOC/DOC_PREP_12E_08C_R4_05_CleanPositiveMatrix准备.md`，实际结果见
`../DOC/DOC_EXEC_12E_08C_R4_05_CleanPositiveMatrix结果.md`。三个必跑输入全部 PASS，正常模型计数保持
`requiredRepairPassCount=0`。R4-06 合同现按 `aishen/meigui/titian` 三个 required family 修订；R4-07
development 已执行，R4-07 final/R4-08 只是依赖准备完成，不能提前放行。

## 17. R4-06 及后续准备结论

R4-06 已补齐独立准备文档，冻结三个 required family、candidate kind、provenance、内容 hash、单位/姿态/尺寸、
材质/UV/纹理属性差异、完整自相交、post-strict 与 repeatability 准入字段。已验证跨族 clean 模型只作 intake
控制组，不计入 `requiredFamilyPassCount`。

R4-06 当前为 `IMPLEMENTATION COMPLETE / DEVELOPMENT 2/2 / REAL FAMILY 0/3`；R4-07 为
`DEVELOPMENT COMPLETE / FINAL WAIT FAMILY 3/3`；R4-08 为 `EXECUTION COMPLETE / DECISION BLOCKED`。详细准备见
`../DOC/DOC_PREP_12E_08C_R4_06_RepairedAssetIntake准备.md`。

## 18. R4-06 实施结果

已实现 required family 和 `development_model_pool` candidate 的 DTO/service/CLI/report、原/新 hash、provenance、尺寸/姿态、材质/纹理/UV
差异、完整自相交、post-strict 和两次审计 repeatability。`ModelPreflightService` 复用同一次完整审计 evidence，
稳定 preflight JSON schema 未改变。Generated unit/golden、相关 preflight 回归和 Release 真实 family 基线脚本
通过；三个现有 required 代表均按预期 BLOCKED，xiao_ma/yecan development candidate `2/2 admitted`，未写
production output。

详细结果见 `../DOC/DOC_EXEC_12E_08C_R4_06_RepairedAssetIntake结果.md`。

## 19. R4-07/08 原子级准备结果

R4-07 development 已使用 xiao_ma minimum/allTexture、yecan intermediate 和 Texture2D 3MF 完成四 case，
fresh intake、global partition/texture/raster/full closure、Release 三次测量、legacy TIFF/RIP 与 no-production
边界全部通过。开发测量不冻结生产预算；required family 0/3 仍阻止最终真实族矩阵。

R4-08 已按实际证据执行：required family 0/3、最终真实族 four-case 缺失、生产预算未冻结、Quick CI 在
`material_process_top2 widthPx expected=48 actual=226` 失败，且没有 production path 授权，因此输出
`BLOCKED`，不能启动 08D。准备与结果详见：

```text
../DOC/DOC_PREP_12E_08C_R4_07_FourCaseReleaseGate准备.md
../DOC/DOC_PREP_12E_08C_R4_08_GO_NO_GORefresh准备.md
REPORT_12E_08C_R4_08_08D_GO_NO_GO刷新状态.md
```
