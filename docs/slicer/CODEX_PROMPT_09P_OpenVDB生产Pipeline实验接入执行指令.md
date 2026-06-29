# CODEX_PROMPT_09P_OpenVDB生产Pipeline实验接入执行指令

> 阶段：09P-R1  
> 当前基线：`spike/09B-R3-shell-production-readiness`  
> 工作分支建议：`spike/09P-openvdb-experimental-pipeline`

## 1. 必读文件

每次执行 09P-R1 任务前先阅读：

```text
AGENTS.md
docs/slicer/CODEX_TASKS_09P_R1.md
docs/slicer/REPORT_09B_R3_壳层纹理生产准入前诊断策略收口当前状态.md
docs/slicer/DOC_DECISION_09B_R3_真实模型拓扑生产准入策略.md
docs/slicer/PRD_09P_OpenVDB表面壳层纹理实验生产管线接入.md
docs/slicer/DEV_09P_OpenVDB与LegacyPipeline融合设计.md
```

## 2. 单任务执行规则

```text
每次只执行用户明确指定的一个 Task。
不要自动执行下一个 Task。
开始前运行 git status --short，并要求工作树干净。
只修改当前 Task 相关文件。
完成后运行 Task 指定验证命令。
验证通过后立即提交。
不要 push，除非用户明确要求。
```

## 3. 生产安全红线

```text
OpenVDB 默认关闭。
legacy slicer_cli 生产路径不得被替代。
warn_and_attempt 只能 nonProduction。
strict_closed 必须拒绝 non-manifold / duplicate / opposite duplicate / local winding。
confirmed self-intersection 必须 fail_fast。
production RGBWSV 协议不修改。
```

禁止修改：

```text
p0.rgbwsv.2
RGBWSV channel order
uint8 bit depth
black_is_print polarity
legacy slicer_cli 默认路径
production TIFF writer 行为
```

## 4. R3 结论

实现 09P 时必须继承 R3 结论：

```text
真实 OBJ/3MF 当前没有 confirmed self-intersection。
R2 的 AABB 自相交候选在 R3 narrow-phase 中主要被归类为 false positive。
真实模型 production blocker 已转移为 non-manifold、duplicate/opposite duplicate、local winding、multi-component admission。
真实 OBJ/3MF 当前不能直接 production RGBWSV 输出。
```

## 5. 推荐提交顺序

```text
docs: align current phase with 09B-R3 readiness
docs: add 09P experimental pipeline planning documents
09P: add production admission policy
09P: attach admission decision to shell diagnostics
09P: add disabled experimental OpenVDB pipeline config
09P: introduce OpenVDB geometry kernel service
09P: add surface shell texture service
09P: add material channel composer bridge
09P: add guarded experimental OpenVDB slicer CLI path
09P: add experimental pipeline validation script
docs: add 09P-R1 experimental pipeline status report
```

Task 12 如果没有修复改动，不提交空 commit。

## 6. 当前 Task 之后

完成 Task 02 后停止。不要继续 Task 03，除非用户明确要求执行 Task 03。
