# DOC_ANALYSIS_12B_R1 2.5D Heightfield Fast Path 可行性评估

> 文档状态：Analysis / Stage 12B-R1
> 生成日期：2026-07-08
> 前置报告：`docs/slice/REPORT/REPORT_12B_R0_Benchmark契约与真实Release对比当前状态.md`
> 前置任务：`docs/codex_task/current/TASKS_12B_R1_LegacyHeightfield优化任务清单.md`

## 1. 评估目标

R1-05 用于判断是否应在 12B-R1 内继续实现新的 `2.5D heightfield fast path`。

评估边界：

```text
1. 不替换 legacy slicer_cli production path；
2. 不改变 RGBWSV 协议；
3. 不改变 12A/12D 材料语义；
4. 不把 OpenVDB 作为默认生产引擎；
5. 只判断是否值得在当前阶段继续投入新的 heightfield fast path。
```

## 2. 当前代码事实

当前 `relief_heightfield` 路径已经具备 heightfield-like 的核心结构。

代码证据：

```text
src/slicer_core/slicer.cpp
- sample_relief_heightfield_masks(...)
- compute_relief_lower_layers(...)
- compute_relief_column_ranges(...)
```

当前逻辑：

```text
1. 按 XY 像素列扫描三角面；
2. 对每列记录 z_min / z_max；
3. 将 z_min / z_max 转换为 lower_layer / upper_layer；
4. 对 lower_layer 到 upper_layer 之间填充 model mask；
5. 后续支撑、模型填充、纹理、光油和材料组合复用这些 column range。
```

因此，当前 `relief_heightfield` 并不是传统意义上每层完整三角求交；它已经接近 `topHeight/bottomHeight + column range` 的 2.5D 数据结构。

## 3. Admission 判断

当前可认为适合 2.5D heightfield-like 路径的条件：

```text
1. slicingMode = relief_heightfield；
2. 模型按甲片/浮雕方式处理；
3. 每个 XY 列可以用 z_min / z_max 表达实体区间；
4. 12A/12D 的材料语义可以基于 column range 合成。
```

当前尚未具备的正式 admission：

```text
1. 对任意 OBJ/3MF 自动判定是否近似 2.5D；
2. 判断强倒扣、多壳、多实体叠层是否会让 z_min/z_max 误填；
3. 输出 heightfieldAdmissionReport；
4. 当 admission 失败时自动回退 legacy general mesh path。
```

结论：

```text
当前真实甲片模型已经走 relief_heightfield-like 路径；
如果后续要把 2.5D fast path 推广到非 relief 模式或任意模型，需要新增正式 admission。
```

## 4. R1 Profile 数据

R1-04 优化后，三个真实模型的 Release core-only profile 如下：

| case | coreMs | maskSamplingMs | mask 占比 | supportGenerationMs | support 占比 | layerComposeMs | compose 占比 |
|---|---:|---:|---:|---:|---:|---:|---:|
| `nai_you_new` | 3358.141 | 75.644 | 2.25% | 1788.073 | 53.25% | 882.743 | 26.29% |
| `aishen_fudiao` | 3324.407 | 75.524 | 2.27% | 1805.311 | 54.30% | 924.601 | 27.81% |
| `meigui_fudiao` | 6158.632 | 92.076 | 1.50% | 2801.870 | 45.50% | 1595.716 | 25.91% |

数据来源：

```text
output/benchmarks/12b_r1_nai_you_new_support_shape_fastpath.json
output/benchmarks/12b_r1_aishen_fudiao_support_shape_fastpath.json
output/benchmarks/12b_r1_meigui_fudiao_support_shape_fastpath.json
```

## 5. Mask 差异统计状态

本阶段未实现第二套独立 heightfield fast path，因此没有执行独立 mask diff。

原因：

```text
1. 当前 relief_heightfield 已经是生产使用的 column z_min/z_max 路径；
2. 再实现一套独立 mask 生成器只为比较，会扩大 R1 范围；
3. 当前 maskSamplingMs 只占 coreComputeMs 的 1.50% 到 2.27%，不是主要瓶颈；
4. 当前更大的瓶颈是 supportGenerationMs 和 layerComposeMs；
5. 在 mask sampling 不是瓶颈时，实现新 fast path 的收益不可解释，风险高于收益。
```

当前可用的语义一致性证据：

```text
R1-04 before/after 中 grid、modelPixels、supportPixels 完全一致；
这说明本轮低风险支撑优化没有改变模型 mask 或支撑统计。
```

如果后续必须正式执行 mask diff，需要先新增：

```text
1. heightfieldAdmissionReport；
2. 独立 HeightfieldMaskCandidate；
3. legacyMask vs heightfieldMask 的 per-layer difference；
4. diffPixels / falsePositivePixels / falseNegativePixels / diffRatio；
5. admission 失败时回退 legacy。
```

## 6. 是否值得进入正式实现

当前结论：

```text
R1 阶段不建议继续实现新的 2.5D heightfield fast path。
```

理由：

```text
1. 当前 relief_heightfield 已经使用 z_min/z_max column range；
2. 当前 maskSamplingMs 占比很低，不是主要性能瓶颈；
3. 新 fast path 需要 admission、mask diff、回退机制，范围会明显扩大；
4. 当前最有收益的方向仍是 support generation 和 layer compose；
5. 新 fast path 若要推广到非 relief 模型，需要另开阶段设计。
```

建议状态：

```text
heightfieldFastPathDecision = defer
recommendedNextOptimization = support_generation_or_layer_compose
requiresNewAdmissionBeforeFormalImplementation = true
```

## 7. 后续建议

短期：

```text
1. 不在 12B-R1 继续实现新 heightfield engine；
2. 保持当前 relief_heightfield production path；
3. 继续优化 supportGenerationMs；
4. 进一步拆分 layerComposeMs，判断是否可做通道合成/cache 优化。
```

中期：

```text
1. 如果未来出现非 relief 模型 maskSamplingMs 明显变高，再启动 heightfield admission；
2. 将 heightfield fast path 定义为可回退候选引擎；
3. 先做 mask diff 和 semantic diff，再谈 production 替代。
```
