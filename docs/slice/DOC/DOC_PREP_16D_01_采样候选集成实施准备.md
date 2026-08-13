# DOC_PREP_16D-01 采样候选集成实施准备

> 状态：**PREPARATION COMPLETE / IMPLEMENTATION AUTHORIZED**
> 日期：2026-08-12
> 对应任务：`16D-01`

## 1. 已满足依赖

```text
16A-06：S3 获批为 diagnostic opt-in；S0 保持生产默认；
16B-04：姿态接入仍未获独立生产批准，本卡不携带姿态字段；
16C-02：S0/S3/S4 x 1/11/12/22 Release 基线已完成；
Stage 14：SPI v1、11 个导出、Worker/Facade 生命周期保持冻结。
```

因此本卡仅接入 `S3` 采样候选，不接入 P3 调平，不修改生产默认。

## 2. 接入合同

宿主 Profile 使用根字段：

```json
{
  "geometrySampling": {
    "strategy": "legacy_center_sample"
  }
}
```

允许进入生产 Worker 的值只有：

```text
legacy_center_sample
layer_slab_supersample_2x2_at_least_two_candidate
```

S2、S4、未知值、S3 与非 `relief_heightfield` 组合均 fail-closed。字段缺省只代表历史
Legacy Profile，不得推断为 S3。宿主新生成的 Profile 必须显式写入字段并纳入 `profileHash`。

## 3. 模块边界

| 模块 | 本卡职责 |
|---|---|
| Reference Host | 提供代码级 Legacy/S3 枚举并生成自哈希 Profile；默认 Legacy |
| Worker materializer | 校验 Profile、批准列表和模式组合；把策略写入 scene effective config |
| Scene Effective Config | 冻结 `geometrySamplingStrategy`，纳入 config hash |
| Production Service / Facade | 比对 effective contract 与 Profile；不一致或未批准时阻断 |
| Qt | 本卡不新增可见控件；诊断展示属于 16D-02 |

## 4. 不变量

```text
不修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
不修改 SPI v1 和导出数量；
不启用 P3；
不把 S3 设为默认；
Legacy 缺省与既有包输出保持不变；
候选失败不得静默回退 S0。
```

## 5. 验证

```text
Host：Legacy 显式写入且默认不变，S3 hash 闭合，S3+非 relief 拒绝；
Worker：S3 materialize 成功，S4/未知策略拒绝；
Effective Config：策略进入 hash，篡改或 Profile/contract 不一致失败；
Production：候选仅在显式 S3 + relief_heightfield 下执行；
Regression：Stage 14 materializer、Host settings、multi-model production 定向 CTest；
Protocol：Package/RIP strict 与 Legacy zero-drift 继续由 16D-03 统一 Gate 复核。
```
