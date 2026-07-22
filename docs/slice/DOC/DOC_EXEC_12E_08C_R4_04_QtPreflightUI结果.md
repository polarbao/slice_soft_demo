# DOC_EXEC_12E-08C-R4-04 Qt Preflight UI 结果

> 文档状态：COMPLETE
> 日期：2026-07-22
> 原子任务：12E-08C-R4-04
> 下一任务：R4-05 Clean Positive Matrix（准备已完成，等待明确启动）
> 历史说明：本文件的“下一步”记录 R4-04 完成时状态；R4-07 当前 Gate 以
> `DOC_DECISION_12E_08C_R4_07_开发准入放宽规则.md` 为准。

## 1. 完成结论

R4-01..03 的 ModelPreflight 合同、两阶段服务和模式准入已经接入 Qt 5.15 调试工作台。三个切片入口
在启动 `ProcessRunner/QProcess` 前都提交 fresh preflight；global blocker 不提供继续按钮，也不会自动回退
legacy。OpenVDB 诊断入口同样先展示共享预检事实，但允许在 topology blocked 时继续只读诊断。

本任务没有实现模型修复，没有修改切片算法、RGBWSV/TIFF/manifest/RIP，也没有把
`global_surface_shell` 提升为生产模式。

## 2. 实现模块

```text
ModelPreflightController：QThreadPool 异步执行、generation、取消、stale、cache 复用；
ModelPreflightPresenter：稳定 code 到中文状态、问题和建议的只读映射；
SlicePreflightCoordinator：单 pending action、legacy warning 确认、global fail-closed；
ModelPreflightPanel：中文状态、模式、准入、四列问题表、重新检测和取消；
MainWindow：三条切片入口和 OpenVDB 诊断入口统一接线；
slicer_cli --openvdb-capability-json：外部候选工具只读能力探针。
```

Controller 的 worker 只持有共享 core service、取消 token 和受互斥保护的 callback bridge；窗口销毁时不会
等待完整几何审计，也不会向已销毁 QObject 回调。连续请求只允许最新 generation 更新 UI 或放行动作。

## 3. UI 行为

左侧运行区新增紧凑的预检状态、当前模式、重新检测和取消按钮；右侧新增“模型预检”页，展示级别、问题、
数量和建议。状态覆盖待检测、检测中、通过、警告、阻断、过期和取消；未知错误码显示中文安全回退文案，
原稳定 code 保留在 tooltip 中。

入口行为：

```text
导入模型并切片：fresh legacy preflight -> PASS 或明确确认 warning 后启动；
运行切片：同一 legacy gate；
导入模型并 OpenVDB 候选切片：fresh global preflight + capability PASS 后启动；
导入模型并 OpenVDB 诊断：先展示共享事实，再运行只读 diagnostic，不宣称 global admitted。
```

配置变化会标记结果 stale。候选切片获准后，UI 内部加载刚刚获准的 generated config 时使用受控抑制，避免
把同一个已审计配置误标为 stale；后续用户编辑仍会正常使结果过期。

## 4. OpenVDB Capability

新增只读命令：

```powershell
slicer_cli --openvdb-capability-json
```

固定 schema 为 `slicesoft.openvdb_capability.12e_r4.1`，输出
`compiledWithOpenVdb/runtimeAvailable/version/reason`。它不要求 config、不加载用户模型、不写 package 或
report。Qt controller 使用独立 `QProcess` 调用候选可执行文件，校验 schema、退出码和 runtime；探针可取消，
并有 120 秒 fail-closed 超时。

本机实测：

```text
build/Debug/slicer_cli.exe：compiledWithOpenVdb=false，runtimeAvailable=false，exit=2；
build-openvdb-09p/Debug/slicer_cli.exe：compiledWithOpenVdb=true，runtimeAvailable=true，version=12.0.1，exit=0。
```

## 5. 模型依据同步

R4-04 检测依据继续以
`REPORT_12E_08C_R4_模型资产预检清单.md` 中“可直接进入”的 7 个 OBJ 为准：

```text
xiao_ma_wu_yu_new 下 5 个 OBJ；
yecan/3.obj；
yecan/4.obj。
```

主正向模型为 `MF_Xiao_ma_Damuzhi_ty02.obj`，独立复核为 `yecan/3.obj`；`yecan/4.obj` 是未跟踪用户资产，
本任务只保留其清单身份，不提交、不覆盖。`model` 目录没有 strict PASS 3MF，正向 Texture2D 3MF 继续使用
`samples/models/3mf/texture2d_checker_cube.3mf`。这些信息已存在于 `.agents/docs/project-profile.md`、R4
任务清单、R4 准备文档和模型资产清单；R4-02 的真实输入测试持续覆盖主 OBJ、独立 OBJ 与 checker 3MF。

正常模型只用于 R4-01..05，不替代 `nai_you/aishen/meigui` required 修复资产，也不解除 R4-06..08。

## 6. 验证结果

已通过：

```text
Debug：slicer_debug_ui、slicer_cli、model_preflight_pipeline_gate_unit_tests；
CTest：model_preflight_contract/service/admission/pipeline_gate，4/4 PASS；
Qt --self-test：startup、experimental-report-summary PASS；
UI Smoke：model-preflight-states PASS；
UI Smoke：model-preflight-one-click-gate PASS，admitted=2、blocked=2、真实 capability verified；
UI Smoke：model-preflight-lifecycle PASS，latest generation/cancel/close 无崩溃；
UI Smoke：workspace-layout-sizes PASS，1440x900、1280x720、1024x768；
OpenVDB ON candidate Debug build 与 capability probe PASS。
```

`scripts/run_ci_quick.ps1` 在 300 秒命令窗口内未完成；随后单独执行 golden，确认仍停在已知历史基线：

```text
material_process_top2 widthPx expected=48 actual=226
```

该差异在 R4-04 前已存在，本任务未修改 golden、模型姿态、生产 writer 或固定协议。

## 7. 边界与下一步

```text
R4-04：COMPLETE；
R4-05：READY / WAIT EXPLICIT EXECUTION；
R4-06：BLOCKED BY EXTERNAL REPAIRED ASSETS；
R4-07：WAIT R4-06；
R4-08：WAIT R4-07；
12E-08D：继续 BLOCKED。
```
