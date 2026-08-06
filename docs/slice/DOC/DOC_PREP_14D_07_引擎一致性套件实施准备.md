# DOC_PREP_14D-07 引擎一致性套件实施准备

> 日期：2026-08-06
> 对应任务：`14D-07`
> 状态：`PREPARATION_GATE = PASS / R2 EXECUTED`

## 1. 审计结论

本文件最初审计出的三个阻塞项均已关闭。`14D-07-R1` 已冻结 E-01..08 合同、fixture
身份和参数化 runner；`14D-04B`、`14D-05` 与三项真实 Worker executor 已完成；
`14D-07-R2` 已通过公开 Worker 进程/文件合同执行当前 Worker Gate。该结论建立的是当前
Worker 基线，不代表未来替换 Worker 自动获得准入资格。

## 2. 已满足的基础

- `14D-03` 已冻结 `file_contract_v1` 协商和 Worker 版本兼容方向；
- 现有生产矩阵可提供 Legacy/Global、OBJ/3MF、RGBWSV、RIP strict 和报告比较能力；
- 现有 schema、golden、Quick CI、生产 TIFF 解码和 package 校验器可复用；
- Worker 结果合同已包含 `engineVersion`，可以形成实现身份证据。

上述基础已经由 R1/R2 转化为可执行门禁和 Debug/Release 证据。

## 3. 阻塞项

### B1：E-01..08 未逐项冻结（已关闭）

必须先通过受控文档修订明确每一项的：

1. 用例名称和要防止的回归；
2. 固定输入资产、Profile、scene 和配置 hash；
3. 比较对象是生产 TIFF、manifest、report、preview、进度还是错误码；
4. 字节相等、语义相等、允许差异和数值容差；
5. Debug/Release、默认/可选 Writer、Legacy/Global 的覆盖边界；
6. 引擎版本变化时 golden 更新审批与 release note 规则。

### B2：真实 Worker 执行入口未完成（已关闭）

`geometry.preflight.full`、`geometry.repair` 与 `slice.rgbwsv` 已由生产 Worker 精确注册，
并经 `WorkerClient` 和 `file_contract_v1` 执行。

### B3：安全发布仍待 14D-05（已关闭）

14D-05 已完成 owned staging/backup/lease、安全发布、旧包保护和双保险恢复；R2 的 E-07
通过真实 Worker 再次验证取消时限、旧包不变与零 owned 临时产物。

## 4. 已冻结并执行的 E-01..08

以下主题已由 `contracts/slicer_engine_conformance_v1.json` 冻结，并由
`stage14d07_engine_conformance_gate` 执行：

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

E-03 对同一 Worker、fixture 与 Profile 的生产 TIFF 要求逐字节相等；未来修改该准则必须走
受控合同修订，不能在测试代码中静默放宽。

## 5. 实施边界

- 套件必须从 `file_contract_v1` 或公开 SPI 驱动 Worker，不得 include Worker 私有实现后直接调用；
- 同一套测试必须接受 Worker 路径/版本作为参数，不把当前实现写死；
- 禁止修改 `p0.rgbwsv.2`、通道顺序、位深、极性和材料闭环规则；
- 性能比较可以记录，但不得在未冻结机器和预算时作为一致性 PASS 条件；
- 真实复杂浮雕的已知 strict 阻断应验证稳定阻断，不得改成伪成功；
- golden 更新必须显式、可审计，禁止自动接受新输出。

## 6. 文件所有权

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
PREPARATION_GATE=PASS
BLOCKER_1=CLOSED_BY_14D_07_R1
BLOCKER_2=CLOSED_BY_14D_08_REAL_EXECUTORS
BLOCKER_3=CLOSED_BY_14D_05_AND_14D_04B
EXECUTION_GATE=PASS_DEBUG_AND_RELEASE
```

当前 Worker 已建立 E-01..08 基线。未来替换 Worker 时必须使用相同 runner 和冻结输入重新
生成证据；仅更换 Worker 路径/版本，不得同时修改宿主、模块、请求或 golden。
