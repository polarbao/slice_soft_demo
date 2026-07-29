# REPORT 12E-09A-05 同层语义 Preview 当前状态

> 状态：COMPLETE / 12E-09A-06 READY
> 日期：2026-07-29

## 1. 完成内容

本任务已在 13C TIFF 原生生产预览上接入 Texture Surface 与 Model Fill 诊断语义。诊断结果、
Support 和 Varnish 现在使用同一个真实 `layerIndex/zMm`，不再通过 preview 文件序号、相邻层或
图像尺寸缩放建立对应关系。

新增诊断预览模式：

```text
分区 + S + V；
Texture Surface；
Model Fill。
```

显示使用独立伪彩，真实生产值仍由生产预览和六通道像素探针读取。诊断视图不会写 TIFF、PNG、
manifest、report 或 package。

## 2. 物理映射

`ProductionLayerRef`、`ProductionPackageIndex` 和 `RgbwsvLayerBuffer` 已保留：

```text
originMm；
pixelSizeMm；
layerThicknessMm；
sceneId / sceneRevision（manifest 提供时）。
```

`TextureFillPartitionSemanticPreview` 按生产像素中心的世界坐标采样诊断体素，独立使用 X/Y
pixel pitch，已覆盖 635x600 等非方形 DPI。生产层位于诊断 Z 网格外时，Texture/Fill 明确为 0，
但同层 TIFF 的 S/V 仍可显示，不进行跨层兜底。

## 3. 身份与失效

场景 package 提供 scene identity 时，必须与诊断结果的 `sceneId/sceneRevision` 一致；不一致结果
直接标记为不可用。场景、实例、变换、纹理宽度、填充材料或新诊断请求发生变化时，UI 会清除旧
诊断语义。

单模型旧 package 未提供 scene identity 时仍可查看生产 TIFF 和诊断结果，但状态明确显示
“生产包未提供场景身份”，不得解释为生产同源验证。材料闭环报告尚未与本视图绑定时显示
“材料闭环联动未评估”。

## 4. 协议边界

本任务未改变：

```text
manifest schema = p0.rgbwsv.2；
通道顺序 R G B W S V；
uint8 / black_is_print；
0=打印、255=不打印；
Legacy 默认生产路线；
OpenVDB 默认关闭；
diagnostic 不等于 production admission。
```

## 5. 验收证据

实际通过：

```text
Debug build：
  slicer_debug_ui
  texture_fill_partition_semantic_preview_unit_tests
  tiff_layer_source_unit_tests
  diagnostic_analysis_worker_unit_tests

CTest：
  3/3 PASS

UI Smoke：
  PASS preview-workspace-shared-layer ... primaryModes=2 diagnosticModes=3
  PASS diagnostic-semantic-preview layer=37 Texture=1 Fill=1 S=1 V=1 stale=blocked
  PASS tiff-native-preview-all-materials layers=25 ... source=TIFF
  PASS preview-legend-probe-context legend=RGBWSV probes=RGB,W,S,V,Empty
  PASS preview-physical-aspect corrected=94x100 fallback=100x100

真实生产包 TIFF Smoke：
  output/ui_sessions/surface_shell_cube_custom_scene_legacy_20260729_121251_654/package
  PASS tiff-native-preview-all-materials layers=50 modes=13 requests=6 source=TIFF

UI self-test：
  PASS startup
  PASS experimental-report-summary

git diff --check：
  PASS（只有仓库既有 LF/CRLF 提示，无空白错误）
```

覆盖结论：

```text
09A-P01..P07：单元测试和 UI smoke 已覆盖；
09A-P08：真实生产 package 的 TIFF 原生加载、模式切换和真实层序已通过；
09A-P09：实现没有新增生产输出写入路径；
09A-P10：RGBWSV 探针、协议和 Legacy 默认路线保持不变。
```

## 6. 剩余风险

当前真实包 smoke 验证了生产 TIFF 数据源；Texture/Fill 诊断分区与真实复杂纹理模型的人工视觉
检查仍应在 09A-06 阶段收口矩阵中执行。此项不影响 09A-05 的数据源、物理映射和身份合同通过。

## 7. 下一任务

`12E-09A-06` 的准备文档和执行指令已经存在，09A-05 前置现已解除。下一步只做诊断 UI 阶段
回归、窗口尺寸、长中文、失败/取消/重复运行、真实模型人工检查、用户手册和状态索引收口，不再
扩展生产切片语义。
