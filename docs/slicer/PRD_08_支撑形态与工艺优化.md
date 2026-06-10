# PRD_08_支撑形态与工艺优化

> 文档版本：v0.1  
> 阶段：08  
> 建议目录：`docs/slicer/`

## 1. 背景

当前支撑已具备 bottom_projection、unsupported_only、bottom_plus_unsupported、island_filter、SupportType diagnostics、support_report 与 preview overlay。

但真实美甲 / 浮雕 / 纹理模型仍可能出现：

```text
支撑碎片
小支撑岛
狭缝割裂
侧边小组件
支撑与模型边界视觉断裂
局部支撑不可制造或不稳定
```

08 需要把这些现象从“人工观察”升级为“可配置、可诊断、可回归”的支撑形态策略。

## 2. 产品目标

让支撑 mask 更适合真实工艺验证，并能通过 report / preview / golden test 自动判断支撑形态是否稳定。

## 3. 必须支持功能

### 3.1 SupportShapePolicy

新增或扩展配置：

```json
{
  "support": {
    "shape": {
      "enabled": true,
      "minComponentAreaPx": 16,
      "xyDilationPx": 1,
      "closingRadiusPx": 1,
      "bridgeGapPx": 2,
      "preserveModelPriority": true,
      "maxAddedSupportRatio": 0.25
    }
  }
}
```

### 3.2 支撑组件分析

统计：

```text
componentCount
largestComponentArea
smallComponentCount
tinyComponentCount
components[].areaPx
components[].bbox
```

### 3.3 小岛过滤

支持 `filterSmallComponents` / `minComponentAreaPx`。

### 3.4 支撑桥接

第一版只做水平/垂直短 gap bridge：

```text
bridgeGapPx
onlyBridgeEmptyPixels
doNotOverwriteModel
```

### 3.5 支撑形态 report

新增：

```text
reports/support_shape_report.json
```

## 4. 验收标准

```text
支撑形态配置可解析
小组件过滤可生效
bridgeGapPx 可连接小间隙
不覆盖 model pixels
Model > Support > Empty 优先级不变
support_shape_report.json 输出
preview RGB+S 可观察变化
golden summary 比较 support component 指标
schema tests 验证 report schema
run_ci_quick.ps1 通过
p0.rgbwsv.2 输出协议不变
```
