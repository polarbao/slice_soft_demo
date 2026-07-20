# REPORT_12E-08C 真实模型拓扑修复专项启动状态

> 文档状态：IN PROGRESS / R1 COMPLETE / R2-01 READY
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
Conservative repair implementation：NOT IMPLEMENTED；
repair_then_strict：placeholder / non-production only。
```

## 3. 新增阶段

```text
12E-08C-R1 Contract & Eligibility；
12E-08C-R2 Conservative Repair；
12E-08C-R3 Real Model & Release Gate。
```

R1-01..04 已完成实现和验证；R2-01 已完成准备，可以在用户明确启动后实施。

## 4. 文档完成度

已生成：Decision、PRD、DEV、DEMO、ROADMAP、Report Schema、Acceptance Matrix、R1 Prep、Tasks、Codex Prompt
和 AI context handoff。

## 5. 当前允许与禁止

允许：R1 contract/diagnostic/hash/report 和 generated fixture。

禁止：实际生产写包、放宽 strict、warn_and_attempt production、OpenVDB 默认化、协议修改和无审计自动修复。

## 6. 下一任务

```text
12E-08C-R2-01 Degenerate/Duplicate Cleanup。
```

## 7. 阶段判断

修复专项准备 COMPLETE；R1-01..04 代码实施 COMPLETE；R2-01 READY；12E-08D 继续保持 BLOCKED。

R2/R3 的独立准备文档已补齐；R2-01 已解除 R1 阻断，R2-02..04/R3 仍按前置 Gate 阻断。R1-04 发现的
sampled self-intersection 缺口已新增 R3-01A 准备。

## 8. 双模式目标同步

后续产品目标已明确为 `legacy` 与 `global_surface_shell` 两条用户可选流水线。当前 legacy 生产 TIFF 路径
继续可用；本专项只为 global 的生产准入提供 repair/post-strict 证据。global 被阻断时不得自动改用 legacy。
统一 TIFF writer 和 UI 双模式选择分别在 12E-08D 与 12E-09B 实施，不改变当前下一任务 R2-01。

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
