# DOC_DECISION MV07-Q2 宿主行数门禁改为债务台账

> 文档状态：**ACTIVE / DECIDED**
> 版本：v1.0 ｜ 日期：2026-08-21
> 授权：用户 2026-08-21「由你判断是否需要进一步更新处理」
> 上游：`DOC_PREP_MATVOL_MV_07_宿主接入实施准备.md` MV07-Q2

---

## 1. 问题：规则实现与自身文案矛盾，且长期失败

`tests/stage14e_02/ValidateQtHostBoundary.py` 原规则对
`apps/slicer_ui_host_sim/**` 下**每个** `.cpp/.h` 断言 ≤500 行，失败文案却写
`new host source exceeds 500 lines` —— **文案说的是「新增文件」，实现查的是「所有文件」。**

后果：8 个既有文件超限，`slicer_stage14e02_qt_host_boundary_test` 长期红着（fail-fast
只报第一个），门禁失去信号价值 —— 它既不能阻止新增超限文件（因为本来就是红的，
没人能从红看出新增违规），也无法推动既有文件收缩。

同时 `scripts/ValidateSourceSizeGuard.py` 已经**正确实现了这个意图**：
G1 新增源 >500 失败、G2 base>1000 的既有文件不得增长。所以原规则实质上是
**一个实现错误的重复门禁**。

## 2. 裁定：改为债务台账，不改阈值、不删门禁

```text
台账文件  tests/stage14e_02/HostSourceSizeDebtLedger.json
台账内    列出 8 个既有超限文件及其当前行数；【只允许缩减，不允许增长】
台账外    仍受 500 行上限约束，超限即失败
```

两种失败信息分离，因此「新增违规」与「既有债务」不再混淆：

```text
host source exceeds 500 lines and is not in the debt ledger: <path> (<n>)
host source in the debt ledger grew: <path> (<n> > recorded <m>); shrink it or update the ledger with a justification
```

### 2.1 台账初始内容（2026-08-21 实测）

| 文件 | 记录行数 |
|---|---|
| `HostRipJobController.cpp` | 1181 |
| `HostSliceSettingsPanel.cpp` | 852 |
| `Main.cpp` | 740 |
| `HostSliceJobController.cpp` | 664 |
| `HostSliceSettings.cpp` | 593 |
| `HostSliceJobPanel.cpp` | 590 |
| `HostWorkspaceState.cpp` | 573 |
| `HostModelImportWorkflow.cpp` | 516 |

**台账只减不增**：任何文件缩减后应同步下调记录值，使门禁锁住新的更低水位。

## 3. 为什么不选另外两条路

```text
甲 · 拆分 8 个既有文件
     涉及 Stage 14 已冻结的宿主实现面，收益是纯结构性的、无功能价值，
     风险与工作量都远高于本专项当前需要；且拆分本身会引入新文件，
     仍要受 500 行约束。⇒ 该做，但应是独立技术债卡，不该塞进 MATVOL。

乙 · 删除该规则
     会丢掉「宿主不得新增超限文件」这一真实约束。ValidateSourceSizeGuard 的
     G1 虽覆盖新增源，但它以 git 基线为准、且不覆盖宿主目录的头文件 200 行细则，
     两者不完全等价。⇒ 不删。
```

## 4. 验证

```text
门禁自测        14E-02 Qt host boundary: PASS   ← 首次转绿
变异 1          台账内 HostModelImportWorkflow.cpp 增 1 行 → 按预期 FAIL 并指出 517 > 516
变异 2          台账外新增 520 行文件 → 按预期 FAIL 并指出不在台账内
两处变异还原后均恢复 PASS
```

## 5. 边界

```text
✅ 不改 500 行阈值本身
✅ 不删除任何既有规则；宿主禁引 slicer_core、11 个导出解析、PE 导入表三条规则一字未动
✅ 不改 scripts/ValidateSourceSizeGuard.py 及其配置与空白名单
✅ 台账不是豁免：它锁死当前水位，只允许向下
⛔ 不把 8 个既有文件的拆分纳入本裁定 —— 另立技术债卡
```

## 6. 后续

建议单开一张技术债卡处理台账内文件的拆分，优先级由高到低即台账行数序，
其中 `HostRipJobController.cpp`（1181 行）已超过 `ValidateSourceSizeGuard`
G2 的 1000 行冻结线，它一旦再增长会同时触发两个门禁。

## 7. 修订记录

| 日期 | 版本 | 变更 |
|---|---|---|
| 2026-08-21 | v1.0 | 首版。裁定把 `ValidateQtHostBoundary` 的 500 行规则改为债务台账：台账内只减不增、台账外维持 500 行上限，两类失败信息分离。记录 8 个既有超限文件的初始行数、双向变异检验结果，以及不选「拆分」与「删除」两条路的理由。 |
