# PRD_12B_R2 OpenVDB SDF Utility 定位

> 文档版本：v0.1
> 文档状态：PRD / Stage 12B-R2
> 生成日期：2026-07-08
> 前置阶段：12B-R0 Benchmark 契约、12B-R1 Legacy 优化与 Heightfield 评估

## 1. 背景

12B-R0 已确认 OpenVDB 当前不能作为 legacy production slicer 的替代引擎：

```text
OpenVDB Release CLI 在 R0 验证时不可用；
OpenVDB Debug candidate 可运行，但 outputSemanticsComparable=false；
replacementPass=false；
OpenVDB 不覆盖 12A/12D production RGBWSV 材料语义。
```

12B-R1 已确认 legacy 路径仍是当前 production path，并完成了 coarse profile、真实模型 baseline 和第一项低风险支撑路径优化。R1 还确认当前 relief_heightfield 已经是 column z_min/z_max 路径，新的 2.5D fast path 不适合作为 R1 后续重点。

因此 12B-R2 的产品定位不是“继续尝试把 OpenVDB 切换为默认切片引擎”，而是判断 OpenVDB 是否应作为 SDF utility 能力，为 legacy production path 提供局部辅助。

## 2. Current State

当前可作为实现依据的事实：

```text
1. legacy slicer_cli 仍是唯一默认 production path；
2. OpenVDB 通过 CMake USE_OPENVDB 选项保持 optional / disabled-by-default；
3. 当前已有 OpenVDB surface-shell、texture transfer、diagnostic report、admission gate 和 smoke 脚本；
4. 09P-R2 已完成 experimental OpenVDB hardening，但明确不写 production RGBWSV TIFF；
5. 12A/12D 已定义 ModelFill、Support、OuterVarnishShell、SurfaceVarnish、MaterialClosure 等正式材料语义。
```

当前不能作为 production 结论的内容：

```text
1. OpenVDB candidate Debug 耗时；
2. warn_and_attempt 输出；
3. diagnostic_only report；
4. 只生成 preview 或 prototype report 的 surface-shell demo；
5. 未通过 strict admission 的真实模型结果。
```

## 3. Target State

R2 完成后，项目应能回答：

```text
1. OpenVDB 是否适合作为 outer varnish shell offset 工具；
2. OpenVDB 是否适合作为 clearance / distance diagnostic 工具；
3. OpenVDB 是否适合作为 complex topology diagnostic 辅助工具；
4. OpenVDB 是否适合作为 material closure gap analysis 辅助工具；
5. 哪些能力应进入后续 production-adjacent utility，哪些能力应继续停留在 experimental；
6. OpenVDB OFF 默认构建是否完全不受影响；
7. OpenVDB ON 构建是否有稳定 smoke / utility report / regression 入口。
```

## 4. 非目标

12B-R2 不做：

```text
1. 不把 OpenVDB 设为默认生产切片引擎；
2. 不替换 legacy slicer_cli --config production path；
3. 不修改 p0.rgbwsv.2、RGBWSV 通道顺序、uint8、black_is_print；
4. 不从 experimental OpenVDB diagnostic path 写 production RGBWSV TIFF；
5. 不默认启用 OpenVDB；
6. 不把 OpenVDB 变成默认构建强制依赖；
7. 不实现 mesh repair / repair_then_strict 自动修复；
8. 不承诺 OpenVDB 在当前甲片模型上比 legacy 更快。
```

## 5. Utility 候选能力

### 5.1 Outer Varnish Shell Offset

目标：

```text
评估 OpenVDB SDF offset 是否可用于外侧光油壳层候选 mask 的生成、质量诊断或厚度一致性检查。
```

要求：

```text
只输出 utility report 或候选 mask 统计；
不得直接覆盖 legacy outerVarnish 生产 mask；
必须与 12A thicknessMm / pixelPitchUm / allowXYExpansion 语义对齐。
```

### 5.2 Clearance / Distance Diagnostic

目标：

```text
评估 SDF distance 是否可用于检测模型局部间隙、壳层厚度不足、支撑距离异常。
```

要求：

```text
输出 diagnostic report；
不修改 production TIFF；
明确 distance 单位、voxelSize、narrowBand 和误差边界。
```

### 5.3 Complex Topology Diagnostic

目标：

```text
复用 OpenVDB / geometry diagnostic 结果，判断真实 OBJ/3MF 模型为何不能进入 strict_closed candidate。
```

要求：

```text
输出 blocker / warning / admissionMode；
保持 confirmed self-intersection、non-manifold、boundary edges 等 strict blocker；
不把 warn_and_attempt 视作 production-safe。
```

### 5.4 Material Closure Gap Analysis Assist

目标：

```text
为 12D material closure 提供可选 SDF 辅助诊断，例如近表面空隙、材料层距离、局部壳层断裂。
```

要求：

```text
12D 的语义闭环仍以 RGBWSV / semantic masks 为生产真源；
OpenVDB 只能提供辅助诊断，不得单独判定 PASS；
报告必须标明 source=openvdb_sdf_assist 或 equivalent。
```

## 6. 用户故事

### US-12B-R2-01 判断 OpenVDB 的真实角色

作为开发负责人，我希望知道 OpenVDB 到底应该继续做切片引擎，还是只作为 SDF 工具模块，这样后续不会继续在错误方向上投入。

验收：

```text
R2 输出 capability matrix；
每个能力给出 promote / keep experimental / reject 结论；
结论引用 R0/R1 benchmark 和当前代码事实。
```

### US-12B-R2-02 保护默认构建

作为维护者，我希望 OpenVDB OFF 默认构建不受 R2 影响，确保普通用户仍可用 legacy slicer_cli 和 Qt UI。

验收：

```text
USE_OPENVDB=OFF 默认 build 可通过；
OpenVDB 相关配置在 OFF 下给出 unavailable diagnostic；
legacy production package 不变。
```

### US-12B-R2-03 评估光油壳层 SDF utility

作为工艺人员，我希望知道 OpenVDB 是否能改善外侧光油壳层的几何质量，而不是让它改变已有 production 输出。

验收：

```text
至少一个 closed fixture 输出 SDF shell utility report；
report 包含 voxelSize、shellThickness、activeVoxels、candidateShellPixels 或等价统计；
legacy outerVarnish 输出不被自动替换。
```

## 7. 成功标准

12B-R2 完成必须满足：

```text
1. R2 PRD / DEV / DEMO / TASKS / CODEX_PROMPT / REPORT 完整；
2. OpenVDB OFF 默认构建验证通过或记录阻断原因；
3. OpenVDB ON lane 有明确 smoke / utility 验证入口；
4. 至少完成 outer varnish / clearance / topology / material closure 四类候选能力的矩阵评估；
5. 每类能力给出 promote / keep experimental / reject；
6. 不改变 legacy production path；
7. 不改变 RGBWSV 协议；
8. 形成 REPORT_12B_R2。
```

## 8. R2 输出结论格式

R2 最终报告必须包含：

```text
Current State：当前代码实际支持哪些 OpenVDB utility；
Target State：哪些 utility 适合继续推进；
Historical State：09/09P/11A/11B 已完成但非 production 的内容；
Pending Confirmation：哪些能力需要用户或工艺确认；
Decision Matrix：promote / keep experimental / reject；
Next Stage：后续是 12B-R2-followup、12D 联动、还是回到 legacy 性能优化。
```
