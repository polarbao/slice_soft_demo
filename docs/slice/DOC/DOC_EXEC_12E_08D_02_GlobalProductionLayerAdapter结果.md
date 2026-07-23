# DOC_EXEC_12E-08D-02 Global Production Layer Adapter 结果

> 文档状态：COMPLETE
> 日期：2026-07-23
> 下一原子任务：12E-08D-03 Shared Writer/Package/RIP

## 1. 完成范围

08D-02 只负责把已经通过 exact raster mapping 和 full-material closure 的 Global Surface Shell
结果转换为共享 writer 可消费的内存层 DTO：

```text
TextureFillPartitionRasterMappingResult
  + TextureFillPartitionFullClosureAdapterResult
  + TextureFillPartitionFullClosureLayerEvidence
  -> GlobalSurfaceShellProductionLayerAdapter
  -> RgbwsvProductionLayer + MaterialClosureSemanticLayerInput
```

本任务不调用 TIFF writer，不创建 manifest、preview、report 或 package，不修改
`RunSlicePipeline` 的 Global fail-closed 状态。

## 2. 实现结果

新增：

```text
src/slicer_core/pipeline/GlobalSurfaceShellProductionLayerAdapter.h
src/slicer_core/pipeline/GlobalSurfaceShellProductionLayerAdapter.cpp
tests/unit/global_surface_shell_production_layer_adapter/Main.cpp
```

扩展：

```text
src/slicer_core/output/rgbwsv/RgbwsvPackage.h
src/slicer_core/config/SlicePipelineConfig.h/.cpp
CMakeLists.txt
.gitignore
```

`RgbwsvProductionLayer` 固定保留：

```text
layerIndex；
zMm；
widthPx / heightPx；
channelOrder = R G B W S V；
最终 uint8 channels。
```

每个 production layer 同时携带 exact `MaterialClosureSemanticLayerInput`，包括 Texture Surface、
Model Fill、Model、Support、Internal Void Support、Surface/Outer Varnish、Model Envelope、
Support Required、Expected Occupied Domain 和 Layer Empty masks。

## 3. Fail-closed Gate

Adapter 仅在以下条件全部满足时返回 `ready_for_writer`：

```text
raster mapping available + diagnostic + partitionPass；
full closure available + exact + fullClosurePass；
未执行 material closure repair；
三份 layer list 数量、layerIndex、zMm 和尺寸一致；
所有 semantic mask 为二值且尺寸一致；
最终 RGBWSV byte 数正确；
Texture Surface RGB 与 raster mapping 完全一致；
Layer Empty mask 与最终六通道值一致；
协议保持 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print。
```

稳定错误码：

```text
E_12E_PIPELINE_GLOBAL_ADAPTER_INPUT_INVALID
E_12E_PIPELINE_GLOBAL_ADAPTER_CLOSURE_REQUIRED
E_12E_PIPELINE_GLOBAL_ADAPTER_LAYER_MISMATCH
E_12E_PIPELINE_GLOBAL_ADAPTER_PROTOCOL_MISMATCH
```

Adapter 成功不等于 production admission：

```text
status = ready_for_writer；
productionAcceptance = not_evaluated；
productionOutputWritten = false。
```

## 4. TDD 与验证

先加入测试并确认因 Adapter 头文件不存在而构建失败；实现后 5 个初始用例通过。随后新增畸形
`textureRgb` layer shape 用例，确认旧实现错误放行后补充 shape/binary gate，最终 6/6 PASS。

实际运行：

```text
cmake --build build --config Debug --target global_surface_shell_production_layer_adapter_unit_tests
build/Debug/global_surface_shell_production_layer_adapter_unit_tests.exe
ctest --test-dir build -C Debug -R "global_surface_shell_production_layer_adapter|texture_fill_partition_full_closure_adapter|texture_fill_partition_raster_mapper|texture_fill_partition_diagnostic_composer|slice_pipeline_router" --output-on-failure
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/run_ci_quick.ps1
```

结果：

```text
Adapter unit tests：6/6 PASS；
定向 CTest：5/5 PASS；
全量 CTest：47/47 PASS；
Debug 全量构建：PASS；
Quick CI：PASS，耗时约 242.4 s；
UI self-test 与 overlay-load-real：PASS。
```

首次全量构建发现此前 E 盘 I/O 故障遗留的多个生成 PDB 损坏。仅将这些 `build/` 生成文件移动到
`build/_corrupt_backup_12e08d02_20260723`，定向重建后全量构建和 Quick CI 均通过；源码和生产
数据未被删除。

## 5. 边界

```text
legacy 仍为默认生产模式；
Global 仍不写 TIFF/package；
不新增第二个 TIFF writer；
OpenVDB 仍 optional/OFF；
p0.rgbwsv.2、RGBWSV、uint8、black_is_print 未改变；
08D-03 才允许把 writer-ready layers 接入共享 writer/package/RIP 验证。
```
