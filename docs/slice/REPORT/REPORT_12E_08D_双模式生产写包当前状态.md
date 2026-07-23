# REPORT_12E-08D 双模式生产写包当前状态

> 状态：12E-08D-01..04 COMPLETE
> 更新日期：2026-07-23
> 当前结论：Legacy 默认生产稳定；Global 受限 Profile GO；Global 全功能等价 NO-GO

## 1. 已完成能力

```text
slicePipeline.mode = legacy | global_surface_shell；
省略 mode 时兼容 legacy；
Global 前置 preflight + 显式 Profile 双重门禁；
Global 分区/纹理/光栅/full closure 到 RGBWSV layer DTO；
Legacy/Global 共享 TIFF/package/preview/report/RIP writer；
staging 校验和原子发布；
Global blocker 无 package、无 silent fallback；
CLI 输出实际 effective pipeline mode。
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

## 4. 真实模型证据

| 模型 | Profile | Production TIFF | W Fill | RIP |
|---|---|---|---|---|
| xiao_ma 大拇指 | restricted candidate | PASS，30 层 | 5028 pixels | PASS |
| yecan/3 | restricted candidate | PASS，31 层 | 87546 pixels | PASS |

两个模型均记录：

```text
requested=effective=global_surface_shell；
productionAcceptance=admitted；
fallbackApplied=false。
```

## 5. 当前决策

```text
12E-08D 原子任务：COMPLETE；
受限 Global production Profile：GO；
普通 Global 全功能工艺等价：NO-GO；
Legacy：继续作为默认正式生产模式。
```

NO-GO 不否定受限 Profile 的真实 package；它表示支撑、光油和最终 0.01 mm Release matrix 尚未达到
legacy 等价。

## 6. 后续建议

1. 12E-09B 只开放受限 Profile UI，禁用并解释支撑、光油和修复选项。
2. 单列 Global Support/Material Parity 专项，把 S/V 生成接入 full closure。
3. 完成 0.01 mm 真实模型内存、耗时、TIFF/RIP Release matrix。
4. 继续保持复杂自相交模型 strict fail-fast，不因受限 GO 放宽拓扑门禁。

详细执行证据见：

```text
docs/slice/DOC/DOC_EXEC_12E_08D_04_显式Profile与ReleaseMatrix结果.md
```
