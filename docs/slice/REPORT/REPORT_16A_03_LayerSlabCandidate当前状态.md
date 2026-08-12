# REPORT_16A-03 Layer Slab Candidate 当前状态

> 状态：**COMPLETE**
> 日期：2026-08-12

## 1. 完成内容

已在 16A-02 Provider 上实现半开 layer slab 候选，并通过顶层
`geometrySampling.strategy=layer_slab_pixel_center_candidate` 显式启用。缺省仍为
`legacy_center_sample`；非 `relief_heightfield` 输入拒绝候选，不做静默回退。

## 2. 语义结果

```text
相交合同：maximumZMm > slabLow && minimumZMm < slabHigh；
边界容差：固定 1e-9 mm；
零厚度列：不占据；
上升/下降斜楔：半开边界无重复、无丢层；
通用 mesh：fail-closed；
Supersample2x2：继续 fail-closed，留给 16A-04。
```

## 3. 验收结果

| 验收项 | 结果 |
|---|---|
| Debug 定向目标构建 | PASS |
| Stage 16 定向 CTest | 2/2 PASS |
| 候选 fixture Package | PASS，24 x 48 x 20 |
| 候选 Package RIP strict | PASS，schema `p0.rgbwsv.2`，warnings=0 |
| Legacy Golden TIFF SHA-256 | 0/25 差异 |
| 默认策略 | 仍为 Legacy |
| RGBWSV 协议 | 未修改 |

## 4. 当前边界

本卡没有实现边界 2x2、通用 mesh layer slab、多区间列或生产默认切换。下一张依赖已满足的卡为
`16A-04 边界自适应 2x2 Candidate`，须独立准备、实现和提交。
