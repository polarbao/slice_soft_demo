# slice_soft_demo

这是 UV 3D 打印切片软件的工程化验证仓库，当前已经从 P0 Demo 演进到 OpenVDB 表面壳层纹理实验链路的生产准入前诊断阶段。

当前最新阶段：

```text
当前最新阶段：09B-R3 已完成
当前工作分支基线：spike/09B-R3-shell-production-readiness
下一阶段：09P OpenVDB 表面壳层纹理实验生产管线接入
```

09B-R3 的结论是：OpenVDB 表面壳层纹理链路已经完成生产准入前诊断收口，包括 narrow-phase 自相交、稳定 issue code、repeat/clamp 纹理边界 fixture、Windows process peak working set 和真实模型 topology admission 策略。

09B-R3 没有接入 production `slicer_cli`，没有写 production RGBWSV TIFF，没有修改 `p0.rgbwsv.2`，也没有修改 RGBWSV 通道顺序、uint8 位深和 `black_is_print` 极性。真实 OBJ / 3MF 当前仍不得直接视为 production-safe。

下一阶段 09P-R1 只做 experimental path / feature flag / diagnostic / report，不默认启用 OpenVDB，不替代 legacy production path。

## Codex 接手顺序

请在 VS Code Codex 中先让 Codex 阅读：

1. `AGENTS.md`
2. `docs/slicer/CODEX_HANDOFF_切片软件开发上下文.md`
3. `docs/slicer/REPORT_09B_R3_壳层纹理生产准入前诊断策略收口当前状态.md`
4. `docs/slicer/CODEX_TASKS_09P_R1.md`

第一条 Codex 指令建议：

```text
请先阅读 AGENTS.md 和 docs/slicer/CODEX_TASKS_09P_R1.md。
现在只执行 Task 01，不执行后续 Task。
```

确认 Task 01 完成并提交后，再按 `CODEX_TASKS_09P_R1.md` 的顺序逐个执行后续任务。
