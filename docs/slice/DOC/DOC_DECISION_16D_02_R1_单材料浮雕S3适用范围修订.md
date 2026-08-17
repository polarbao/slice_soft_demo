# DOC_DECISION_16D-02-R1 单材料浮雕 S3 适用范围修订

> 状态：**ACCEPTED / USER AUTHORIZED / S3 EXPLICIT OPT-IN ONLY**
> 日期：2026-08-14
> 上游：`DOC_DECISION_16A_06_采样候选决策刷新.md`
> 对应任务：`16D-02-R1`

## 1. 问题

Reference Host 原实现把 S3 与“启用彩色纹理”等同。单材料浮雕 Profile 虽然使用
`relief_heightfield` 几何链路，但因 `texture.enabled=false` 被 Qt 和 Host Profile Builder
错误拒绝，导致白墨 W、光油 V 单材料浮雕无法显式选择 S3。

## 2. 决策

S3 的准入依据改为“是否使用 `relief_heightfield`”，而不是“是否启用纹理”。允许范围为：

```text
彩色纹理 relief_heightfield；
单材料白墨 W relief_heightfield；
单材料光油 V relief_heightfield。
```

单材料 W/V 仅在用户显式选择 S3 时切换到 `relief_heightfield`。纹理仍保持关闭，材料通道
仍分别为 W 或 V。普通无纹理 RGB Profile 与 S3 的组合继续 fail-closed。

## 3. 不变量

```text
S0 继续为生产默认；
不自动迁移现有 Profile；
不启用纹理采样或贴图回退；
不改变 W/V 材料通道、支撑策略和 Model > Support 优先级；
不修改 p0.rgbwsv.2、uint8、black_is_print 或 R G B W S V；
不授权 S3 或 P3 成为生产默认。
```

## 4. 验收

1. 普通无纹理 RGB + S3 仍被拒绝；
2. 单材料白墨 W + S3 生成 `relief_heightfield`，且纹理关闭；
3. 单材料光油 V + S3 生成 `relief_heightfield`，且纹理关闭；
4. Profile hash 在策略切换后闭合；
5. Qt 可选择 S3 并提交 W/V 单材料浮雕 Profile；
6. 单材料光油 S3 样例可生成 RGBWSV Package，并通过 RIP strict；
7. S0 默认值及既有生产协议不变。

## 5. 回退

若真实模型出现不可接受的几何扩大、支撑扰动或耗时回归，用户可在“切片设置”中切回
S0。回退不需要修改模型、材料 Profile 或输出协议。
