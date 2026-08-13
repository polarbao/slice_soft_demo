# REPORT_16C-03 支撑统计扫描融合当前状态

> 状态：**COMPLETE / PASS**
> 日期：2026-08-13
> 对应任务：`16C-03`

## 1. 实现结论

支撑生成阶段不再在后处理前先完整扫描一次 volume。island/filter 汇总继续在生成阶段按层累计，
最终的 support mask、SupportType、layersWithSupport 和连接性统一由
`CalculateSupportGenerationStats()` 在支撑形态、铺底和光油优先级处理完成后扫描一次。

新增诊断字段 `supportStatisticsScanCount`，单模型 Legacy 生产路径固定为 `1`；多模型场景按可见实例
累加。该字段只属于运行时 telemetry，不进入 RGBWSV Package schema，也不参与材料或支撑决策。

## 2. 等价性验证

在同一 MSVC Release 工具链、LibTIFF 后端和提交父基线 `b87d1d2` 上独立构建 before 版本，
与当前实现逐项比较：

| 模型 | Grid | TIFF 层数 | TIFF 不同层 | modelPixels | supportPixels | SupportType totals | RIP strict |
|---|---:|---:|---:|---|---|---|---|
| `nai_you_new` | `290x573x126` | 126 | 0 | 相同 | 相同 | 相同 | PASS/PASS |
| `aishen_fudiao` | `299x531x189` | 189 | 0 | 相同 | 相同 | 相同 | PASS/PASS |
| `meigui_fudiao` | `301x718x182` | 182 | 0 | 相同 | 相同 | 相同 | PASS/PASS |

另以启用 `support.shape` 的爱神配置验证后处理路径：Grid、modelPixels 和 supportPixels 全部相同，
最终统计扫描次数为 1。

## 3. Release 性能

三模型采用 before/after 交替执行，各 3 次，表中为中位数；core-only 不写 TIFF、Preview 或报告文件。

| 模型 | support before ms | support after ms | 变化 | core before ms | core after ms | 变化 |
|---|---:|---:|---:|---:|---:|---:|
| `nai_you_new` | 1682.412 | 1393.274 | -17.19% | 2523.045 | 2223.088 | -11.89% |
| `aishen_fudiao` | 749.715 | 752.231 | +0.34% | 1566.271 | 1541.487 | -1.58% |
| `meigui_fudiao` | 1003.921 | 1012.972 | +0.90% | 3363.317 | 3412.567 | +1.46% |

收益取决于配置是否触发支撑后处理。`nai_you_new` 的 shape/base/varnish 路径消除了第二次扫描，收益明确；
另外两个不触发重复扫描的配置保持约等价，波动在本轮单机噪声范围内。本卡不把代码收敛描述为所有模型
都会获得相同加速。

## 4. 验证

```text
Release slicer_cli build: PASS
Debug stage14d08_r2_slice_materializer_tests: PASS
Debug multimodel_scene_contract_unit_tests: PASS
Debug multi_model_production_service_unit_tests: PASS
三真实模型 before/after TIFF SHA-256: 0 层差异
三真实模型 before/after RIP strict: 6/6 PASS
support.shape 后处理 probe: support totals 相同，scanCount=1
git diff --check: PASS
```

## 5. 边界

本任务没有修改支撑生成规则、SupportType 优先级、材料冲突优先级、TIFF 字节协议、默认 Profile、
SPI v1 或 OpenVDB 默认状态。`16C-04` Range Provider、`16C-05` Layer Compose 融合和有限并行仍是
后续独立任务。
