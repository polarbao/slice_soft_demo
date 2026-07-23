# REPORT_12E-08D 双模式生产写包当前状态

> 状态：12E-08D-01..06 COMPLETE
> 更新日期：2026-07-23
> 当前结论：Legacy 默认 GO；Global 两个显式候选 GO；Global 默认替换 Legacy 性能 NO-GO

## 1. 已完成能力

```text
slicePipeline.mode = legacy | global_surface_shell；
省略 mode 时兼容 legacy；
Global 前置 preflight + 显式 Profile 双重门禁；
Global 分区/纹理/光栅/full closure 到 RGBWSV layer DTO；
Legacy/Global 共享 TIFF/package/preview/report/RIP writer；
staging 校验和原子发布；
Global blocker 无 package、无 silent fallback；
CLI 输出实际 effective pipeline mode；
Global lower/internal-void support 写 S；
Global surface/outer varnish 写 V；
Model > OuterVarnishShell > Support > Empty 优先级通过 full closure。
```

## 2. 固定输出合同

两种 production success 均必须满足：

```text
p0.rgbwsv.2；
R G B W S V；
uint8；
black_is_print；
0 = 打印；
255 = 不打印；
完整 TIFF layer list；
RIP Reader strict PASS。
```

Preview 和报告是附加数据，不能代替 TIFF。

## 3. Global 当前可用范围

已准入 Profile：

```text
global_surface_shell_restricted_candidate
```

支持：

```text
strict PASS 的闭合 OBJ；
global 3D distance 纹理壳层；
RGB 纹理；
W 白墨 Model Fill；
600 dpi；
stripped/tiled 共享 TIFF writer 能力；
PNG/PPM preview；
Release package 和 RIP strict。
```

当前受限：

```text
support 必须关闭；
surface/outer varnish 必须关闭；
closure repair 必须关闭；
legacy materialPolicy/materialRoleMapping 不得混入；
OpenVDB 不是 production backend，继续 optional/OFF。
```

08D-05 新增材料等价候选：

```text
global_surface_shell_material_parity_candidate
```

它支持 `lower` 支撑、内部空洞支撑、surface varnish 和 XY outer varnish。upper/both、
full_vertical_projection、支撑 shape/offset/dilation 继续阻断。

## 4. 真实模型证据

| 模型 | Profile | Production TIFF | W Fill | RIP |
|---|---|---|---|---|
| xiao_ma 大拇指 | restricted candidate | PASS，30 层 | 5028 pixels | PASS |
| yecan/3 | restricted candidate | PASS，31 层 | 87546 pixels | PASS |

材料等价候选：

| 模型 | S | V | RIP |
|---|---:|---:|---|
| xiao_ma 大拇指 | 2453268 | 317680 | PASS |
| yecan/3 | 3286174 | 406422 | PASS |

两个模型均记录：

```text
requested=effective=global_surface_shell；
productionAcceptance=admitted；
fallbackApplied=false。
```

0.01 mm 最终 Release 矩阵：

| 路径 | xiao_ma 总耗时 / 峰值内存 | yecan 总耗时 / 峰值内存 | TIFF/RIP |
|---|---:|---:|---|
| Legacy | 9.820 s / 684.9 MiB | 14.300 s / 868.8 MiB | PASS |
| Global restricted | 47.284 s / 5610.9 MiB | 117.839 s / 7470.3 MiB | PASS |
| Global material parity | 84.392 s / 5715.8 MiB | 73.532 s / 7597.2 MiB | PASS |

Global 相对 Legacy 的总耗时为 4.82x~8.59x，峰值内存为 8.19x~8.74x。此结果不影响
显式候选的协议/材料正确性 GO，但阻断 Global 成为默认引擎。

## 5. 当前决策

```text
12E-08D-01..06：COMPLETE；
受限 Global production Profile：GO；
0.01 mm Global material-parity opt-in candidate：GO；
Global 默认替换 Legacy：NO-GO，原因是当前性能与峰值内存回退；
Legacy：继续作为默认正式生产模式。
```

NO-GO 不否定 Global 两个显式 Profile 的真实 package；它只表示当前 Global 不满足“默认替换
Legacy”的性能与资源条件。

## 6. 后续建议

1. 12E-09B 可开放 Legacy/Global 模式选择；默认 Legacy，Global 必须显式选择并按 Profile 锁定能力。
2. UI 应显示 Global 候选的高内存/高耗时状态，不得暗示其已替换 Legacy。
3. 后续独立性能专项优化 Global 全体积 mask/evidence 常驻和重复副本。
4. 继续保持复杂自相交模型 strict fail-fast，不因候选 GO 放宽拓扑门禁。

详细执行证据见：

```text
docs/slice/DOC/DOC_EXEC_12E_08D_04_显式Profile与ReleaseMatrix结果.md
docs/slice/DOC/DOC_EXEC_12E_08D_05_Global材料等价结果.md
docs/slice/DOC/DOC_EXEC_12E_08D_06_0.01mmRelease矩阵与最终分层结论.md
```
