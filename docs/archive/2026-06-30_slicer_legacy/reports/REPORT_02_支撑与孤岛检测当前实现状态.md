# REPORT_02_支撑与孤岛检测当前实现状态

> 文档版本：v0.1  
> 文档状态：Current Implementation Snapshot  
> 适用阶段：PRD_02 / DEV_02 / DEMO_02  
> 更新日期：2026-06-05  

## 1. 阶段定位

阶段 02 已按 v0.2 文档执行，目标是：

```text
支撑生成、孤岛检测与 SupportType 元数据扩展
```

本阶段不是：

```text
复杂支撑树
支撑可拆结构
支撑力学优化
彩色纹理
OpenVDB
Qt UI
```

## 2. 冻结协议

当前仍保持：

```text
RGBWSV
uint8
0 = 打印
255 = 不打印
polarity = black_is_print
channelOrder = R G B W S V
Model > Support > Empty
```

`SupportType` 只进入 report / metadata，不新增 TIFF 通道。

## 3. 已实现 Support Modes

当前 `support.mode` 支持：

```text
bottom_projection
unsupported_only
bottom_projection_plus_unsupported
full_vertical_projection
```

其中：

```text
bottom_projection:
  使用下表面 / first model layer 生成平台投影支撑。

unsupported_only:
  按 layer-to-layer overlap 检测 island，只生成 unsupported island 支撑。

bottom_projection_plus_unsupported:
  先生成 bottom_projection，再检测未承托 island 并补充支撑。

full_vertical_projection:
  debug / 保守模式，不作为业务默认。
```

## 4. 已实现配置字段

`support` 当前支持：

```text
enabled
mode
value
offsetMm
minAreaPx
minOverlapRatio
minIslandAreaPx
connectivity
unsupportedProjection
xyDilationPx
writeSupportTypeDebug
```

当前 `unsupportedProjection` 已实现：

```text
project_to_build_plate
```

暂未实现：

```text
project_to_nearest_supported_layer
```

该值目前会被配置校验拒绝，避免误以为已经可用。

## 5. Island Detection

当前实现：

```text
4 / 8 邻域 connected component
previous_model OR previous_support 承托基础
xyDilationPx 可选扩张
overlapRatio = overlapPixels / componentPixels
overlapRatio < minOverlapRatio 判定为 island
componentPixels < minIslandAreaPx 判定为 filtered island
```

生成策略：

```text
project_to_build_plate:
  对 island footprint 向下投影到平台
  不覆盖 model mask
  support type = unsupported_island
```

## 6. SupportType

当前内部支持：

```text
none
bottom_projection
unsupported_island
full_vertical_projection
```

合并优先级：

```text
unsupported_island > full_vertical_projection > bottom_projection > none
```

生产 TIFF 中仍只体现：

```text
S = 0
```

## 7. Reports

### 7.1 support_report.json

新增 / 保留字段：

```text
enabled
mode
supportMode
value
minOverlapRatio
minIslandAreaPx
connectivity
unsupportedProjection
xyDilationPx
slicingMode
supportSource
modelPriority
supportPixels
supportPrintPixels
columnsWithSupport
islandCount
islandPixels
unsupportedPixels
filteredIslandCount
filteredIslandPixels
layersWithIslands
layersWithSupport
totals
supportTypeStats
layers
```

### 7.2 slice_report.json

`totals` 与每层 `layers` 已增加：

```text
islandCount
islandPixels
unsupportedPixels
filteredIslandCount
filteredIslandPixels
supportTypeStats
```

旧字段继续保留：

```text
supportPixels
supportPrintPixels
modelPrintPixels
rgbPrintPixels
whitePrintPixels
varnishPrintPixels
```

## 8. 新增样例

新增模型：

```text
samples/models/support/floating_island.stl
```

该模型包含：

```text
平台上的底座块
同 XY 上方断续出现的悬空块
独立 XY 的悬空块
```

用于同时验证：

```text
bottom_projection
unsupported island
bottom_projection_plus_unsupported
filtered island
```

新增配置：

```text
samples/configs/support/support_bottom_projection.json
samples/configs/support/support_unsupported_only.json
samples/configs/support/support_bottom_plus_unsupported.json
samples/configs/support/support_island_filter.json
```

## 9. 验证结果

构建已通过：

```powershell
cmake --build build --config Debug
```

### 9.1 Support 样例

```text
SupportBottomProjection
  mode = bottom_projection
  supportPixels = 6912
  islandCount = 0
  unsupportedPixels = 0
  supportTypeStats.bottom_projection = 6912
  supportTypeStats.unsupported_island = 0
  rip_reader_test = pass

SupportUnsupportedOnly
  mode = unsupported_only
  supportPixels = 12672
  islandCount = 2
  islandPixels = 1152
  unsupportedPixels = 1152
  supportTypeStats.bottom_projection = 0
  supportTypeStats.unsupported_island = 12672
  rip_reader_test = pass

SupportBottomPlusUnsupported
  mode = bottom_projection_plus_unsupported
  supportPixels = 12672
  islandCount = 1
  islandPixels = 576
  unsupportedPixels = 576
  supportTypeStats.bottom_projection = 6912
  supportTypeStats.unsupported_island = 5760
  rip_reader_test = pass

SupportIslandFilter
  mode = unsupported_only
  supportPixels = 0
  islandCount = 0
  unsupportedPixels = 0
  filteredIslandCount = 2
  filteredIslandPixels = 1152
  supportTypeStats.unsupported_island = 0
  rip_reader_test = pass
```

### 9.2 回归样例

已重新验证：

```text
samples/configs/slice_config.json
  package = output/SlicePackage
  modelPixels = 1440
  supportPixels = 2880
  rip_reader_test = pass

samples/configs/relief/relief_nail_varnish_support.json
  package = output/ReliefNailVarnishSupport
  modelPixels = 19602925
  supportPixels = 62664673
  rip_reader_test = pass

samples/configs/relief/relief_nail_white_support.json
  package = output/ReliefNailWhiteSupport
  modelPixels = 19602925
  supportPixels = 62664673
  rip_reader_test = pass

samples/configs/relief/relief_rgb_gray.json
  package = output/ReliefRgbGray
  modelPixels = 19602925
  supportPixels = 0
  rip_reader_test = pass
```

## 10. Preview / Debug

本轮没有新增独立 debug preview 图像：

```text
island_mask
unsupported_mask
support_type
```

原因：

```text
DEMO_02 允许暂不输出图像，但必须至少输出 report 统计。
本轮已优先完成 support_report / slice_report 统计闭环。
```

现有 support preview 仍可查看 S 通道支撑。

## 11. 当前未实现能力

仍未实现：

```text
project_to_nearest_supported_layer
debug preview: island_mask / unsupported_mask / support_type
支撑树几何优化
支撑可拆除结构
支撑密度渐变
支撑力学仿真
局部光油
top_surface_only
彩色纹理
Qt UI
OpenVDB
```

## 12. 是否建议进入 PRD_03

建议进入 PRD_03，但执行时应保持：

```text
不改变 RGBWSV 通道顺序
不改变 uint8 / black_is_print / 0=打印 / 255=不打印
不把 SupportType 写入 TIFF 通道
不实现 RIP 半色调 / CMYK / 喷头数据流
```

推荐下一阶段：

```text
PRD_03 / DEV_03：RGBWSV TIFF / manifest / RIP Reader 输入协议固化与负向测试
```
