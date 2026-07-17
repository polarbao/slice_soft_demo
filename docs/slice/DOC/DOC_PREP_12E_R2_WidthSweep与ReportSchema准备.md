# DOC_PREP_12E-R2 Width Sweep 与 Report Schema 准备

> 文档状态：PREPARED / 12E-05 READY FOR USER ADMISSION
> 日期：2026-07-17
> 前置任务：12E-01、12E-02、12E-03、12E-04 COMPLETE
> 覆盖任务：12E-05 Width Sweep 与 Report Schema

## 1. 准备结论

12E-04 已提供默认 OFF 的稳定 unavailable 行为、OpenVDB ON 同 request grid candidate、
CPU/OpenVDB 差异 DTO、严格拓扑门禁和 OFF/ON conformance fixture。12E-05 可以冻结
`slicesoft.texture_fill_partition.12e.1` 的成功结果序列化，并建立宽度扫描与单调性验证。

12E-05 仍为 diagnostic-only。它只能生成内存 JSON 或测试目录中的诊断报告，不得把报告
加入 production manifest，不得写 RGBWSV TIFF，不得接入 Qt 或 composer。

## 2. 当前代码事实

已具备：

```text
GlobalTextureFillPartitionService：统一 grid、mask 和分区不变量；
LegacyCpuGlobalDistanceBackend：默认 OFF 可运行的 CPU candidate；
OpenVdbTextureFillConformanceBackend：可选 ON conformance candidate；
TextureFillPartitionConformanceResult：同 grid、mask 差异、距离差异、阈值和性能比；
TextureFillPartitionReportData：unavailable report DTO；
BuildTextureFillPartitionReportSkeleton：slicesoft.texture_fill_partition.12e.1 骨架；
TextureFillPartitionWidthMetrics：requested 之外的动态宽度证据；
TextureFillPartitionStats/Performance：分区、耗时和内存证据。
```

明确缺口：

```text
没有从 GlobalTextureFillPartitionResult 生成成功报告的 serializer；
没有 per-layer partition 统计；
没有 width sweep request/result DTO；
没有单调性 validator 和稳定失败 issue；
没有 min/intermediate/allTexture golden；
现有 report skeleton 的 performance 字段未覆盖 levelSet/gridSample/OpenVDB grid bytes；
conformance DTO 尚未序列化。
```

## 3. 固定 Schema

Schema 保持：

```text
slicesoft.texture_fill_partition.12e.1
```

根对象至少包括：

```text
schema；
packageProtocol；
enabled / strategy；
availability / status / productionAcceptance；
backend / backendRole；
grid；
width；
partition；
performance；
conformance（可选，只有双 backend 比较时存在）；
layers；
issues；
configSnapshot。
```

12E-05 不修改 `p0.rgbwsv.2`，`packageProtocol` 只是报告关联字段。

## 4. 成功报告序列化

建议新增接口：

```cpp
Json BuildTextureFillPartitionReport(
    const SliceConfig& config,
    const GlobalTextureFillPartitionResult& result,
    const TextureFillPartitionConformanceResult* conformance = nullptr);
```

序列化约束：

```text
available candidate -> availability=available；
partitionPass=true -> status=diagnostic，不自动写 pass；
productionAcceptance 固定 not_evaluated；
动态 width 值必须来自 result，不得根据 config 猜测；
所有 uint64 计数保持整数；
不可用测量值使用 null，不用 0 冒充；
issues 保留稳定 code；
OpenVDB 内部类型不得进入 JSON 或公共 DTO。
```

## 5. Per-layer 统计

三维 mask 按 request grid 的 Z 维拆层：

```text
layerIndex = z；
zMm = originZMm + (z + 0.5) * spacingZMm；
modelVoxels；
textureSurfaceVoxels；
modelFillVoxels；
overlapTextureFillVoxels；
unassignedModelVoxels；
partitionPass。
```

12E-05 中字段仍使用 `Voxels`。只有进入最终打印 raster grid 后才允许另行输出 `Pixels`；
不得把三维分类 voxel 统计伪装成生产 TIFF 像素统计。

## 6. Width Sweep 契约

建议 DTO：

```text
TextureFillPartitionWidthSweepSample：requestedWidthMm + result summary；
TextureFillPartitionWidthSweepResult：backend、samples、monotonic、endpoint、issues；
TextureFillPartitionWidthSweepOptions：中间采样数量和最大样本数。
```

扫描边界：

```text
minimum = 第一次有效 candidate 的 effectiveMinimumWidthMm；
maximum = 第一次有效 candidate 的 allTextureThresholdMm；
步长固定 0.01 mm；
至少包含 minimum、一个可用 intermediate（若区间允许）和 maximum；
maximum 必须精确作为最后一个 requestedWidthMm；
不得用 modelFill.enabled=false 构造 allTexture endpoint。
```

为了避免大模型产生无界样本，默认只取代表点：

```text
minimum；
25%；
50%；
75%；
allTexture threshold；
```

每个代表点按 0.01 mm 量化并去重。完整逐步扫描只能由显式测试选项启用。

## 7. 单调性 Validator

按 requestedWidthMm 升序检查：

```text
modelVoxels 不变；
textureSurfaceVoxels 非递减；
modelFillVoxels 非递增；
overlapTextureFillVoxels = 0；
unassignedModelVoxels = 0；
textureSurfaceVoxels + modelFillVoxels = modelVoxels；
最后一个 sample allTexture=true；
最后一个 sample textureSurfaceVoxels=modelVoxels；
最后一个 sample modelFillVoxels=0。
```

建议稳定 issue：

```text
E_12E_WIDTH_SWEEP_EMPTY
E_12E_WIDTH_SWEEP_SAMPLE_FAILED
E_12E_WIDTH_SWEEP_MODEL_CHANGED
E_12E_WIDTH_SWEEP_TEXTURE_NON_MONOTONIC
E_12E_WIDTH_SWEEP_FILL_NON_MONOTONIC
E_12E_WIDTH_SWEEP_ENDPOINT_INVALID
```

## 8. Backend 行为

默认 OFF：

```text
CPU width sweep 和成功报告必须可独立运行；
OpenVDB conformance report 为 unavailable；
不得要求 OpenVDB DLL 或 vcpkg。
```

OpenVDB ON：

```text
CPU 与 OpenVDB 分别执行同一组量化宽度；
每个 backend 独立验证单调性；
可输出同宽度 conformance 差异；
差异只标记 diagnostic，不冻结生产容差。
```

## 9. Fixture 与 Golden

最低矩阵：

| Fixture | 断言 |
|---|---|
| closed box | minimum/intermediate/maximum 单调，末端 fill=0 |
| sloped body | 不依赖逐层方向，最近表面距离驱动单调变化 |
| thin wall | minimum 即可能 allTexture，去重后允许单样本 |
| closed cavity | cavity 始终不计入 model，外壳单调 |
| unavailable backend | report unavailable、sweep 不伪造 sample |
| invalid/blocked sample | stable issue、后续样本不冒充完整 sweep |
| repeat | 样本宽度、计数和 JSON golden 确定 |

Golden 建议：

```text
tests/golden/expected/12e_texture_fill_partition_report_schema.json；
tests/golden/expected/12e_width_sweep_summary.json。
```

## 10. 性能与证据

成功报告至少序列化：

```text
topologyMs；
levelSetMs；
gridSampleMs；
occupancyMs；
distanceMs；
partitionMs；
totalCoreMs；
maskBytes；
closestReferenceBytes；
openVdbGridBytes；
workingSetBytes；
peakWorkingSetBytes。
```

width sweep 总耗时必须与单次 candidate core timing 分开，不得混入 TIFF/PNG/JSON 写盘时间。

## 11. 文件范围

允许修改：

```text
src/slicer_core/materials/texture_application/TextureFillPartitionTypes.*；
src/slicer_core/materials/texture_application/GlobalTextureFillPartitionService.*；
src/slicer_core/reports/TextureFillPartitionReport.*；
tests/unit/texture_fill_partition_report/*；
tests/unit/texture_fill_partition_width_sweep/*；
tests/golden/expected/12e_*；
CMakeLists.txt；
12E schema/matrix/task/report 文档。
```

禁止修改：

```text
slicer.cpp production generation；
MaterialChannelComposer / TIFF writer；
Qt UI；
12D repair；
OpenVDB 默认值和 vcpkg 依赖策略；
p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print。
```

## 12. 验证计划

默认 OFF：

```powershell
cmake --build build --config Debug --target texture_fill_partition_width_sweep_unit_tests texture_fill_partition_report_unit_tests
.\build\Debug\texture_fill_partition_width_sweep_unit_tests.exe
.\build\Debug\texture_fill_partition_report_unit_tests.exe
ctest --test-dir build -C Debug -R "texture_fill_partition_(width_sweep|report)|legacy_cpu_global_distance" --output-on-failure
```

OpenVDB ON：

```powershell
cmake --build build-openvdb-09p --config Debug --target texture_fill_partition_width_sweep_unit_tests texture_fill_partition_report_unit_tests
ctest --test-dir build-openvdb-09p -C Debug -R "texture_fill_partition_(width_sweep|report)|openvdb_texture_fill_conformance" --output-on-failure
```

共同守门：

```powershell
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

## 13. Gate 结论

```text
12E-01：COMPLETE；
12E-02：COMPLETE；
12E-03：COMPLETE；
12E-04：COMPLETE；
12E-05：PREPARED / READY FOR USER ADMISSION；
12E production：NOT ADMITTED。
```

12E-05 完成后才能准备 12E-06 Texture Transfer 与 Diagnostic Composer。准备完成不自动
启动 12E-05，也不代表 OpenVDB 获得 production role。
