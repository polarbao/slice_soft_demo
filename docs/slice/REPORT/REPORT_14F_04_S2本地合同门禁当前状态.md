# REPORT_14F-04 S2 本地合同门禁当前状态

> 状态：**SLICER-SIDE COMPLETE / EXTERNAL VALIDATION DEFERRED**  
> 日期：2026-08-07  
> 权威条款：`DOC_DECISION_14_S2_RIP接口合同定案.md`、`DOC_DECISION_14F_外部验证延期与接口冻结.md`

## 1. 本次结论

14F-04 已完成切片仓库内可执行的 S2 C1-C7 合同门禁：冻结机器合同、
`rip_output_validator` 本地验证入口、2 个正例和 7 个稳定错误码负例均已落地并通过。

本结果只证明**本仓库对冻结 S2 接缝的表达和 fail-closed 行为完整**。目标 RIP 实际输出、
打印软件 `ChannelSplitter`、极性映射、干净机及实物证据均未执行，继续标记为
`EXTERNAL_VALIDATION_DEFERRED`。

## 2. 交付物

| 交付物 | 路径 | 作用 |
|---|---|---|
| S2 机器合同 | `contracts/slicer_rip_s2_contract.json` | 冻结输入协议、量化、白区、输出组织、混合与外部状态 |
| 本地验证器 | `scripts/RipOutputValidator.ps1` | 验证 RIP 输出描述符并返回稳定 C1-C7 错误码 |
| Gate runner | `scripts/Run14F04S2ContractGate.ps1` | 执行 2 正例、7 负例并生成机器证据 |
| grayBits fixtures | `tests/contracts/stage14f04/` | 覆盖 grayBits=1 与 grayBits=2 |
| CTest | `stage14f04_s2_contract_gate` | Release/CI 可重复入口 |
| VS Code task | `SliceSoft 14F: Run S2 Contract Gate (Release)` | 本地显式执行入口 |

## 3. C1-C7 覆盖

| 编号 | 本地合同项 | 负例错误码 | 结果 |
|---|---|---|---|
| C1 | `p0.rgbwsv.2`、RGBWSV、uint8、`black_is_print`、0/255 | `S2_C1_INPUT_CONTRACT_INVALID` | PASS |
| C2 | `profile.device.grayBits` 必填且只允许 1/2 | `S2_C2_GRAY_BITS_INVALID` | PASS |
| C3 | W/S/V 最大墨滴：2-bit=6/9/9，1-bit=2/3/3 | `S2_C3_QUANTIZATION_LIMIT_INVALID` | PASS |
| C4 | manifest 权威白区语义；不透明白为 W 真实材料；禁哨兵 | `S2_C4_WHITE_SEMANTICS_INVALID` | PASS |
| C5 | 每层一个 `rip_%06d.tif`、交错多通道、≥7 samples、stripped、非 tiled | `S2_C5_OUTPUT_LAYOUT_INVALID` | PASS |
| C6 | `dropRange` 与抽样墨滴均不得超过对应通道上限 | `S2_C6_DROP_RANGE_INVALID` | PASS |
| C7 | Support:Varnish=6:1、clamp=9；极性映射外部状态不得伪记 PASS | `S2_C7_MIXING_OR_EXTERNAL_STATE_INVALID` | PASS |

## 4. 实际验证

```text
Run14F04S2ContractGate.ps1:
  positive grayBits=1  PASS
  positive grayBits=2  PASS
  negative C1..C7      7/7 EXPECTED FAILURE PASS

ctest -C Release -R stage14f04_s2_contract_gate:
  1/1 PASS
```

机器证据输出在构建目录：
`build-slicesoft/main/stage14f04_evidence/Release/stage14f04_s2_gate.json`。

## 5. 边界与下一步

1. 本地验证器消费的是可审计 S2 输出描述符，不冒充目标 RIP 的真实 TIFF 解析器。
2. S2-R1 极性映射仍由 RIP 与打印软件共同持有，本仓库只验证其状态明确为延期。
3. 目标 RIP 未来接入时，应由适配器从真实输出提取同结构描述符，再复用本门禁。
4. 14F-05 可汇总 14F-01..04 的切片侧证据，并以
   `SLICER PACKAGE READY / INTERFACES FROZEN / EXTERNAL ACCEPTANCE DEFERRED` 收口。
