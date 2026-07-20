# REPORT_12E-08C 真实模型拓扑修复专项启动状态

> 文档状态：IN PROGRESS / R1 COMPLETE / R2-01..03 COMPLETE / R2-04 READY
> 日期：2026-07-20

## 1. 启动原因

12E-08C 证据任务已完成，但三个真实 OBJ 被 strict topology 阻断，无法取得真实生产模型的全局纹理分区和
Release 性能证据。12E-08D 因此继续 BLOCKED。

## 2. 当前基线

```text
12E-08A raster mapping：COMPLETE / diagnostic only；
12E-08B full material closure：COMPLETE / diagnostic only；
12E-08C Release evidence：COMPLETE / budget blocked；
12E-08D production admission：BLOCKED；
Mesh Repair contract/hash/report skeleton：R1-01 COMPLETE；
Eligibility Policy：R1-02 COMPLETE；
Generated fixture/golden：R1-03 COMPLETE；
Real-model pre-repair baseline：R1-04 COMPLETE；
Conservative repair implementation：R2-01 cleanup、R2-02 guarded topology、R2-03 simple boundary COMPLETE；
repair_then_strict：placeholder / non-production only。
```

## 3. 新增阶段

```text
12E-08C-R1 Contract & Eligibility；
12E-08C-R2 Conservative Repair；
12E-08C-R3 Real Model & Release Gate。
```

R1-01..04 与 R2-01..03 已完成实现和验证；R2-04 已完成准备，可以在用户明确启动后实施。

## 4. 文档完成度

已生成：Decision、PRD、DEV、DEMO、ROADMAP、Report Schema、Acceptance Matrix、R1 Prep、Tasks、Codex Prompt
和 AI context handoff。

## 5. 当前允许与禁止

允许：R1 contract/diagnostic/hash/report 和 generated fixture。

禁止：实际生产写包、放宽 strict、warn_and_attempt production、OpenVDB 默认化、协议修改和无审计自动修复。

## 6. 下一任务

```text
12E-08C-R2-04 Post-Repair Strict 与 Attribute Guard。
```

## 7. 阶段判断

修复专项准备 COMPLETE；R1-01..04、R2-01..03 代码实施 COMPLETE；R2-04 READY；12E-08D 继续保持 BLOCKED。

R2/R3 的独立准备文档已补齐；R2-01..03 已完成，R2-04 解除前置阻断，R3 仍按 Gate 阻断。R1-04
发现的 sampled self-intersection 缺口已新增 R3-01A 准备。

## 8. 双模式目标同步

后续产品目标已明确为 `legacy` 与 `global_surface_shell` 两条用户可选流水线。当前 legacy 生产 TIFF 路径
继续可用；本专项只为 global 的生产准入提供 repair/post-strict 证据。global 被阻断时不得自动改用 legacy。
统一 TIFF writer 和 UI 双模式选择分别在 12E-08D 与 12E-09B 实施，不改变当前下一任务 R2-04。

## 9. R1-04 实际基线

```text
nai_you_new：manual，V/T/C=58924/117705/10，boundary=113，degenerate=1；
aishen_fudiao：manual，V/T/C=42193/84533/10，boundary=3，nonManifold=59，opposite=2，degenerate=1；
meigui_fudiao：manual，V/T/C=34722/76926/2，nonManifold=10940，opposite=7192；
Texture2D 3MF：strict_pass_no_repair，V/T/C=8/12/1。
```

四个 case 均完成 config/source/geometry/attribute hash 与两次 stable projection 对照。三个 OBJ 的
self-intersection evidence 为 sampled 而非 confirmed，因此新增 R3-01A 完整证据任务。R1 没有执行 repair、
post-strict 或 production write。

## 10. R2-01 实际结果

R2-01 新增保守 cleanup、source mapping 和真实模型重复性脚本。两个含退化面的 OBJ 各补齐 1 个 adapter
过滤记录；`aishen_fudiao`/`meigui_fudiao` 的 opposite duplicate 未删除；闭合 3MF 维持 no-op strict PASS。
三个 OBJ 仍为 manual，证明 cleanup 没有通过放宽 Gate 制造伪 PASS。

## 11. R2-02 实际结果

R2-02 新增受约束 vertex weld、唯一 local winding 传播、组件不隐式 merge 和 vertex provenance。
generated fixture 覆盖 safe weld、跨组件近邻、退化阻断、唯一 winding、non-orientable 歧义；四个真实 case
均双运行稳定。三个 OBJ 无新增 weld/flip，保持 manual；闭合 3MF 保持 no-op strict PASS。该结果作为
R2-03 的输入基线，12E-08D 继续 BLOCKED。

## 12. R2-03 实际结果

R2-03 新增简单 boundary loop 的拓扑、平面、凸性、预算、完整相交证据和统一无 UV 材质守门，并输出
`generatedTriangleMappings[]`。generated 缺顶 box 可补 2 面后 strict PASS；非平面、超预算、UV 和 branching
fixture 稳定 blocked。四个真实 case 双运行稳定，未生成新面；R2-04 READY，12E-08D 继续 BLOCKED。
