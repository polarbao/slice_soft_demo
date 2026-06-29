# PRD_09P_OpenVDB表面壳层纹理实验生产管线接入

> 阶段：09P  
> 子阶段：09P-R1  
> 文档类型：PRD  
> 当前基线：`spike/09B-R3-shell-production-readiness`  
> 工作分支建议：`spike/09P-openvdb-experimental-pipeline`

## 1. 背景

09B-R3 已完成 OpenVDB 表面壳层纹理生产准入前诊断策略收口。R3 结论显示：

```text
真实 OBJ/3MF 当前没有 confirmed self-intersection。
R2 的 AABB 自相交候选在 R3 narrow-phase 中主要被归类为 false positive。
真实模型 production blocker 已转移为 non-manifold、duplicate/opposite duplicate、local winding、multi-component admission。
真实 OBJ/3MF 当前不能直接 production RGBWSV 输出。
```

09P 的目标是在不破坏 legacy production path 的前提下，为 OpenVDB 表面壳层纹理链路建立 experimental production pipeline 接入边界。

## 2. 目标

09P-R1 只做：

```text
feature flag
experimental path
diagnostic/report
service abstraction
```

09P-R1 需要让工程上可以显式选择 OpenVDB 表面壳层纹理实验路径，并在 report 中看到 admission、topology、texture transfer、nonProduction 等诊断信息。

## 3. 非目标

09P-R1 不做：

```text
默认启用 OpenVDB
替代 legacy slicer_cli production path
直接写真实 OBJ/3MF 的 production RGBWSV TIFF
修改 p0.rgbwsv.2
修改 RGBWSV 通道顺序
修改 uint8 位深
修改 black_is_print 极性
把 warn_and_attempt 输出声明为 production-safe
复杂 mesh repair
compensated varnish
support clearance
设备/RIP 工艺联调
```

## 4. 用户与工程场景

面向角色：

```text
算法开发者：验证 OpenVDB shell/interior/texture transfer 是否能通过实验路径跑通。
QA：通过稳定 issue code、report 和脚本确认实验路径没有破坏 production 协议。
工艺工程师：查看模型是否因为 topology blocker 不适合直接生产。
UI/应用开发者：后续通过 report/diagnostic 连接 Qt Debug UI。
```

典型场景：

```text
legacy slicer_cli 默认路径继续正常输出 production RGBWSV。
显式开启 experimental OpenVDB path 时，只输出 diagnostic/report 或 nonProduction 实验结果。
strict_closed 遇到生产阻塞拓扑时拒绝 production admission。
warn_and_attempt 可以继续实验 preview/report，但只能 nonProduction。
```

## 5. 功能范围

09P-R1 功能范围：

```text
ProductionAdmissionPolicy
OpenVdbGeometryKernelService
SurfaceShellTextureService
MaterialChannelComposer bridge
slicer_cli experimental flag
09P validation script
09P-R1 status report
```

这些能力只建立边界和诊断，不改变 production RGBWSV package 协议。

## 6. Production Safety Rules

必须遵守：

```text
OpenVDB 默认关闭。
legacy slicer_cli 生产路径不得被替代。
warn_and_attempt 只能 nonProduction。
strict_closed 必须拒绝 non-manifold / duplicate / opposite duplicate / local winding。
confirmed self-intersection 必须 fail_fast。
production RGBWSV 协议不修改。
```

冻结协议：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
```

## 7. Feature Flag 策略

09P-R1 所有 OpenVDB experimental path 必须由显式配置或 CLI flag 开启。

默认行为：

```text
experimental.openvdbPipeline.enabled = false
engine = legacy
admissionMode = strict_closed
allowNonProductionOutput = false
writeProductionRgbwsv = false
```

OpenVDB 不可用时，experimental path 必须输出 `OPENVDB_UNAVAILABLE` 或等价稳定 diagnostic，不影响 legacy path。

## 8. Admission Policy

准入模式：

```text
strict_closed
warn_and_attempt
diagnostic_only
repair_then_strict
```

规则：

```text
MESH_SELF_INTERSECTION_CONFIRMED => fail_fast
MESH_NON_MANIFOLD_EDGES => strict_closed blocker
MESH_DUPLICATE_FACES => strict_closed blocker
MESH_OPPOSITE_DUPLICATE_FACES => strict_closed blocker
MESH_LOCAL_WINDING_INCONSISTENCY => strict_closed blocker
warn_and_attempt => productionAllowed=false, nonProduction=true
diagnostic_only => 不写 production package
repair_then_strict => 09P-R1 只保留占位，不做自动修复
```

真实 OBJ/3MF 当前仍不得直接视为 production-safe。

## 9. 验收标准

09P-R1 完成后应满足：

```text
OpenVDB experimental path 默认关闭。
legacy slicer_cli production path 行为不变。
稳定 admission decision 可由 issue code 推导。
strict_closed 阻断 R3 已确认的真实模型 blocker。
warn_and_attempt 只能 nonProduction。
diagnostic/report 中可见 blockerCodes、warningCodes、productionAllowed、nonProduction。
所有新增能力通过任务指定验证命令。
```

## 10. 风险与限制

当前限制：

```text
真实 OBJ/3MF 拓扑质量仍不满足 production strict admission。
R3 未实现 topology repair。
09P-R1 只建立实验接入边界，不做 production package 输出收口。
OpenVDB ON 构建依赖本机 vcpkg/OpenVDB 环境。
```

主要风险：

```text
误把 nonProduction 实验结果当作 production-safe。
OpenVDB path 意外成为默认路径。
配置或 CLI flag 绕过 admission policy。
report 字段不稳定导致后续 UI/CI 难以判断。
```
