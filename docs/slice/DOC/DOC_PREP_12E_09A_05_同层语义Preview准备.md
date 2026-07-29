# DOC PREP 12E-09A-05 同层语义 Preview 准备

> 文档状态：READY FOR DEVELOPMENT
> 日期：2026-07-29
> 前置：12E-09A-01..04、13C-01..05、13D-01..04 COMPLETE
> 后续：12E-09A-06 WAIT 09A-05

## 1. 任务目标

在 13C 已完成的 TIFF 原生生产预览上，叠加 09A 异步分析生成的 Texture Surface 与
Model Fill 诊断语义，并与生产 TIFF 中的 Support、Varnish 保持同一真实
`layerIndex/zMm`。本任务只新增只读诊断显示，不写回生产 TIFF、manifest、report 或 package。

## 2. 当前事实

```text
LayerPreviewPanel 已按 manifest 中真实 layerIndex 异步加载 RGBWSV TIFF；
PreviewWorkspace 已使用生产 TIFF 层列表作为共享低 Z -> 高 Z 层序；
DiagnosticAnalysisWorker 已返回 immutable identity 和内存 benchmark evidence；
benchmark evidence 已包含完整三维 Texture Surface / Model Fill 分区；
旧 PreviewOverlayPanel/PreviewPanel 仍消费 preview PNG，只作为迁移期诊断来源；
ProductionLayerRef 尚未把 grid.originMm、pixelSizeMm 和 layerThicknessMm 传入层 buffer。
```

因此 09A-05 不能按图像宽高直接缩放诊断 mask，也不能按 preview PNG 序号配层；必须先把
manifest 的物理网格元数据传到 TIFF layer buffer，再按像素中心世界坐标采样诊断分区。

## 3. 固定数据源

### 3.1 生产材料

```text
RGB / W / S / V：只来自当前 manifest 列出的当前层 RGBWSV TIFF；
layerIndex / zMm：只来自同一个 ProductionLayerRef；
grid origin / pixel pitch / layer thickness：只来自当前 manifest.grid；
不得从 preview PNG、文件名序号或相邻层补齐。
```

### 3.2 诊断语义

```text
Texture Surface / Model Fill：只来自当前 DiagnosticAnalysisResult.evidence.partition；
width / coverage / allTexture：只来自同一 result；
identity：sessionId、sceneId、modelId、instanceId、sceneRevision、
          transformRevision、configHash；
结果状态不是 Succeeded、partitionPass=false 或证据为空时显示“未评估”。
```

生产 TIFF 不能反推出 Texture Surface / Model Fill 的业务归属。诊断 mask 也不能替代生产
S/V 通道。

## 4. 同层与物理映射合同

生产像素中心：

```text
worldX = productionOriginX + (pixelX + 0.5) * pixelSizeX
worldY = productionOriginY + (pixelY + 0.5) * pixelSizeY
worldZ = layer.zMm
```

诊断体素：

```text
voxelX = floor((worldX - diagnosticOriginX) / diagnosticSpacingX)
voxelY = floor((worldY - diagnosticOriginY) / diagnosticSpacingY)
voxelZ = floor((worldZ - diagnosticOriginZ) / diagnosticSpacingZ)
```

固定规则：

```text
只读取上述 worldZ 所在诊断体素，不寻找最近的其他生产层；
支持 635x600 等非方形 DPI，不允许假设 X/Y pitch 相同；
超出诊断网格的像素保持无诊断语义；
空生产层仍显示真实生产 TIFF，Texture/Fill 统计可为 0；
物理元数据缺失、非有限或非正时 fail-closed。
```

## 5. 身份与陈旧结果

```text
场景 package 存在 scene summary 时，sceneId/sceneRevision 必须与诊断结果一致；
单模型 package 缺少 scene summary 时，只显示“生产包未提供场景身份”，不得声明同源；
场景、当前实例、变换 revision、参数或 configHash 变化时清除旧诊断叠加；
生产 package 切换时清除旧 TIFF buffer，并重新做身份判断；
陈旧或身份不匹配结果不得自动显示。
```

单模型 package 可继续显示生产 TIFF；只是不把缺失身份的 Texture/Fill 叠加解释为已验证的
生产同源关系。

## 6. UI 合同

在“诊断预览”下增加“纹理/填充同层语义”来源，并至少提供：

```text
Texture Surface；
Model Fill；
分区 + S + V。
```

显示要求：

```text
状态栏显示真实 layerIndex、zMm、width、coverage、allTexture 和诊断 identity 摘要；
Texture、Fill、Support、Varnish 使用仅用于显示的不同伪彩；
真实空白保持独立背景色；
缺少 fullClosure linkage 时显示“材料闭环联动未评估”；
不得把显示伪彩当作 0=print 的生产值；
不得新增重复 TIFF/PNG 写盘。
```

生产预览和像素探针继续由 13C `LayerPreviewPanel` 提供，09A-05 不复制 TIFF 读取链。

## 7. 文件所有权

预计修改：

```text
src/slicer_core/preview/ProductionLayerRef.h；
src/slicer_core/preview/TiffLayerSource.*；
src/slicer_core/preview/TextureFillPartitionSemanticPreview.*；
apps/slicer_debug_ui/widgets/LayerPreviewPanel.*；
apps/slicer_debug_ui/widgets/DiagnosticSemanticPreviewPanel.*；
apps/slicer_debug_ui/widgets/PreviewWorkspace.*；
apps/slicer_debug_ui/MainWindow.*；
apps/slicer_debug_ui/services/UiSmokeTestRunner.cpp；
CMakeLists.txt / apps/slicer_debug_ui/CMakeLists.txt；
定向 unit tests；
09A 状态、索引和用户说明。
```

## 8. 验收矩阵

```text
09A-P01：635x600 TIFF 与诊断网格按世界坐标映射；
09A-P02：Texture/Fill XOR 且只在模型域内；
09A-P03：S/V 只读取当前层生产 TIFF；
09A-P04：非连续 layerIndex 不跨层兜底；
09A-P05：空层稳定显示，统计为 0；
09A-P06：缺 evidence、失败、取消、stale 和 identity mismatch 明确未评估；
09A-P07：场景 package 的 sceneId/revision 匹配；
09A-P08：真实模型/真实 package smoke；
09A-P09：不生成新的 TIFF/PNG/package；
09A-P10：生产 RGBWSV 像素探针、协议和默认 Legacy 路线保持不变。
```

## 9. 验证计划

```powershell
cmake --build build --config Debug --target `
  texture_fill_partition_semantic_preview_unit_tests `
  tiff_layer_source_unit_tests `
  diagnostic_analysis_worker_unit_tests `
  slicer_debug_ui

ctest --test-dir build -C Debug `
  -R "^(texture_fill_partition_semantic_preview_unit_tests|tiff_layer_source_unit_tests|diagnostic_analysis_worker_unit_tests)$" `
  --output-on-failure

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe `
  --ui-smoke-test --case diagnostic-semantic-preview

.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

以上命令是计划门禁；只有本任务实际运行通过后才能写入状态报告。

## 10. 安全边界

```text
p0.rgbwsv.2 不变；
R G B W S V 不变；
uint8 / black_is_print 不变；
Legacy 仍为默认；
OpenVDB 默认关闭；
诊断结果不等于 production admission；
09A-05 不写生产输出；
不按旧 preview PNG 序号跨层兜底。
```
