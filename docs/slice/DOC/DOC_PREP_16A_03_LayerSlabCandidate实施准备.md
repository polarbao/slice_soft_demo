# DOC_PREP_16A-03 Layer Slab Candidate 实施准备

> 状态：**COMPLETE / IMPLEMENTED**
> 日期：2026-08-12
> 任务：`16A-03`

## 1. 准入结论

`16A-02` 已冻结 STL-only Policy/Provider，并证明 Legacy 默认输出零漂移。本卡只实现
`LayerSlabCoverage + PixelCenter`，且只允许既有单区间 `relief_heightfield` 输入。

## 2. 冻结语义

对层 `i` 和高度区间 `[minimumZMm, maximumZMm)`：

```text
slabLow  = i * layerThicknessMm
slabHigh = (i + 1) * layerThicknessMm
occupied = maximumZMm > slabLow && minimumZMm < slabHigh
```

只计正测度相交；零厚度区间不占据任何层。恰好位于层边界的数值使用固定
`1e-9 mm` 吸附容差，不按模型或网格尺寸缩放。

## 3. 配置与准入

```text
缺省或 geometrySampling.strategy=legacy_center_sample：保持 Legacy；
geometrySampling.strategy=layer_slab_pixel_center_candidate：显式候选；
显式候选只允许 slicingMode=relief_heightfield；
通用 mesh、多区间列和 2x2 supersample 均 fail-closed。
```

## 4. 实施边界

```text
复用 16A-02 Provider，不引入 Qt/Writer/材料依赖；
Legacy 继续执行原生产循环；
候选只替换模型 occupancy mask 和列首末占用层；
不修改 RGBWSV、8-bit、black_is_print、支撑和材料优先级；
不实现 16A-04 的边界 2x2；不切换生产默认。
```

## 5. 验证计划

```powershell
cmake --build build-slicesoft/main --config Debug --target stage16_layer_occupancy_provider_tests slicer_cli rip_reader_test
ctest --test-dir build-slicesoft/main -C Debug -R "^stage16_(geometry_sampling_fixture|layer_occupancy_provider)_tests$" --output-on-failure
build-slicesoft/main/Debug/slicer_cli.exe --config samples/configs/stage16/layer_slab_pixel_center_candidate.json
build-slicesoft/main/Debug/rip_reader_test.exe --package output/Stage16LayerSlabPixelCenterCandidate --summary
```

另以任务前快照逐文件比较 Legacy Golden TIFF 的 SHA-256，要求差异为零。
