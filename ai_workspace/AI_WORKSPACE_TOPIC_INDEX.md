# AI Workspace Topic Index

> 更新日期：2026-07-21

## Stage 12E 双切片模式与真实模型拓扑修复

当前上下文：`context_handoff/2026-07-21_12E-08C-R3-01A完整自相交证据完成.md`

当前任务入口：`docs/codex_task/current/TASKS_12E_08C_真实模型拓扑修复任务清单.md`

下一任务：执行 `12E-08C-R3-02 真实模型 Repair Matrix`。

R2-01..04 已完成 cleanup、受约束 weld/winding、simple boundary fill、source/vertex/generated mapping、
独立 post-strict/attribute/hash validator 和四 case 重复性证据。R3-01 已完成只读 non-manifold pattern
classifier；R3-01A 已完成确定性 AABB BVH 完整证据。三个 required OBJ 均确认存在 self-intersection，
闭合 3MF 保持 no-op strict PASS；四 case 无 budget blocked 且双运行稳定。R3-02 专用准备已完成。12E-08D
继续 BLOCKED；修复专项不得绕过 strict 或直接写 production package。Target State 已固化为
`slicePipeline.mode=legacy|global_surface_shell`，两条 production success 均共用现有 RGBWSV TIFF writer，
但 Router、global production adapter 和 Qt 选择器尚未实现。

## Stage 12C Qt 工作台（历史）

历史上下文：`context_handoff/2026-07-15_12C-R2-05-00_阶段封口准入.md`

历史任务入口：`docs/codex_task/current/TASKS_12C_Qt_UI配置预览任务清单.md`

状态：12C 已完成并被 12D/12E 后续状态取代。

12D 历史准备上下文：`context_handoff/2026-07-13_12D_材料闭环准备准入.md`。
