# DOC_PREP_16A-02 GeometryOccupancyPolicy 与 Provider 合同实施准备

> 状态：**COMPLETE / IMPLEMENTED**
> 日期：2026-08-12
> 任务：`16A-02`

## 1. 准入

`16A-01` 已冻结合成采样 fixture 和工程差异 schema。本卡只建立 STL-only 占用策略与 Provider 包装，并让既有 `relief_heightfield` 路径显式选择 Legacy 策略；不实现候选采样语义。

## 2. 冻结合同

```text
LayerOccupancyMode：LegacyCenterSample / LayerSlabCoverage；
XyCoverageMode：PixelCenter / Supersample2x2；
GeometryOccupancyPolicy：layerMode / xyMode / minimumCoveredSubsamples；
GeometryOccupancyColumn：occupied / minimumZMm / maximumZMm；
LayerOccupancyProvider：输入列范围和策略，输出逐层 mask 与首末占用层。
```

默认合同固定为 `LegacyCenterSample + PixelCenter + 1`。`LayerSlabCoverage` 和 `Supersample2x2` 在本卡中必须 fail-closed，分别留给 `16A-03` 和 `16A-04`。

## 3. 接入边界

```text
公共核心合同只依赖 C++ STL；
不依赖 Qt、JSON、TIFF、RGBWSV 或材料类型；
既有 relief 生产循环继续生成原有 mask，不用 Provider 重算，避免字节漂移；
生产路径在进入既有 Legacy 循环前显式构造并校验 Legacy policy；
Provider 的独立 materialize 能力由定向单元测试覆盖，供后续候选卡复用。
```

## 4. 验证

```text
cmake --build build-slicesoft/main --config Debug --target stage16_layer_occupancy_provider_tests slicer_cli
ctest --test-dir build-slicesoft/main -C Debug -R "^stage16_(geometry_sampling_fixture|layer_occupancy_provider)_tests$" --output-on-failure
```

结果：构建 PASS；Stage 16 定向 CTest 2/2 PASS；`material_process_top2_fixture` 既有 25 个 TIFF layer 与任务前快照逐文件 SHA-256 对比，差异 0。
