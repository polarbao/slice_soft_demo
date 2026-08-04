# REPORT 12E 全局纹理壳层与模型填充当前状态

> 状态：STAGE 12E COMPLETE / LEGACY DEFAULT / GLOBAL EXPLICIT CANDIDATE
> 日期：2026-08-03
> 协议：`p0.rgbwsv.2` / `R G B W S V` / `uint8` / `black_is_print`

## 1. 阶段成果

Stage 12E 已完成当前批准范围内的全局纹理壳层、模型填充分区、双模式生产写包、Qt 产品入口、
同层生产预览、真实模型矩阵和 Release 性能收口。

```text
Legacy：默认生产模式；
Global Surface Shell：显式 opt-in 候选模式；
两模式共用 RGBWSV TIFF/package/RIP 边界；
任何失败禁止 silent fallback；
OpenVDB 不是默认生产依赖。
```

## 2. 已完成能力

```text
Texture Surface 与 Model Fill 的互斥、完整分区；
minimum/intermediate/all_texture 三种纹理宽度请求；
Legacy 层深和 Global 三维表面距离各自明确生效；
Model Fill 的 RGB/W/V 材料输出；
lower/internal-void S 与 surface/outer V 材料闭环；
Legacy/Global 共用 uint8 black_is_print TIFF Writer；
Qt 中文 Profile、Effective Config、生产入口和 no-fallback 状态；
生产 TIFF 原生同层预览和 R/G/B/W/S/V 像素探针；
真实 OBJ、3MF、复杂拓扑负向和 Release 性能矩阵。
```

## 3. 12E-10 最终证据

| 任务 | 结果 | 核心证据 |
|---|---|---|
| 10A 同层一致性 | PASS | TIFF、Texture/Fill、W/S/V、closure report 按 layerIndex/zMm 绑定 |
| 10B 真实矩阵 | PASS | 14 行生产 PASS、3 行 BLOCKED_EXPECTED、RIP strict 14/14、fallback 0 |
| 10C 性能矩阵 | PASS | 36/36 计量 PASS、RIP strict 36/36、fallback 0 |
| 10D 文档封口 | COMPLETE | 最终报告、用户说明、索引、任务与上下文同步 |

正向模型包括 xiao_ma、yecan 和 Texture2D checker 3MF。aishen/meigui/titian 保持 strict
`BLOCKED_EXPECTED`，不得生成假 package。

## 4. 性能结论

当前参考机、600 x 600 DPI、0.20 mm 层厚、stripped/uncompressed、Preview 关闭的三次中位数：

```text
Global / Legacy core：1.826x .. 2.562x；
Global / Legacy total：2.244x .. 3.161x；
Global / Legacy peak working set：3.079x .. 4.304x。
```

因此当前没有默认替换 Legacy 的性能依据。Global 保留为几何语义候选，用于显式准入 Profile 和后续
性能工程化，不对普通生产请求自动启用。

## 5. 固定生产合同

```text
schema = p0.rgbwsv.2；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
printValue = 0；
emptyValue = 255；
生产成功必须具有完整 manifest/layers/report；
RIP Reader strict 必须通过；
requestedPipelineMode 必须等于 effectivePipelineMode；
fallbackApplied 必须为 false。
```

## 6. 明确未完成或不在本阶段

```text
复杂浮雕 aishen/meigui/titian 的自动重建与生产准入；
Global 默认替代 Legacy；
设备 buildVolume、原点、机器轴和 22 实例生产预算；
12G-TCWS 白区/RIP 私有信号合同；
OpenVDB 默认生产化；
硬件打印质量和跨设备性能 SLA；
03E PackBits 外部目标 RIP 互操作。
```

## 7. 后续路线

```text
12F：以 10C 基线为输入，先执行 Release benchmark 刷新，再按 profile 证据逐项优化；
Stage 14：按已准备的能力包集成文档推进切片模块封装与打印软件对接；
Stage 13 production：等待设备 buildVolume/坐标轴和 22 实例预算；
12G-TCWS：继续冻结，等待 RIP 合同决策。
```

Stage 12E 的当前批准范围至此封口。后续工作必须作为独立阶段授权，不得回写本报告把候选能力描述为
默认生产能力。
