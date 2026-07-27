# DEV_13C TIFF Layer Source 与统一材料合成设计

> 文档版本：v0.1
> 文档状态：Formal DEV / PREPARED
> 生成日期：2026-07-24

## 1. 当前代码事实

当前已有可复用能力：

```text
slicer_core::read_rgbwsv_tiff 支持 stripped/tiled 六通道 uint8；
LayerPreviewPanel::RenderProductionRgb 已直接读取 TIFF；
LayerPreviewPanel 像素探针已读取六通道；
PreviewWorkspace 已统一 layerIndex 和三个预览入口；
PreviewPhysicalScale 已处理 dpiX/dpiY。
```

当前缺口：

```text
W/S/V 显示仍主要读取 preview PNG；
PreviewOverlayPanel 仍按 preview 文件合成；
同一 TIFF 在 RGB 和像素探针中可能重复解码；
没有统一 TIFF layer cache；
没有 RGB+S+W+V；
生产预览与诊断 preview 的数据边界仍不够直观。
```

## 2. 目标架构

```text
PackageLoader
  -> ProductionLayerIndex
  -> TiffLayerSource
  -> TiffLayerCache
  -> MaterialPreviewComposer
  -> UnifiedLayerPreviewWidget

DiagnosticReport/MaskSource
  -> DiagnosticPreviewComposer
  -> UnifiedLayerPreviewWidget
```

生产和诊断共享 `layerIndex/zMm/physicalScale`，但不共享虚构的数据来源。

## 3. 核心 DTO

```text
ProductionLayerRef：
  layerIndex/zMm/path/width/height/storage/checksum；

RgbwsvLayerBuffer：
  layerIndex/width/height/pixels/channelStats；

MaterialPreviewRequest：
  mode/enabledChannels/pseudoColors/alpha/showEmpty；

MaterialPreviewResult：
  image/sourceIdentity/layerIndex/renderStats/warnings。
```

Public core API 使用 STL/domain DTO，Qt 转换只发生在 UI service。

## 4. TIFF Layer Source

`TiffLayerSource` 负责：

```text
只接受 manifest 列出的 TIFF；
调用 read_rgbwsv_tiff；
验证 dimensions/samples/bit depth；
保留真实 layerIndex；
返回错误码，不吞异常；
支持取消和 session identity；
不做伪彩和业务材料判断。
```

UI Worker 异步加载。结果绑定：

```text
package path；
manifest hash；
layerIndex；
TIFF file size/mtime 或 checksum；
request generation。
```

旧请求完成时若 generation 已变化，结果必须丢弃。

## 5. LRU Cache

首版建议缓存当前层、相邻两层和最近访问层：

```text
默认 5 层；
默认内存上限 256 MiB；
可按内存预算缩减；
key=packageIdentity+layerIndex+checksum；
缓存原始六通道 buffer，不缓存每种组合图片；
组合图片可做小型二级 cache；
切换 package 时全部失效。
```

单层超过内存上限时允许服务当前请求，但不得进入 cache。完整 source/cache/stale/error 合同见
`DOC_PREP_13C_01_TIFFLayerSource与Cache准备.md`。

不得一次性读取几百层 TIFF。

## 6. 合成算法

### 6.1 打印判断

```text
hasR = R < 255；
hasG = G < 255；
hasB = B < 255；
hasRgb = hasR || hasG || hasB；
hasW = W < 255；
hasS = S < 255；
hasV = V < 255；
isEmpty = all six channels == 255。
```

### 6.2 单通道

R/G/B 使用相应通道色或灰度强度；W/S/V 使用配置伪彩。空白始终使用 Empty 显示色。

### 6.3 RGB

生产 RGB 显示沿用当前项目已经验证的 RGB 解释，禁止把 `black_is_print` 简单等同为整张图反相。
纹理真彩若依赖额外语义数据，应作为显示策略显式标识。

### 6.4 RGB+S+W+V

建议采用：

```text
RGB 为底图；
W/S/V 使用独立可配置 alpha；
默认绘制顺序 RGB -> W -> S -> V；
图例显示叠加顺序；
像素探针显示全部通道，避免颜色覆盖造成误判；
可增加“仅轮廓”模式，用于重叠通道检查。
```

显示顺序不等于生产材料优先级。

## 7. UI 收口

建议用一个 `UnifiedLayerPreviewWidget` 替代生产层和材料叠加的重复控制：

```text
层滑块；
上一层/下一层；
通道复选或预设；
RGB+S+W+V 预设；
图例；
缩放/适应/1:1；
像素探针；
加载状态和错误。
```

`RawPreview` 改为“诊断”页，仅在存在 report/mask 时启用。现有 `PreviewWorkspaceMode` 可收敛为：

```text
Production；
Diagnostic。
```

迁移过程先让旧 Panel 通过 adapter 使用新 `TiffLayerSource`，验证后再删除重复合成代码，遵守
`wrap first, move later, rewrite last`。

## 8. 输出策略

生产配置建议新增显示/诊断分离：

```json
{
  "preview": {
    "productionImages": false,
    "diagnosticImages": false,
    "exportOnDemand": true
  }
}
```

兼容旧 `preview.enabled`：

```text
迁移期读取旧字段；
旧字段只控制诊断 preview；
生产 TIFF 永远由 output/writeTiffLayers 控制；
关闭 PNG 不得关闭 TIFF。
```

正式字段名需在实现任务中结合现有 config migration 冻结，本文示例不代表当前已实现 schema。

## 9. 错误处理

```text
TIFF 缺失：显示具体 layer/path；
TIFF 协议错误：显示 RIP/Reader 等价错误码；
当前层加载失败：不跨层兜底；
诊断 mask 缺失：显示未提供；
缓存过期：重新加载；
用户快速滑层：取消或丢弃旧 generation；
UI 关闭：Worker 安全退出。
```

## 10. 性能指标

记录：

```text
单层 TIFF decode ms；
单通道 compose ms；
RGB+S+W+V compose ms；
cache hit/miss；
峰值 cache bytes；
首层可见时间；
快速滑动丢弃请求数；
切片 preview PNG write ms before/after。
```

目标不是声称 TIFF 解码零成本，而是消除生产流程中重复生成和读取多个材料 PNG。

## 11. 测试

```text
stripped/tiled TIFF；
黑即打印和真实空白；
R/G/B/W/S/V 单通道；
RGB+S+W+V；
同像素多通道；
layerIndex/zMm；
600/600、635/600；
缓存失效；
取消/快速滑层；
坏 TIFF/缺层；
无 preview 目录；
旧 package 兼容；
UI smoke 和 RIP strict 回归。
```
