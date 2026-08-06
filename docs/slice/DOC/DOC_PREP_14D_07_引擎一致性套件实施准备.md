# DOC_PREP_14D-07 引擎一致性套件实施准备

> 日期：2026-08-06
> 对应任务：`14D-07`
> 状态：`PREPARATION_GATE = BLOCKED`

## 1. 审计结论

`TASKS_14`、`DEV_14`、`PRD_14` 和 `DEMO_14` 多次要求 Worker 替换前通过
`E-01..08`，但当前仓库没有找到这八项用例的逐项定义、输入夹具、比较字段或通过阈值。
`DEMO_14` 只登记了“换 Worker 后过 E-01..08”和 E-03 golden checksum 的引用，不能作为可执行
测试规格。因此 14D-07 目前不能进入开发，也不能把既有回归脚本改名后当成引擎一致性套件。

## 2. 已满足的基础

- `14D-03` 已冻结 `file_contract_v1` 协商和 Worker 版本兼容方向；
- 现有生产矩阵可提供 Legacy/Global、OBJ/3MF、RGBWSV、RIP strict 和报告比较能力；
- 现有 schema、golden、Quick CI、生产 TIFF 解码和 package 校验器可复用；
- Worker 结果合同已包含 `engineVersion`，可以形成实现身份证据。

上述基础只说明“可以建设套件”，不等于 E-01..08 已定义或已通过。

## 3. 阻塞项

### B1：E-01..08 未逐项冻结

必须先通过受控文档修订明确每一项的：

1. 用例名称和要防止的回归；
2. 固定输入资产、Profile、scene 和配置 hash；
3. 比较对象是生产 TIFF、manifest、report、preview、进度还是错误码；
4. 字节相等、语义相等、允许差异和数值容差；
5. Debug/Release、默认/可选 Writer、Legacy/Global 的覆盖边界；
6. 引擎版本变化时 golden 更新审批与 release note 规则。

### B2：真实 Worker 执行入口未完成

14D-08 仍因文件合同到 Facade 映射和 full preflight/repair 适配器缺口而阻断。套件可以先建设
“任意 Worker 路径”的测试驱动，但在真实 Worker 可执行前不能产生替换准入结论。

### B3：安全发布仍待 14D-05

引擎一致性不仅比较成功包，也必须验证失败、取消、崩溃时 staging 和既有包语义。14D-05
完成前，14D-07 不能关闭 D14-D-05..08 类安全用例。

## 4. 建议的 E-01..08 冻结草案

以下仅作为待决策草案，不能直接记为已接受合同：

| 编号 | 建议主题 | 最低比较项 |
|---|---|---|
| E-01 | 合同与请求身份 | schema、jobId、correlationId、capability、sceneHash |
| E-02 | 成功包结构 | manifest、layer list、RGBWSV 协议、RIP strict |
| E-03 | 固定 golden | 生产 TIFF checksum 或经授权的基线修订 |
| E-04 | 报告语义 | scene/material/support/slice report 的稳定字段投影 |
| E-05 | 进度与耗时协议 | 单调进度、终态、timing 字段完整性 |
| E-06 | 错误与拒绝 | 稳定错误码、非法请求、stale scene、拓扑阻断 |
| E-07 | 取消与恢复 | ≤2s、退出码 8、无 staging、旧包不变 |
| E-08 | 可替换性 | 仅替换 Worker 二进制，宿主/DLL/请求不变且前七项通过 |

正式定义必须由 Stage 14 权威决策/DEV/DEMO 受控修订确认，尤其 E-03 是否要求逐字节相等。

## 5. 实施边界

- 套件必须从 `file_contract_v1` 或公开 SPI 驱动 Worker，不得 include Worker 私有实现后直接调用；
- 同一套测试必须接受 Worker 路径/版本作为参数，不把当前实现写死；
- 禁止修改 `p0.rgbwsv.2`、通道顺序、位深、极性和材料闭环规则；
- 性能比较可以记录，但不得在未冻结机器和预算时作为一致性 PASS 条件；
- 真实复杂浮雕的已知 strict 阻断应验证稳定阻断，不得改成伪成功；
- golden 更新必须显式、可审计，禁止自动接受新输出。

## 6. 文件所有权（解阻后）

- `contracts/slicer_engine_conformance_v1.*` 或等价合同：E-01..08 机器可读定义；
- `tests/stage14d_07/*`：参数化 Worker 驱动、比较器和负例；
- `samples/fixtures/stage14d_07/*`：最小、确定性输入；
- `scripts/Run14D07EngineConformance.ps1`：Debug/Release 执行入口；
- `output/benchmarks/14d_07`：忽略的本地证据，不提交大包。

不得由本任务修改生产引擎、材料策略、TIFF Writer 或 UI 来迎合测试。

## 7. 解阻顺序

```text
14D-07-R0 受控冻结 E-01..08
  -> 14D-07-R1 建设参数化套件和固定 fixture
  -> 14D-05 + 14D-08 完成
  -> 14D-07-R2 对当前 Worker 执行完整 Gate
```

## 8. 准备门结论

```text
PREPARATION_GATE=BLOCKED
BLOCKER_1=E_01_TO_E_08_NOT_NORMATIVELY_DEFINED
BLOCKER_2=REAL_WORKER_EXECUTION_ENTRY_NOT_AVAILABLE
BLOCKER_3=SAFE_PUBLISH_GATE_NOT_COMPLETE
```

下一步不是直接写测试，而是先完成 E-01..08 的受控合同定义。只有 B1 关闭后，才能把
14D-07-R1 标记为可开发；完整替换准入还要等待 B2/B3。
