# DEV_09P_R2_ReportSchema_AdmissionGate_CI_UI设计

> 文档版本：v0.1
> 文档状态：Formal DEV / Stage 09P-R2
> 生成日期：2026-07-01
> 阶段定位：Report Schema / Admission Gate / Service Contract / CI / UI

---

## 1. 技术原则

09P-R2 的技术目标是 hardening，不是替换 legacy production path。

必须遵守：

```text
OpenVDB optional and disabled by default；
experimental path 不写真实 OBJ/3MF production RGBWSV；
p0.rgbwsv.2 不变；
RGBWSV channel order 不变；
uint8 bit depth 不变；
black_is_print 不变；
UI 读取 report/package，不依赖 OpenVDB 内部类型；
slicer_core 不依赖 Qt。
```

---

## 2. Experimental Report Schema

建议 schema 名：

```text
p0.experimental_openvdb_shell_cli_report.1
```

建议顶层结构：

```text
schema
input
configSnapshot
openvdb
surfaceShell
productionAdmission
diagnostics
textureTransfer
materialComposer
outputContract
legacyPath
timing
memory
```

关键布尔字段：

```text
legacyPathExecuted
productionPackageWritten
writeProductionRgbwsv
openvdbAvailable
surfaceShellGenerated
productionAllowed
nonProduction
```

---

## 3. Admission Gate 设计

ProductionAdmissionPolicy 应把诊断 issue 映射为稳定结果：

```text
allowed；
blocked；
warning；
nonProduction；
reasonCodes[]；
blockerCodes[]；
warningCodes[]。
```

严格阻断：

```text
confirmed self-intersection；
non-manifold；
duplicate face；
opposite duplicate face；
local winding inconsistency；
OpenVDB level set failed。
```

---

## 4. Service Data Contract

需要稳定的服务边界：

```text
OpenVdbGeometryKernelService
SurfaceShellTextureService
MaterialChannelComposer
ProductionAdmissionPolicy
ReportWriter
```

每个服务必须说明：

```text
input DTO；
output DTO；
ValidationIssue 传播规则；
timing / memory / stats 字段；
允许为空字段；
必须稳定字段；
错误处理策略。
```

---

## 5. Downstream Output Contract

09P-R2 只负责上游切片输出契约，不实现 RIP。

建议 outputContract 字段：

```text
packageSchema
channelOrder
bitDepth
polarity
layerCount
resolution
perLayerStats
textureFidelity
fallbackCodes
diagnosticSummary
```

这些字段用于下游 RIP 工程师分析输入是否足够，不代表本项目实现下游 RIP 处理能力。

---

## 6. Qt UI Integration

09P-R2 UI 最小目标：

```text
读取 experimental report；
显示 OpenVDB availability；
显示 productionAdmission.status；
显示 productionAllowed / nonProduction；
显示 blockerCodes / warningCodes；
显示 legacyPathExecuted；
显示 productionPackageWritten。
```

禁止：

```text
UI 直接调用 OpenVDB 内部算法；
UI 直接依赖 OpenVDB 类型；
UI 绕过 CLI/report 修改 productionAllowed。
```

---

## 7. CI Matrix

建议矩阵：

```text
OpenVDB OFF：build + ctest + run_ci_quick + experimental unavailable smoke；
OpenVDB ON：openvdb smoke + surface shell tests + experimental CLI smoke；
Benchmark：Release optional/manual。
```

推荐脚本：

```text
scripts/run_09p_r2_ci_matrix.ps1
scripts/run_09p_schema_tests.ps1
scripts/run_09p_golden_tests.ps1
```
