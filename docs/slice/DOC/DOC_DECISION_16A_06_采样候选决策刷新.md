# DOC_DECISION_16A-06 采样候选决策刷新

> 状态：**ACCEPTED / S3 DIAGNOSTIC OPT-IN / PRODUCTION DEFAULT S0**
> 日期：2026-08-12
> 对应任务：`16A-06`
> 证据：`REPORT/REPORT_16A_05_机器可读候选矩阵当前状态.md`

## 1. 决策结论

Stage 16 后续 A/B、性能和集成验证采用以下冻结口径：

```text
生产默认：S0 Legacy，不变；
首选诊断候选：S3，Layer Slab + Pixel Center + 固定 2x2，>=2/4 子采样命中；
薄特征上限对照：S4，Layer Slab + Pixel Center + 固定 2x2，>=1/4 子采样命中；
历史中间候选：S2，仅保留合同/回归，不进入首选集成入口；
默认切换：DEFER，不在 16A-06 授权。
```

S3 获准用于 `16B-03` 姿态 A/B 的统一采样口径、`16C-02` Release 基线和
`16D-01` 的显式 opt-in 配置接入。该批准不等于把 S3 设为生产默认。

## 2. 选择依据

16A-05 在合成 fixture、Reality 101..105 和 Stage 15 白区载体上完成 S0/S2/S3/S4
机器可读矩阵，并对可写包资产完成 Package/RIP strict 8/8 PASS。

| 维度 | S3 判断 | S4 判断 |
|---|---|---|
| 尺寸忠实 | Reality 5/5 的模型占用增量均小于 S4 | 更容易向边界外扩张 |
| 薄特征保存 | 需要至少 2/4 子采样命中，抑制单点噪声 | 1/4 即占用，作为薄特征保存上限对照 |
| 支撑扰动 | Reality 5/5 均比 S4 克制 | Reality 5/5 支撑增量均高于 S3 |
| Z 层偏差 | 0 或 +1 层 | 0 或 +1 层 |
| 协议闭合 | Package/RIP strict PASS | Package/RIP strict PASS |

因此首选 S3 作为“尺寸忠实优先、兼顾薄特征”的诊断候选；S4 保留为更包容的边界对照，
不得因为局部像素更多就推断工艺更优。

## 3. 未授权的事项

本决策不授权以下行为：

```text
不修改 GeometryOccupancyPolicy 的默认值；
不把现有 Legacy Profile 静默迁移到 S3/S4；
不修改 p0.rgbwsv.2、RGBWSV、uint8 或 black_is_print；
不改变 Stage 15 材料闭合和 Model > Support 优先级；
不把 16A-05 单次计时当成 p50/p95；
不批准 P2/P3 姿态进入生产路径；
不删除 S0 回退路径。
```

## 4. 后续 Gate

```text
16B-03：使用 S3 比较 P0/P2/P3，输出姿态、接触、尺寸、支撑与准入差异；
16C-02：重新运行 S0/S3/S4 的 Release cold/warm 基线；
16D-01：可先接入 S3 显式 opt-in；姿态字段必须继续等待 16B-04；
默认切换：仍需 16C 性能证据、16D 统一回归、必要工艺证据及独立用户授权。
```

若任一后续 Gate 出现 Legacy 漂移、协议失败、不可解释的尺寸扩大或不可接受的性能/内存回归，
生产配置继续使用 S0，候选入口 fail-closed。

## 5. 可追溯证据

```text
matrix schema = slicesoft.stage16.sampling_matrix.1
assetCount = 7
Package/RIP strict = 8/8 PASS
matrix SHA-256 = F52E758C05D1F020BEEB4DB08524CB70B6501EA8675757A2ABF364E9FFD490B9
```

