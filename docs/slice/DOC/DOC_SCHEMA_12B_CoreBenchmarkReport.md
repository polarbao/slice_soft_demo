# DOC_SCHEMA_12B Core Benchmark Report

> 文档状态：Schema
> 日期：2026-07-08
> Schema：`slicesoft.benchmark.12b.1`

## 1. 目标

固定 12B core-only benchmark 报告字段，避免不同脚本或不同阶段输出不可比较数据。

该报告只描述 benchmark，不替代 production package report。

## 2. Report 根结构

```json
{
  "schema": "slicesoft.benchmark.12b.1",
  "generatedAt": "2026-07-08T00:00:00+08:00",
  "caseName": "nai_you_new_same_pose",
  "buildType": "Release",
  "samePose": true,
  "samePoseReason": "same_model_transform_and_auto_orient",
  "sameResolution": true,
  "sameResolutionReason": "same_dpi_and_layer_thickness",
  "sameSemanticsRequested": true,
  "outputPolicy": {},
  "environment": {},
  "inputs": {},
  "engines": [],
  "comparison": {},
  "decision": {}
}
```

## 3. outputPolicy

```json
{
  "writeTiff": false,
  "writePreview": false,
  "writeReports": "benchmark_stdout_only",
  "publishPackage": false,
  "noImageWrite": true
}
```

规则：

```text
coreComputeMs 不包含 TIFF、preview、manifest、report 写盘时间；
若需要 end-to-end benchmark，必须另开 report，不得混入 core-only 结论。
```

## 4. environment

```json
{
  "os": "Windows",
  "compiler": "MSVC",
  "cpu": "unknown",
  "buildDir": "build",
  "openVdbEnabled": false,
  "vcpkgRoot": "from environment when applicable"
}
```

说明：

```text
R0 可以先记录 available 字段；
CPU 详细信息后续可补充，但不能阻塞 R0。
```

## 5. inputs

```json
{
  "modelPath": "model/obj/nai_you_new/xxx.obj",
  "legacyConfig": "output/12b/configs/nai_you_legacy.json",
  "openVdbConfig": "output/12b/configs/nai_you_openvdb.json",
  "layerThicknessMm": 0.01,
  "dpiX": 600,
  "dpiY": 600,
  "transform": {
    "scale": [0.8, 0.8, 0.8],
    "rotationDeg": [0, 0, 0],
    "translationMm": [0, 0, 0],
    "autoOrientApplied": true
  }
}
```

## 5.1 samePose / sameResolution

规则：

```text
samePose=true 需要 legacy/openvdb 使用同一个模型路径、同 scale、同 rotationDeg、同 translationMm、同 autoOrient 开关。
sameResolution=true 需要 dpiX、dpiY、layerThicknessMm 完全一致。
单引擎 benchmark 可输出 true，reason=single_engine_benchmark。
双引擎 benchmark 若任一字段为 false，必须输出 reason，并且 performanceComparable=false。
```

常见 reason：

```text
single_engine_benchmark
same_model_transform_and_auto_orient
same_dpi_and_layer_thickness
model_path_differs
scale_differs
rotation_differs
translation_differs
auto_orient_differs
dpi_x_differs
dpi_y_differs
layer_thickness_differs
config_unavailable
```

## 6. engines[]

每个引擎一条记录：

```json
{
  "engine": "legacy",
  "available": true,
  "configPath": "output/12b/configs/nai_you_legacy.json",
  "buildType": "Release",
  "grid": {
    "widthPx": 283,
    "heightPx": 531,
    "layerCount": 717
  },
  "stats": {
    "modelPixels": 13781916,
    "supportPixels": 36902358,
    "rgbPrintPixels": 0,
    "whitePrintPixels": 0,
    "varnishPrintPixels": 0,
    "shellPixels": 0
  },
  "timingsMs": {
    "import": null,
    "coreCompute": 49.716,
    "materialCompose": null,
    "ioWrite": 0,
    "previewWrite": 0,
    "reportWrite": 0,
    "endToEnd": 49.716
  },
  "profile": {
    "available": true,
    "profileLevel": "coarse",
    "configLoadMs": 0.05,
    "modelLoadMs": 100.0,
    "gridSetupMs": 1.0,
    "maskSamplingMs": 3000.0,
    "texturePrepareMs": 100.0,
    "supportGenerationMs": 500.0,
    "layerComposeMs": 1000.0,
    "reportBuildMs": 200.0,
    "reportWriteMs": 0.0,
    "totalMs": 4801.05,
    "notes": []
  },
  "memory": {
    "processPeakWorkingSetAvailable": true,
    "processPeakWorkingSetBytes": 123456789
  },
  "replacementGate": {
    "outputSemanticsComparable": true,
    "performanceComparable": true,
    "replacementPass": false,
    "failureReasons": []
  }
}
```

### 6.1 profile

`profile` 是 12B-R1 引入的诊断字段，不属于生产 RGBWSV package 协议。

规则：

```text
profile.available=true 表示当前引擎输出了内部粗粒度计时；
profileLevel=coarse 表示只拆到稳定的大阶段，不伪造 geometry/material/support 细节；
reportWriteMs 在 core-only benchmark 下应为 0 或接近 0；
字段缺失或 unavailable 时，不影响旧报告读取，但不能用于热点判断。
```

当前 coarse profile 字段：

```text
configLoadMs：run_slicer 内部配置加载耗时；
modelLoadMs：模型/材质/纹理元数据加载耗时；
gridSetupMs：grid、输出目录和 TIFF spec 准备耗时；
maskSamplingMs：model mask / relief mask 采样耗时；
texturePrepareMs：纹理运行时、纹理列和材料角色列准备耗时；
supportGenerationMs：支撑、光油壳层、surface varnish mask 和 support shape 生成耗时；
layerComposeMs：逐层 RGBWSV compose 和可选 TIFF/preview 写入耗时；
reportBuildMs：内存中的 report/manifest JSON 构建耗时；
reportWriteMs：report/manifest 写盘耗时；
totalMs：run_slicer 内部总耗时。
```

## 7. comparison

```json
{
  "legacyCoreComputeMs": 49.716,
  "openVdbCoreComputeMs": 1038.711,
  "openVdbToLegacyCoreRatio": 20.893,
  "outputSemanticsComparable": false,
  "performanceComparable": false,
  "replacementPass": false,
  "failureReasons": [
    "openvdb_output_semantics_not_comparable"
  ]
}
```

规则：

```text
outputSemanticsComparable=false 时，不能声明 OpenVDB 比 legacy 快或慢到可替代；
可记录耗时事实，但 replacementPass 必须 false。
```

## 8. decision

```json
{
  "recommendedProductionEngine": "legacy",
  "openVdbRole": "sdf_utility_candidate",
  "nextStage": "12B-R1 legacy/heightfield optimization",
  "notes": [
    "OpenVDB candidate remains experimental"
  ]
}
```

## 9. 兼容 11B 报告

11B 已存在：

```text
schema = p0.openvdb_legacy_core_benchmark.1
```

12B 脚本可读取 11B 单引擎 stdout JSON，但最终聚合报告必须输出：

```text
schema = slicesoft.benchmark.12b.1
```
