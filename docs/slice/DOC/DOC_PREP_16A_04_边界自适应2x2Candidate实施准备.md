# DOC_PREP_16A-04 边界自适应 2x2 Candidate 实施准备

> 状态：**PASS / IMPLEMENTATION AUTHORIZED**
> 日期：2026-08-12
> 任务：`16A-04`

## 1. 准入结论

`16A-03` 已冻结半开 Layer Slab 语义，并证明 Legacy 默认输出零漂移。本卡可以在不改变
`p0.rgbwsv.2`、材料优先级和生产默认的前提下，实现两个仅限 `relief_heightfield` 的显式候选。

## 2. 配置身份

```text
layer_slab_supersample_2x2_at_least_two_candidate：S3，逐层覆盖 >=2/4；
layer_slab_supersample_2x2_any_hit_candidate：S4，逐层覆盖 >=1/4；
legacy_center_sample：继续为缺省；
layer_slab_pixel_center_candidate：继续保留为 S2。
```

两个 2x2 候选只允许 `slicingMode=relief_heightfield`。通用 mesh、多区间列和未知阈值必须
fail-closed，不允许回退到 S0/S2。

## 3. 固定采样合同

以一个输出像素左下角为局部原点，四个子样本位置和遍历顺序固定为：

```text
0: (0.25, 0.25)
1: (0.75, 0.25)
2: (0.25, 0.75)
3: (0.75, 0.75)
```

每个子样本独立形成单区间 Heightfield 列，并使用 16A-03 的半开 Layer Slab 相交语义。
覆盖阈值必须按“同一输出层、同一输出像素”统计，不允许先在整列上投票再扩展到全部层。
采样不使用随机 jitter；相同网格、模型和配置必须产生确定输出。

## 4. 边界自适应合同

```text
明确内部：中心命中，且八邻域中心均命中；复用中心列作为四个覆盖样本，不重复求交；
明确外部：不落入任何三角形 XY 投影候选范围；直接为空；
边界候选：其余落入三角形 XY 投影候选范围的像素；执行固定 2x2 求交。
```

三角形投影候选范围允许保守扩大，目的是保留中心未命中的亚像素薄特征；不得仅依赖中心命中
的八邻域判断外部，否则 S4 会丢失孤立亚像素特征。

中间数据只保存四组二维单区间列，不生成或常驻高分辨率三维体。边界中心未命中但候选通过时，
纹理代表点选择四个子样本中最高 Z 的有效表面点；同 Z 时沿固定三角形遍历顺序确定。

## 5. Provider 合同

`LayerOccupancyRequest.columns` 继续表示输出像素列和结果宽度；2x2 候选额外提供按
`outputColumnIndex * 4 + sampleIndex` 展平的覆盖子样本列。Provider 使用逐列临时层计数，
不得分配 `layerCount * pixelCount * 4` 的常驻三维覆盖体。

## 6. 实施边界

```text
不修改 Legacy 和 S2 的配置身份与默认行为；
不修改 RGBWSV、uint8、black_is_print、TIFF Writer 或 RIP Reader；
不加入形态学膨胀、最小线宽、孤儿连通域保活或 4x4；
不把 S3/S4 暴露为生产默认；
不在本卡执行 16A-05 真实模型候选矩阵。
```

## 7. 验证计划

```powershell
cmake --build build-slicesoft/main --config Debug --target `
  stage16_geometry_sampling_fixture_tests `
  stage16_layer_occupancy_provider_tests `
  slicer_cli rip_reader_test

ctest --test-dir build-slicesoft/main -C Debug `
  -R "^stage16_(geometry_sampling_fixture|layer_occupancy_provider)_tests$" `
  --output-on-failure
```

另需执行 S3/S4 小型 Package 与 RIP strict，并逐文件比较 Legacy Golden TIFF SHA-256，差异必须为零。
