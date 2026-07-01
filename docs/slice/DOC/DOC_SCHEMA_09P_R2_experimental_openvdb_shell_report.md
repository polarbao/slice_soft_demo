# DOC_SCHEMA_09P_R2_experimental_openvdb_shell_report

> 文档版本：v0.1
> 文档状态：Formal Schema Contract / Stage 09P-R2-2
> 生成日期：2026-07-01
> Schema：`p0.experimental_openvdb_shell_cli_report.1`

---

## 1. 目的

本文件固定 09P-R2 experimental OpenVDB CLI report 的稳定字段契约。

该 report 只用于 experimental diagnostic / hardening，不代表 production-safe 输出。

---

## 2. 安全不变量

以下字段在 09P-R2 必须保持：

```text
experimentalOpenvdbShell = true
legacyPathExecuted = false
productionPackageWritten = false
writeProductionRgbwsv = false
productionAdmission.productionAllowed = false
productionAdmission.nonProduction = true
outputContract.packageSchema = p0.rgbwsv.2
outputContract.channelOrder = R G B W S V
outputContract.bitDepth = 8
outputContract.polarity = black_is_print
```

`warn_and_attempt` 只能输出 non-production diagnostic report，不能 productionAllowed。

---

## 3. 顶层字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `schema` | string | 固定为 `p0.experimental_openvdb_shell_cli_report.1` |
| `input` | object | 输入配置、模型路径、输出包路径 |
| `configSnapshot` | object | 本次 CLI 归一化后的关键配置快照 |
| `experimentalOpenvdbShell` | bool | 是否进入 experimental CLI path |
| `legacyPathExecuted` | bool | legacy production path 是否执行，09P-R2 必须为 false |
| `productionPackageWritten` | bool | 是否写 production package，09P-R2 必须为 false |
| `noProductionRgbwsv` | bool | CLI 是否显式禁止 production RGBWSV |
| `writeProductionRgbwsv` | bool | experimental config 是否请求 production RGBWSV，09P-R2 必须为 false |
| `openvdb` | object | OpenVDB 编译和运行状态 |
| `surfaceShell` | object | surface shell 是否生成及原因 |
| `diagnostics` | array | `ValidationIssue` 列表 |
| `productionAdmission` | object | production admission 决策 |
| `textureTransfer` | object | texture transfer 是否执行及 fallback 信息 |
| `materialComposer` | object | material composer 是否执行 |
| `outputContract` | object | 下游可消费的 RGBWSV 输出契约摘要 |
| `legacyPath` | object | legacy path 执行状态 |
| `timing` | object | timing 字段容器 |
| `memory` | object | 进程内存统计 |
| `stats` | object | issue/blocker/warning 摘要 |

---

## 4. productionAdmission 字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `mode` | string | `strict_closed` / `warn_and_attempt` / `diagnostic_only` / `repair_then_strict` |
| `status` | string | `production_allowed` / `non_production_only` / `diagnostic_only` / `fail_fast` |
| `allowed` | bool | 与 `productionAllowed` 同步 |
| `blocked` | bool | 是否存在 blocker 或 fail_fast |
| `warning` | bool | 是否存在 warning code |
| `productionAllowed` | bool | experimental CLI 必须为 false |
| `nonProduction` | bool | experimental CLI 必须为 true |
| `reasonCodes` | array | blockerCodes + warningCodes 的稳定去重集合 |
| `blockerCodes` | array | 阻断 production admission 的 code |
| `warningCodes` | array | warning 级 code |
| `suggestedActions` | array | 人类可读建议 |

---

## 5. outputContract 字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `packageSchema` | string | 固定为 `p0.rgbwsv.2` |
| `channelOrder` | array | 固定为 `R G B W S V` |
| `bitDepth` | number | 固定为 8 |
| `polarity` | string | 固定为 `black_is_print` |
| `printValue` | number | 固定为 0 |
| `emptyValue` | number | 固定为 255 |
| `layerCount` | number/null | experimental CLI 不写 production package 时可为 null |
| `resolution` | object | dpi 和 layer thickness |
| `perLayerStats` | object | per-layer stats 是否可用 |
| `textureFidelity` | object | 纹理保真摘要容器 |
| `fallbackCodes` | array | fallback code 摘要 |
| `diagnosticSummary` | object | issue/blocker/warning 摘要 |

---

## 6. 验证入口

Schema contract 的机器可读 golden：

```text
tests/golden/expected/09p_experimental_report_schema.json
```

验证脚本：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_schema_tests.ps1
```

该脚本会生成 `strict_closed`、`diagnostic_only`、`warn_and_attempt` 三种 report，并检查字段完整性和安全不变量。
