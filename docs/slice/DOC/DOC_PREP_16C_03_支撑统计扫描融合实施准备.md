# DOC_PREP_16C-03 支撑统计扫描融合实施准备

> 状态：**PREPARATION COMPLETE / IMPLEMENTATION READY**
> 日期：2026-08-13
> 对应任务：`16C-03`

## 1. 目标与当前重复扫描

`generate_support_masks()` 在支撑写入完成后会完整扫描全部 layer/pixel，计算 support/type totals、
layersWithSupport 和连接性。随后支撑形态、铺底、Outer Varnish 优先级可能改变 mask/type，当前
`RecalculateSupportGenerationStats()` 会再次完整扫描 volume，并重复同一统计逻辑。

本卡只融合这两处统计实现并消除“先统计、随后必然重算”的路径，不改变任何支撑生成决策。

## 2. 实现边界

```text
提取单一 SupportGenerationStats 扫描器；
generate_support_masks() 在没有后处理重算需求时使用一次扫描；
存在 shape/baseProjection/varnish 后处理时，延迟到最终 mask/type 后只扫描一次；
island/filter totals 保持生成阶段增量统计，不由新扫描器重算；
support connectivity 仍来自最终 support mask；
不改变 SupportType 优先级、支撑像素、材料优先级或报告 schema。
```

## 3. 验收

```text
grid/model/support/type totals/hash 与基线完全一致；
support_report、slice_report 和 RGBWSV TIFF/RIP strict 零语义漂移；
三真实模型 Release before/after 至少各 3 次，报告 median 与 build identity；
性能没有改善时如实报告，不以减少代码行数冒充性能收益。
```

## 4. 文件所有权

| 文件 | 责任 |
|---|---|
| `src/slicer_core/slicer.cpp` | 融合最终支撑统计扫描，保持生成语义 |
| `tests/stage16/*` | 增加统计一致性/调用路径证据 |
| `scripts/*stage16*` | 三模型 Release before/after 采集 |
| `REPORT_16C_03_*` | 记录输出一致性和真实性能数据 |

## 5. 风险与回退

主要风险是漏算某个 `SupportType`、在后处理前读取非最终 mask，或把 island totals 重置。实现时必须
保留旧扫描函数作为测试基线，先做 A/B 对照；只有输出完全一致后才能移除旧路径。若 Release
收益不稳定，仍可保留单一统计实现，但不得声称已达到性能 Gate。

## 6. 与相邻任务关系

16C-03 可与 16D-02 的 UI 诊断设计独立；两者都可能触及 telemetry/report 展示，因此代码提交必须
串行落地。16C-04 的 range provider 和 16C-05 的 layer compose 扫描不在本卡实施。
