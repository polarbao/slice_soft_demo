# DEMO_02_支撑与孤岛检测验证方案_v0.2

> 文档版本：v0.2  
> 文档状态：Draft / DEMO 强化版  
> 适用阶段：PRD_02 / DEV_02  
> 建议提交目录：`docs/slicer/`

---

## 1. Demo 目标

验证支撑系统从单一 `bottom_projection` 扩展到：

```text
bottom_projection
unsupported_only
bottom_projection_plus_unsupported
island detection
SupportType stats
```

---

## 2. 样例目录

建议新增：

```text
samples/models/support/
samples/configs/support/
```

样例模型：

```text
floating_island.stl
bridge_test.stl
stepped_overhang.stl
```

样例配置：

```text
support_bottom_projection.json
support_unsupported_only.json
support_bottom_plus_unsupported.json
support_island_filter.json
```

---

## 3. 必须验证场景

### 3.1 bottom_projection 回归

配置：

```text
support.mode = bottom_projection
```

验收：

```text
supportPixels > 0
rip_reader_test pass
原 relief_nail_varnish_support 不受影响
```

---

### 3.2 unsupported_only

配置：

```json
{
  "support": {
    "enabled": true,
    "mode": "unsupported_only",
    "value": 0,
    "minOverlapRatio": 0.2,
    "minIslandAreaPx": 16,
    "connectivity": 8,
    "unsupportedProjection": "project_to_build_plate"
  }
}
```

验收：

```text
islandCount > 0
unsupportedPixels > 0
supportTypeStats.unsupported_island > 0
```

---

### 3.3 bottom_projection_plus_unsupported

配置：

```text
support.mode = bottom_projection_plus_unsupported
```

验收：

```text
bottom_projection 支撑仍存在
unsupported island 支撑也存在
supportTypeStats 中两类均可统计
```

---

### 3.4 小孤岛过滤

配置：

```text
minIslandAreaPx = 100
```

验收：

```text
filteredIslandCount > 0
filteredIslandPixels > 0
```

---

## 4. 验证命令

```powershell
cmake --build build --config Debug

build\Debug\slicer_cli.exe --config samples\configs\support\support_unsupported_only.json
build\Debug\rip_reader_test.exe --package output\SupportUnsupportedOnly

build\Debug\slicer_cli.exe --config samples\configs\support\support_bottom_plus_unsupported.json
build\Debug\rip_reader_test.exe --package output\SupportBottomPlusUnsupported
```

---

## 5. 回归 Checklist

- [ ] 普通 `samples/configs/slice_config.json` 通过。
- [ ] `relief_nail_varnish_support.json` 通过。
- [ ] `support_bottom_projection.json` 通过。
- [ ] `support_unsupported_only.json` 通过。
- [ ] `support_bottom_plus_unsupported.json` 通过。
- [ ] `support_island_filter.json` 通过。
- [ ] support_report 包含 island 统计。
- [ ] support_report 包含 supportTypeStats。
- [ ] slice_report 包含逐层 island 字段。
- [ ] S 通道有支撑打印像素。
- [ ] 支撑不覆盖模型。
- [ ] rip_reader_test 通过。
- [ ] RGBWSV / uint8 / black_is_print 不变。

---

## 6. 非目标

DEMO_02 不验证：

```text
支撑树
可拆支撑
支撑密度渐变
力学支撑优化
彩色纹理
Qt UI
OpenVDB
```

---

## 7. 状态报告

完成后必须生成：

```text
docs/slicer/REPORT_02_支撑与孤岛检测当前实现状态.md
```

报告应说明：

```text
已实现的 support modes
island detection 统计
supportTypeStats
通过的样例配置
未实现的复杂支撑能力
是否建议进入 PRD_03
```
