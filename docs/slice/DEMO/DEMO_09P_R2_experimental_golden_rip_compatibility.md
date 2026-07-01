# DEMO_09P_R2_experimental_golden_rip_compatibility

> 文档版本：v0.1
> 文档状态：Formal DEMO / Stage 09P-R2-6
> 生成日期：2026-07-01
> 适用范围：experimental golden / downstream output contract / texture fidelity compatibility

---

## 1. 目标

09P-R2-6 的目标是定义 experimental OpenVDB 路线如何做 golden 和下游兼容性判断。

该任务不实现 RIP，不写真实 OBJ/3MF production RGBWSV TIFF，不把 experimental output 解释为 production-safe。

---

## 2. Golden 类型

| 类型 | 产物 | 用途 | 是否 production-safe |
|---|---|---|---:|
| Report golden | `p0.experimental_openvdb_shell_cli_report.1` JSON | 检查 schema、安全不变量、admission、diagnostics | 否 |
| Diagnostic golden | `diagnostics` / `productionAdmission` / `stats` | 检查 stable issue code 与 blocker/warning 行为 | 否 |
| Output contract golden | `outputContract` | 检查 channelOrder、bitDepth、polarity、下游字段容器 | 否 |
| Texture fidelity golden | `textureFidelity` / `fallbackCodes` | 检查纹理链路信息是否可表达 | 否 |
| In-memory RGBWSV summary golden | `MaterialChannelComposerStats` 或摘要 JSON | 后续比较 composer 行为，不写 TIFF | 否 |

当前 09P-R2 experimental CLI 只生成 diagnostic report；`surfaceShell`、`textureTransfer`、`materialComposer` 均为 not executed，因此当前 golden 重点是字段存在性、安全不变量和不可生产边界。

---

## 3. Downstream Compatibility 不等于 Production Safe

下游兼容性只表示 report/package 摘要字段足够被 RIP 或 UI 工程师理解。

它不表示：

```text
1. 可以写 production RGBWSV package；
2. 可以进入 RIP 半色调；
3. 可以发送设备 bitstream；
4. 可以绕过 ProductionAdmissionPolicy；
5. 可以把 warn_and_attempt 视为 production-safe。
```

---

## 4. 可稳定比较字段

这些字段可以作为 golden exact match：

```text
schema
experimentalOpenvdbShell
legacyPathExecuted
productionPackageWritten
writeProductionRgbwsv
productionAdmission.mode
productionAdmission.status
productionAdmission.productionAllowed
productionAdmission.nonProduction
productionAdmission.reasonCodes
outputContract.packageSchema
outputContract.channelOrder
outputContract.bitDepth
outputContract.polarity
outputContract.printValue
outputContract.emptyValue
outputContract.perLayerStats.available
outputContract.textureFidelity.available
surfaceShell.generated
textureTransfer.executed
materialComposer.executed
legacyPath.executed
legacyPath.productionPackageWritten
```

---

## 5. 趋势字段

这些字段只能做趋势或范围判断，不能 exact match：

```text
memory.workingSetBytes
memory.peakWorkingSetBytes
timing
performance
stats.issueCount
stats.blockerCount
stats.warningCount
textureCacheBytes
nearest query stats
active voxel count
```

原因：

```text
不同构建类型、OpenVDB ON/OFF、Windows 进程状态、依赖版本和模型规模都会影响这些值。
```

---

## 6. 机器可读 Golden Contract

R2-6 新增：

```text
tests/golden/expected/09p_experimental_output_contract.json
```

该文件固定：

```text
report schema；
required admission modes；
RGBWSV output contract safety invariants；
exact comparable fields；
trend-only fields；
diagnostic reason code。
```

---

## 7. 验证脚本

R2-6 新增：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_golden_tests.ps1
```

脚本行为：

```text
1. 运行 run_09p_schema_tests.ps1 生成 strict_closed / diagnostic_only / warn_and_attempt reports；
2. 运行 run_09p_cli_experimental_tests.ps1 验证 CLI diagnostic path；
3. 读取 tests/golden/expected/09p_experimental_output_contract.json；
4. 对 outputContract、textureFidelity、fallbackCodes、legacyPath、surfaceShell 和 materialComposer 做 exact match；
5. OpenVDB OFF 时额外检查 OPENVDB_UNAVAILABLE diagnostic；
6. 不写 production RGBWSV package。
```

---

## 8. 验收命令

基础验证：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_golden_tests.ps1
```

阶段验证：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_09p_experimental_pipeline_tests.ps1
git diff --check
```

OpenVDB ON 验证可通过 `run_09p_experimental_pipeline_tests.ps1 -OpenVdbBuildDir <dir>` 扩展执行，但不是默认轨道。

---

## 9. 后续扩展

后续进入 10 阶段或 09P-R3/R4 时，可扩展：

```text
1. real-model report golden；
2. per-layer summary golden；
3. texture fidelity threshold golden；
4. in-memory RGBWSV summary golden；
5. downstream handoff checklist；
6. UI report viewer golden fixture。
```

扩展时仍必须保持：

```text
p0.rgbwsv.2 不变；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
experimental output 不自动变成 production-safe。
```
