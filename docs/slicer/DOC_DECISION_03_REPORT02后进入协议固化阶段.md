# DOC_DECISION_03_REPORT02后进入协议固化阶段

> 文档版本：v0.1  
> 文档状态：Decision / 执行决策  
> 适用阶段：REPORT_02 之后  
> 建议提交目录：`docs/slicer/`  
> 主题：02 支撑与孤岛检测完成后，进入 03 RGBWSV 协议固化阶段

---

## 1. 阶段判断

根据 `REPORT_02_支撑与孤岛检测当前实现状态.md`，02 阶段已经达到阶段完成条件：

```text
support.mode 已支持：
  bottom_projection
  unsupported_only
  bottom_projection_plus_unsupported
  full_vertical_projection

已实现：
  connected component island detection
  previous_model OR previous_support 承托判断
  minOverlapRatio
  minIslandAreaPx
  xyDilationPx
  supportTypeStats
  support_report / slice_report 扩展
  support 样例与回归
  rip_reader_test pass
```

因此当前不建议继续扩展 02 的复杂支撑能力。

---

## 2. 为什么不继续深挖 02

02 阶段仍有未实现项：

```text
project_to_nearest_supported_layer
debug preview: island_mask / unsupported_mask / support_type
复杂支撑树
支撑可拆结构
支撑密度渐变
支撑力学仿真
```

这些属于增强项，不应阻塞进入 03。

其中：

```text
project_to_nearest_supported_layer
debug preview
```

可以后续进入 `02A` 或 UI/调试增强阶段。

复杂支撑树、支撑密度、可拆结构不属于当前 Demo/协议固化阶段。

---

## 3. 当前应进入 03

03 阶段目标：

```text
RGBWSV TIFF / manifest / RIP Reader 输入协议固化与负向测试
```

03 不改变当前生产协议，而是将现有事实协议固化：

```text
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
SamplesPerPixel = 6
PlanarConfig = contiguous
tiled = true
tile padding = 255
```

---

## 4. 03 阶段不要做什么

03 不做：

```text
RIP 半色调
CMYK 分色
喷头 bitstream
ICC 色彩管理
墨量曲线
彩色纹理
3MF
OpenVDB
Qt UI
复杂支撑树
```

03 只做：

```text
协议字段
schema
writer/reader 校验
错误码
负向测试
回归脚本
统计字段标准化
```

---

## 5. 进入 03 前的最小冻结项

在 Codex 执行 03 前，应将 02 当前状态视为冻结基线：

```text
02 baseline:
  bottom_projection pass
  unsupported_only pass
  bottom_projection_plus_unsupported pass
  support_island_filter pass
  relief 回归 pass
```

03 的所有改动不得破坏这些 baseline。

---

## 6. 结论

下一阶段应继续执行：

```text
PRD_03 / DEV_03 / TASKS_03：RGBWSV 协议固化与负向测试
```

不需要再回头重写 02。

只需在 03 回归脚本中纳入 02 的 support 样例，确保协议固化后不破坏支撑输出。
