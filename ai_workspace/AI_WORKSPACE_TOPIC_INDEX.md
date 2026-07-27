# AI Workspace Topic Index

> 更新日期：2026-07-27

## Stage 13 模型场景、排版联合切片与 TIFF 原生预览

当前上下文：`context_handoff/2026-07-27_13B-01完成与09A-02准备.md`

当前任务入口：`docs/codex_task/current/TASKS_13_模型场景排版联合切片与TIFF预览任务清单.md`

当前状态：

```text
13-R0、P0 需求/设计/验证和 17 个近程原子任务准备完成；
实现代码已开始，近程任务完成 2/17；
13A=模型俯视和实例变换；
13B=最多 11x2 规则排版和联合切片；
13C=TIFF 原生 RGBWSV 单通道与 RGB+S+W+V；
13A-01=COMPLETE；
13B-01=COMPLETE；
13C-01=READY / 排在 identity wave 后；
跨阶段执行看板=docs/codex_task/current/TASKS_12_13_后续开发计划总览清单.md；
12G-TCWS=FROZEN；
下一任务=scene-aware 12E-09A-02；
09A-02 独立 PRD/DEV/DEMO/PREP/PROMPT=READY；
13A-02=依赖已解除，按 identity wave 顺序稍后执行。
```

## Stage 12E 双切片模式与真实模型拓扑修复

当前上下文：已被 Stage 13 上下文纳入，09C 已完成。

当前任务入口：`docs/codex_task/current/TASKS_12E_09B_Qt双模式生产入口任务清单.md`

当前生产主线状态：`12E-09B/09C COMPLETE`。
独立诊断支线下一任务：`12E-09A-02 Effective Config`，scene identity 已由 13B-01 冻结。

R4-08-R2 已 GO，Quick-CI-R1 与候选预算闭环，12E-08D-01..06 已完成。Legacy 保持默认；Global restricted
和 material-parity 为显式 opt-in 候选，默认替换 Legacy 因时间和峰值内存 NO-GO。12E-09B-01/02 已完成
能力目录、fail-closed UI DTO、原子 Production Effective Config、stale override 清理和审计投影。
12E-09A-02..06 保持独立 diagnostic UI 支线。12E-09C 已为 X=635/Y=600、显式 600/600 兼容、
Reader/光油/preview 建立并审计完整准备文档，等待 09B-06。
09A-05、09B-05 和 09C 是 12E-10A 的联合前置。

## Stage 12C Qt 工作台（历史）

历史上下文：`context_handoff/2026-07-15_12C-R2-05-00_阶段封口准入.md`

历史任务入口：`docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md`

状态：12C 已完成并被 12D/12E 后续状态取代。

12D 历史准备上下文：`context_handoff/2026-07-13_12D_材料闭环准备准入.md`。
