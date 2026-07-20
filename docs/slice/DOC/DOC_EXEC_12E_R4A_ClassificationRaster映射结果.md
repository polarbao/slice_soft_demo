# DOC_EXEC_12E-R4A Classification-to-Raster 映射结果

> 文档状态：12E-08A COMPLETE / DIAGNOSTIC ONLY
> 日期：2026-07-17
> 前置任务：12E-01 至 12E-07 COMPLETE
> 后续任务：12E-08B 完整材料语义 sidecar 与 full closure

## 1. 任务结论

12E-08A 已建立三维 classification grid 到最终尺寸 raster grid 的确定性映射合同、DTO、算法、
generated fixture 和报告字段。该结果只提供诊断证据，不写 TIFF、manifest 或 package，也不授予
`global_surface_shell` production role。

```text
mappingMethod = world_space_cell_containment
raster sample = pixel/layer cell center in world space
source owner = containing classification cell
outside source extent = Empty
texture/fill transfer = exact binary ownership copy
texture RGB = copied only when TextureSurfaceMask=1
productionOutputWritten = false
productionAcceptance = not_evaluated
```

## 2. 新增实现

```text
src/slicer_core/raster/TextureFillPartitionRasterMapper.h
src/slicer_core/raster/TextureFillPartitionRasterMapper.cpp
tests/unit/texture_fill_partition_raster_mapper/main.cpp
tests/golden/expected/12e_texture_fill_partition_raster_mapping.json
```

报告入口扩展：

```text
TextureFillPartitionReport.rasterMapping
TextureFillPartitionReport.performance.rasterMappingMs
```

## 3. 映射合同

目标 raster 每个采样点使用真实世界坐标：

```text
x = originXMm + (pixelX + 0.5) * pixelPitchXMm
y = originYMm + (pixelY + 0.5) * pixelPitchYMm
z = originZMm + (layerIndex + 0.5) * layerThicknessMm
```

classification cell 归属使用半开区间：

```text
[sourceOrigin + i * sourceSpacing,
 sourceOrigin + (i + 1) * sourceSpacing)
```

位于 classification grid 外的 raster sample 保持 Empty。实现禁止使用 PNG resize、双线性插值
或跨层兜底，因此不会把预览缩放结果冒充生产几何映射。

## 4. 不变量

输入 Gate：

```text
partition.available=true；
partition.partitionPass=true；
model/texture/fill mask 尺寸一致且为二值；
TextureSurface ∩ ModelFill = Empty；
TextureSurface ∪ ModelFill = Model；
texture RGB 只存在于 TextureSurface；
allTexture=true 时 ModelFill 必须为空。
```

输出 Gate：

```text
overlapRasterVoxels=0；
unassignedModelRasterVoxels=0；
modelRasterVoxels = textureSurfaceRasterVoxels + modelFillRasterVoxels；
真实 layerIndex/zMm 单调递增；
生产写出始终关闭。
```

## 5. 诊断指标

```text
rasterVoxelCount；
mappedSourceGridVoxels / outsideSourceGridVoxels；
uniqueSourceVoxelCount / reusedSourceVoxelCount；
model/texture/fill/textureRgb raster voxel counts；
sourceModelCoverage / rasterModelCoverage / modelCoverageDelta；
maxCenterQuantizationErrorMm；
mappingMs；
partitionPass。
```

`modelCoverageDelta` 是分类网格和目标 raster 全域占用率之差，只用于识别采样范围或量化变化，
不是 production acceptance 的单独判据。

## 6. 稳定错误码

```text
E_12E_RASTER_MAPPING_INPUT_INVALID
E_12E_RASTER_MAPPING_GRID_INVALID
E_12E_RASTER_MAPPING_PARTITION_INVALID
E_12E_RASTER_MAPPING_TRANSFER_INVALID
E_12E_RASTER_MAPPING_INVARIANT_FAILED
```

所有失败均发生在任何 production writer 之前。

## 7. 测试覆盖

generated fixture 覆盖：

```text
classification/raster 同分辨率精确保持；
更细 raster 的确定性 cell ownership；
更粗 raster 的边界 tie 确定性；
目标范围超出 source 时保持 Empty；
allTexture 终点 fill=0；
纹理 RGB 不泄漏到 fill；
非法 raster grid；
非法 source partition；
非法 texture transfer；
allTexture 与 fill 非空的矛盾状态；
空输入与禁止生产写出。
```

验证结果：

```text
default OpenVDB OFF：raster mapper + report 2/2 PASS；
OpenVDB ON：raster mapper + report 2/2 PASS；
default OpenVDB OFF 全量 Debug build PASS，CTest 18/18 PASS；
Repair Disabled RIP strict 与 30 层 TIFF SHA-256 invariant PASS。
```

全量验证结果以 `REPORT_12E_启动准备状态.md` 最新记录为准。

## 8. 未完成边界

12E-08A 不解决：

```text
support/internalVoid/surfaceVarnish/outerVarnish 完整 sidecar；
最终 Model > OuterVarnishShell > Support > Empty 冲突裁决；
12D full closure；
Release 真实模型性能和峰值内存准入；
production package 与 RIP strict；
Qt Profile、设置和 preview。
```

因此 12E-08 总任务仍为 `IN_PROGRESS / PRODUCTION NOT ADMITTED`。
