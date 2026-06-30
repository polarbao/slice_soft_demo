# slice_soft_demo

这是 UV 3D 打印切片软件的工程化验证仓库，当前已经从 P0 Demo 演进到 OpenVDB 表面壳层纹理实验生产管线的 hardening 前置阶段。

当前最新阶段：

```text
当前最新阶段：09P-R1 已完成
当前工作分支基线：spike/09P-openvdb-experimental-pipeline
下一阶段：09P-R2 文档治理与 OpenVDB experimental path hardening
```

09P-R1 的结论是：OpenVDB 表面壳层纹理能力已经接入 experimental production pipeline 边界，包括 feature flag、diagnostic CLI path、ProductionAdmissionPolicy、OpenVdbGeometryKernelService、SurfaceShellTextureService 和 MaterialChannelComposer bridge。

09P-R1 没有替代 production `slicer_cli`，没有从 experimental path 写真实 OBJ/3MF production RGBWSV TIFF，没有修改 `p0.rgbwsv.2`，也没有修改 RGBWSV 通道顺序、uint8 位深和 `black_is_print` 极性。真实 OBJ / 3MF 当前仍不得直接视为 production-safe。

下一阶段 09P-R2 做文档治理、report schema、admission gate、service contract、UI/report 和 CI matrix hardening；仍不默认启用 OpenVDB，不替代 legacy production path。

## Codex 接手顺序

请在 VS Code Codex 中先让 Codex 阅读：

1. `AGENTS.md`
2. `docs/slice/README.md`
3. `docs/slice/DOC_INDEX_SliceSoft_PRD_DEV_文档体系整理.md`
4. `docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md`

第一条 Codex 指令建议：

```text
请先阅读 AGENTS.md、docs/slice/README.md 和 docs/codex_task/current/TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md。
现在只执行 Task 09P-R2-X，不执行后续 Task。
```

确认当前任务完成并提交后，再按 `TASKS_09P_R2_正式化前置文档治理与Hardening任务清单.md` 的顺序逐个执行后续任务。
