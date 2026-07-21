# REPORT_12E-08C 真实模型拓扑修复专项启动状态

> 文档状态：COMPLETE / NON-PRODUCTION / R3-04 NO-GO
> 日期：2026-07-21

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
repair_then_strict：R2-04 independent evidence validator COMPLETE / non-production only。
```

## 3. 新增阶段

```text
12E-08C-R1 Contract & Eligibility；
12E-08C-R2 Conservative Repair；
12E-08C-R3 Real Model & Release Gate。
```

R1-01..04、R2-01..04、R3-01、R3-01A、R3-02 与 R3-03 已完成实现和验证；R3-04 已输出 NO-GO。

## 4. 文档完成度

已生成：Decision、PRD、DEV、DEMO、ROADMAP、Report Schema、Acceptance Matrix、R1 Prep、Tasks、Codex Prompt
和 AI context handoff。

## 5. 当前允许与禁止

允许：R1 contract/diagnostic/hash/report 和 generated fixture。

禁止：实际生产写包、放宽 strict、warn_and_attempt production、OpenVDB 默认化、协议修改和无审计自动修复。

## 6. 下一任务

```text
12E-08C-R4-01 Model Preflight Contract READY；
R4-01..05 可先建立预检、模式准入、UI 和正常模型正向链；
R4-06..08 等待三个 required OBJ 外部修复版本。
```

## 7. 阶段判断

修复专项非生产证据 COMPLETE；R3-04 决策为 NO-GO；12E-08D 继续保持 BLOCKED。

R3-01A 已把三个真实 OBJ 的 sampled 结论升级为完整 confirmed self-intersection 证据。R3-02 的独立准备
文档已补齐，并明确任务证据完成不等于 production Gate PASS。

## 8. 双模式目标同步

后续产品目标已明确为 `legacy` 与 `global_surface_shell` 两条用户可选流水线。当前 legacy 生产 TIFF 路径
继续可用；本专项只为 global 的生产准入提供 repair/post-strict 证据。global 被阻断时不得自动改用 legacy。
统一 TIFF writer 和 UI 双模式选择分别在 12E-08D 与 12E-09B 实施；当前 R3-04 NO-GO，不得启动 08D。

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
fixture 稳定 blocked。四个真实 case 双运行稳定，未生成新面；其候选已由 R2-04 独立 validator 复核，
12E-08D 继续 BLOCKED。

## 13. R2-04 实际结果

R2-04 新增独立只读 evidence validator，并按固定顺序复核 operation、source/vertex/generated mapping、
material/UV、完整 post-strict、canonical hash 和 non-production safety。negative fixture 覆盖缺失/重复
provenance、属性破坏、operation/hash 篡改和 incomplete strict。四个 required case 各运行两次：闭合
Texture2D 3MF 全 Gate PASS；三个真实 OBJ 均因 sampled self-intersection evidence 返回
`blocked_incomplete_post_strict` 并丢弃 candidate。四 case stable projection 全部一致，始终未写生产包。

## 14. R3-01 实际结果

R3-01 新增确定性 non-manifold pattern classifier，不修改网格。generated fixture 覆盖全部六种 primary
pattern、material/UV conflict、错误属性数量和双运行顺序。四个 required case 各执行两次：

```text
nai_you_new：nonManifold=0，not_present；
aishen_fudiao：59 = duplicate exporter 2 + attribute conflict 57；
meigui_fudiao：10940 = duplicate exporter 10935 + attribute conflict 5；
Texture2D 3MF：nonManifold=0，not_present。
```

四 case repeatability 全部 PASS，两个真实 non-manifold case 均不满足 all-unique fan split，因此继续 manual，
不创建 `split_edge_fan` operation。

## 15. R3-01A 实际结果

R3-01A 新增确定性 AABB BVH 完整自相交分析、候选 pair SHA-256、显式预算/资源阻断、CLI/report/unit 与
真实模型证据脚本。四个 required case 各运行两次，稳定投影 4/4 PASS，budget blocked 为 0：

```text
nai_you_new：236181 candidates，8409 confirmed；
aishen_fudiao：491365 candidates，19270 confirmed，20 coplanar；
meigui_fudiao：346104 candidates，5592 confirmed；
Texture2D 3MF：8 candidates，0 confirmed/coplanar/touching，strict_pass_no_repair。
```

三个真实 OBJ 因完整 confirmed self-intersection 继续 `rejected_self_intersection`，不允许自动修复或伪
strict PASS。R3-02 已据此形成真实模型矩阵，12E-08D 继续 BLOCKED。

## 16. R3-02 实际结果

R3-02 新增 `strict_no_repair` 与 `conservative_repair` 双 lane 真实模型矩阵，每条 lane 双运行：

```text
nai_you_new：8409 confirmed，rejected，mutation/operation=0；
aishen_fudiao：19270 confirmed + 20 coplanar，rejected，mutation/operation=0；
meigui_fudiao：5592 confirmed，rejected，mutation/operation=0；
Texture2D 3MF：complete_no_intersection，no-op strict PASS，validator/attribute PASS。
```

四个 case 的任务证据全部完成，production Gate 0/4 通过。该矩阵证明当前 conservative repair 没有绕过
完整自相交证据制造伪 PASS。R3-03 可以进入非生产 Release core 与 legacy regression，但三个 OBJ 的 global
core 必须明确 `skipped_due_topology`；12E-08D 继续 BLOCKED。

## 17. R3-03 实际结果

R3-03 在 Release lane 中复跑四 case repair 证据。三个真实 OBJ 均保持
`rejected_self_intersection/skipped_due_topology`，闭合 Texture2D 3MF 完成 partition、texture transfer、
raster mapping 与 full closure，`fullClosurePass=true`。global lane 未写 TIFF/package。

Release build PASS、CTest 37/37 PASS；repair-disabled 30 层 TIFF SHA-256 invariant 与 RIP strict 2/2 PASS。
Quick CI 实际被既有 `material_process_top2 widthPx=48/226` baseline 阻断并被显式记录。Release budget
继续 blocked，threshold 未冻结。

## 18. R3-04 决策

Gate 结果为 1/4 global core completed、3/4 topology skipped。三个 required OBJ 没有 admitted candidate，
因此 provenance、post-strict、真实模型性能预算均不完整。R3-04 输出 NO-GO；只有外部修复三个 OBJ、
四 case strict/global PASS、预算冻结、legacy golden 处置和用户重新确认后，才能再次申请 12E-08D。

## 19. R4 插入专项准备

已生成 R4 Decision/PRD/DEV/DEMO/Roadmap/Tasks/Prompt/Prep。专项不立即实现通用复杂自相交重建，而是：

```text
先补齐导入预检和模式相关 fail-closed；
用正常闭合 OBJ/3MF 继续 12E Texture Surface/Model Fill 正向验证；
保留三个真实 OBJ required 身份；
通过外部修复资产 intake、属性审计和 post-strict 重新申请 Gate。
```

R4-01 已 READY；12E-08D 继续 BLOCKED。
