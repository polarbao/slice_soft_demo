# PRD_02_支撑生成孤岛检测与SupportType扩展_v0.2

> 文档版本：v0.2  
> 文档状态：Draft / PRD 强化版  
> 适用阶段：REPORT_01 后  
> 所属模块：Slicer / Support  
> 建议提交目录：`docs/slicer/`

---

## 1. 背景

当前项目已经具备：

```text
closed_mesh_scanline 普通模型支撑
relief_heightfield 浮雕模型支撑
bottom_projection 支撑模式
S 通道支撑输出
support_report.json
slice_report.json
preview support_s
```

当前支撑适合：

```text
美甲甲片拱形下表面支撑
普通模型底部投影支撑
```

但不适合：

```text
中高层孤岛
局部悬空结构
层间承托不足
桥接结构
断续出现的模型区域
```

因此 PRD_02 需要在不破坏当前 bottom_projection 的基础上，增加孤岛检测与 unsupported 支撑能力。

---

## 2. 业务目标

PRD_02 的业务目标：

```text
识别模型中没有被下层模型或已有支撑承托的区域，并生成可追踪、可统计、可预览的 S 通道支撑。
```

这不是复杂支撑树，也不是支撑形态优化。

PRD_02 的产物仍然是：

```text
RGBWSV 六通道 TIFF
S 通道表示支撑材料
SupportType 只作为 report / preview metadata
```

---

## 3. 核心语义

### 3.1 支撑材料

支撑材料始终写入：

```text
S 通道
```

在 00B 协议下：

```text
S = 0   表示支撑打印
S = 255 表示支撑不打印
```

### 3.2 SupportType

SupportType 是支撑来源/类型的元数据。

SupportType 不新增 TIFF 通道。

SupportType 只进入：

```text
support_report.json
slice_report.json
preview metadata
可选 debug map
```

### 3.3 优先级

继续保持：

```text
Model > Support > Empty
```

如果某像素同时属于模型与支撑，则必须输出模型材料，不能输出 S 支撑。

---

## 4. 支撑模式

### 4.1 bottom_projection

已有模式。

逻辑：

```text
对每个 XY column：
  找到模型下表面层 lower_layer
  lower_layer 以下到平台之间写 S 支撑
```

适用：

```text
美甲甲片下表面支撑
普通实体底部支撑
```

---

### 4.2 unsupported_only

新增模式。

逻辑：

```text
逐层判断当前模型 component 是否被上一层模型或上一层支撑承托。
未被承托的 component 标记为 island，并生成 S 支撑。
```

适用：

```text
局部悬空
中高层孤岛
桥接结构
断续出现区域
```

---

### 4.3 bottom_projection_plus_unsupported

新增组合模式。

逻辑：

```text
先生成 bottom_projection 支撑
再对剩余层执行 unsupported island 检测
对未承托 island 补充支撑
```

这是 PRD_02 推荐业务默认增强模式。

适用：

```text
美甲甲片 + 局部浮雕/凸起/复杂悬空混合结构
```

---

### 4.4 full_vertical_projection

调试/保守模式。

逻辑：

```text
只要某 XY column 上方存在模型，则尽可能从平台向上生成连续 S 支撑。
```

注意：

```text
full_vertical_projection 可能严重过支撑。
不作为正式默认业务模式。
```

---

## 5. Island 定义

### 5.1 基本定义

在第 `L` 层中，一个模型连通区域 `component` 如果与第 `L-1` 层的承托区域重叠不足，则视为 island。

承托区域：

```text
support_base = previous_model_mask OR previous_support_mask
```

重叠率：

```text
overlapRatio = overlapPixels / componentPixels
```

如果：

```text
overlapRatio < support.minOverlapRatio
```

则该 component 视为 unsupported island。

---

### 5.2 小岛过滤

如果：

```text
componentPixels < support.minIslandAreaPx
```

该 component 可作为小噪声过滤，不生成支撑，但必须写入 report。

---

### 5.3 连通性

配置：

```text
support.connectivity = 4 / 8
```

推荐默认：

```text
8
```

---

## 6. 支撑生成策略

PRD_02 第一版支持两类投影策略：

### 6.1 project_to_build_plate

最保守策略。

```text
从 island 当前层下方一路投影到构建平台。
遇到模型区域时不覆盖模型。
```

优点：

```text
实现简单，支撑稳定。
```

缺点：

```text
可能过支撑。
```

### 6.2 project_to_nearest_supported_layer

候选增强策略。

```text
从 island 向下投影，直到遇到上一层模型或已有支撑。
```

PRD_02 可先记录配置，但实现可延后。

---

## 7. 配置需求

推荐配置：

```json
{
  "support": {
    "enabled": true,
    "mode": "bottom_projection_plus_unsupported",
    "value": 0,
    "minOverlapRatio": 0.2,
    "minIslandAreaPx": 16,
    "connectivity": 8,
    "unsupportedProjection": "project_to_build_plate",
    "xyDilationPx": 0,
    "writeSupportTypeDebug": true
  }
}
```

兼容旧配置：

```json
{
  "support": {
    "mode": "bottom_projection"
  }
}
```

---

## 8. Report 需求

### 8.1 support_report.json

必须增加：

```text
supportMode
minOverlapRatio
minIslandAreaPx
connectivity
unsupportedProjection
islandCount
islandPixels
unsupportedPixels
filteredIslandCount
filteredIslandPixels
supportTypeStats
layersWithIslands
layersWithSupport
```

### 8.2 slice_report.json

每层增加：

```text
islandCount
islandPixels
unsupportedPixels
filteredIslandPixels
supportTypeStats
```

### 8.3 supportTypeStats

示例：

```json
{
  "supportTypeStats": {
    "bottom_projection": 123456,
    "unsupported_island": 7890,
    "full_vertical_projection": 0
  }
}
```

---

## 9. Preview 需求

新增可选 preview：

```text
support_s
island_mask
unsupported_mask
support_type
```

建议颜色：

```text
support_s = green
island_mask = red
unsupported_mask = orange
support_type = debug palette
```

Preview 不影响生产 TIFF。

---

## 10. 验收标准

1. 原 `bottom_projection` 样例不受影响。
2. `relief_nail_varnish_support` 仍通过。
3. `unsupported_only` 模式可运行。
4. `bottom_projection_plus_unsupported` 模式可运行。
5. 至少一个人工 island 测试模型能检测 island。
6. `support_report.islandCount > 0`。
7. `support_report.supportTypeStats.unsupported_island > 0`。
8. `slice_report` 有逐层 island / support 统计。
9. S 通道存在支撑打印像素。
10. 支撑不覆盖模型像素。
11. `rip_reader_test` 通过。
12. RGBWSV / uint8 / black_is_print 不变。

---

## 11. 非目标

本阶段不做：

```text
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

---

## 12. 结论

PRD_02 v0.2 将支撑从“能生成底部支撑”升级为：

```text
能发现未承托区域
能补充支撑
能区分支撑来源
能报告和回归验证
```
