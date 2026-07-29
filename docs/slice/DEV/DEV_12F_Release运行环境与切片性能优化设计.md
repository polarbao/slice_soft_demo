# DEV_12F Release 运行环境与切片性能优化设计

> 文档状态：DEV / R0 Implemented / R0-R1 Build Revision Implemented / R1-R5 Planned
> 日期：2026-07-16；构建轨道修订：2026-07-29

## 1. R0 Runtime 架构

```text
PrepareSliceSoftRuntime.ps1
  -> Resolve Qt5_DIR
  -> Import VsDevCmd x64 environment
  -> CMake Visual Studio multi-config configure
  -> MSBuild 8-job incremental build of slicer_cli/rip_reader_test/slicer_debug_ui
  -> copy samples/model/Profile documents
  -> validate portable scenario/config/model paths
  -> windeployqt into staging
  -> write runtime_manifest.json
  -> atomic directory publish
```

目录：

```text
build-slicesoft/
  main/
    Debug/
    Release/

runtime/slicesoft/
  Debug/
    slicer_debug_ui.exe
    slicer_cli.exe
    rip_reader_test.exe
    Qt5*Cored.dll / plugins
  Release/
    slicer_debug_ui.exe
    slicer_cli.exe
    rip_reader_test.exe
    Qt5*.dll / plugins
    samples/
    model/
    docs/slice/PRD/
    runtime_manifest.json
```

`build-*` 和 `runtime/` 均为本地生成目录，不提交二进制。

运行包中的 UI 检测到 `applicationDir/samples/scenarios/slicer_scenarios.json` 时，以 `applicationDir` 作为资源根目录；开发构建目录不存在该索引时继续使用当前工作目录。该规则避免快捷方式的“起始位置”影响场景/Profile 加载。

Runtime 发布守门逐个检查 30 个场景的 `configPath`、`input.modelPath`、`output.packageDir` 和可选 `docPath`。模型及输出路径不得保留开发机绝对路径；模型必须能在配置目录或运行包根目录下解析。

## 2. ToolPaths 解析

Windows 按以下优先级解析：

```text
1. applicationDir/slicer_cli.exe；
2. build-slicesoft/main/<Debug|Release>/slicer_cli.exe；
3. build-slicesoft/<Debug|Release>/slicer_cli.exe（历史 NMake 兼容）；
4. build/<Debug|Release>/slicer_cli.exe；
5. 若均不存在，返回统一 build 预期路径用于错误提示。
```

构建类型通过 `_DEBUG` 决定，禁止 Release UI 自动回退到 Debug CLI。

OpenVDB candidate 只查找同构建类型的：

```text
build-openvdb-09p/<Config>/slicer_cli.exe
```

默认 Runtime 不部署 OpenVDB DLL 或 candidate CLI。

## 3. Visual Studio 主轨道与 NMake 兜底

2026-07-16 的 NMake 选择是对当时 `HostX86/x64 cl.exe` 构建超时的稳定性规避，
不是用 NMake 替代 CMake。CMake 始终负责配置和生成，NMake/MSBuild 只是不同的
底层构建工具。

2026-07-29 默认轨道改为：

```text
CMakePresets.json
  -> Visual Studio 18 2026 / x64
  -> build-slicesoft/main
  -> Debug / Release / RelWithDebInfo
  -> jobs = 8
```

`slicer_core` 使用 target-based `/MP`，MSBuild 同时提供 project-level 和
translation-unit-level 并行。NMake 保留在 `build-slicesoft-nmake`，仅当 Visual
Studio 主轨道不可用时手动运行；两个生成器绝不复用同一个 CMake cache。

## 4. R1 Benchmark 刷新设计

复用 `slicesoft.benchmark.12b.1`，至少重新测量：

```text
nai_you_new；
aishen_fudiao；
meigui_fudiao。
```

要求：

```text
Release；
writeTiff=false；
writePreview=false；
same pose/resolution/semantics；
每 case 预热 1 次、正式 5 次、取中位数；
记录 peak working set；
记录 git revision 和 dirty flag，但不把 dirty 文件内容写入报告。
```

## 5. R2 支撑生成优化设计

当前热点来源：

```text
完整 support_masks 和 support_type_maps 分配；
按 XY 列向下逐层写入；
内部镂空逐层分析；
生成结束后再次扫描全 volume 统计。
```

候选按风险排序：

```text
A. 生成时同步统计，移除第二遍全量统计扫描；
B. bottom projection 使用 ColumnSupportRange 中间表示；
C. compose 时按层展开支撑，不提前物化全部 mask；
D. internal void layer signature/cache；
E. column/tile 并行。
```

先执行 A，再根据 profile 决定 B/C。每一步必须保持 support type 和逐层统计一致。

## 6. R3 Layer Compose 优化设计

候选：

```text
compose 与 channel stats 合并为一次扫描；
每层 RGBWSV buffer 复用；
固定策略在进入像素循环前解析；
减少 Json 临时对象和重复 layer diagnostics 转换；
确认无层间依赖后再考虑并行。
```

Gate：逐层 channel hash、总像素统计和 RIP strict 结果不变。

## 7. R4 Relief 稠密 Mask 优化设计

当前已经有 `ColumnLayerRange`，但仍完整物化：

```text
model_masks[layer][pixel]
```

候选设计：

```text
ReliefOccupancyProvider
  IsModel(layer, pixel)
  LowerLayer(pixel)
  UpperLayer(pixel)
  MaterializeLayer(layer, scratchBuffer)
```

先以 adapter 包装当前数据，随后让 support/compose 使用 provider。只有 mask diff=0、support diff=0、channel hash 一致后，才允许关闭 relief 的完整 model mask 常驻。

## 8. R5 缓存和并行

缓存 key：

```text
model content hash；
format/import options；
transform/autoOrient；
DPI/layer thickness；
slicing mode；
support geometry settings。
```

材料颜色或 preview 设置变化时，可复用 geometry/heightfield；支撑配置不变时可复用 support intermediate。缓存必须有版本、内存上限和显式失效原因。

并行只在单线程优化后评估，避免把内存带宽问题放大。

## 9. 验证矩阵

```text
Runtime：Debug/Release UI self-test + CLI --help + manifest/schema；
Portable Profile：从非运行包工作目录执行 scenario-registry，并对稳定 Profile 执行 inspect-model；
R1：Release core-only 3 models x 5 median；
R2：support stats/hash/golden/RIP；
R3：RGBWSV layer hash + report totals；
R4：mask/support/channel diff；
R5：cold/warm cache 和单/多线程确定性。
```
