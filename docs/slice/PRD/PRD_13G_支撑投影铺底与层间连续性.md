# PRD_13G 支撑投影铺底与层间连续性

> 文档状态：PRD / IMPLEMENTED
> 版本：v1.2
> 日期：2026-07-30

## 1. 背景

真实甲片打印需要模型内侧支撑从平台稳定生长，同时需要在支撑底部使用最大支撑投影
形成连续铺底。当前实现已有 lower projection 和逐层二维 internal void，但缺少独立的
支撑铺底产品能力，也没有把甲片正反面错误与支撑算法错误分开。

## 2. 产品目标

```text
1. 所有甲片在支撑计算前必须通过正面朝 +Z Gate；
2. 支撑底部可按最大支撑投影新增固定层数的 S 材料物理铺底；
3. 默认铺底 30 层，用户可在 UI 和配置中调整；
4. 内部支撑在正确姿态下保持可解释、连续，不出现无原因断层；
5. 每层可区分 lower、internal_void 和 projection_base 支撑来源；
6. 彩色纹理与单材料模型使用同一支撑策略。
```

## 3. 用户故事

### US-13G-01 正确姿态后生成支撑

作为操作员，我希望甲片尖端朝场景 +Y、外表面朝 +Z、最低点落在 Z=0 后再生成支撑，
避免为反向模型生成错误支撑。

验收：

```text
Reality 五模型低层不再表现为中心最低后向两侧扩展；
报告包含有效旋转、frontUp 判定和 minZ；
frontUp 无法确定时预检告警，不静默声称通过。
```

### US-13G-02 最大投影铺底

作为工艺人员，我希望支撑的最大 XY 投影在最初若干层形成完整 S 材料底座，使上方支撑
具有连续基础。

验收：

```text
默认 layerCount=30；
新增 layerIndex 0..29 作为模型下方的独立铺底层；
原模型整体上移 30 层，输出 TIFF 总层数增加 30；
历史配置可继续选择只覆盖既有层、不增加总层数的兼容语义；
铺底只写 S；
Model > OuterVarnishShell > Support > Empty 保持不变。
```

### US-13G-03 UI 可配置

作为调试人员，我希望在切片设置的“支撑”区域启停铺底并设置层数，而不手工编辑 JSON。

验收：

```text
控件名称为“支撑投影铺底”和“铺底层数”；
层数范围 0..1000；
0 等价于关闭；
帮助文本明确“第 1..N 层”和“不改变模型材料”。
```

### US-13G-04 支撑连续性诊断

作为开发和工艺人员，我希望报告能解释支撑消失是因为模型占位、二维开口、铺底结束，
还是配置关闭。

验收：

```text
每层统计 projectionBaseSupportPixels；
统计 internalVoidSupportPixels；
记录 baseProjectionEffectiveLayerRange；
对 support 断层给出 stable reason code。
```

## 4. 配置要求

```json
{
  "support": {
    "baseProjection": {
      "enabled": true,
      "layerCount": 30,
      "layerPlacement": "prepend_below_model",
      "source": "max_support_footprint"
    }
  }
}
```

校验：

```text
enabled 必须为 bool；
layerCount 必须为 0..1000 整数；
source 当前只接受 max_support_footprint；
layerPlacement 接受 prepend_below_model 或 overlay_existing；
enabled=true 且 layerCount=0 时 effectiveEnabled=false，并输出提示；
生产 UI 默认 true/30/prepend_below_model；
历史 fixture 缺省保持兼容关闭，已显式启用但未写 layerPlacement 时保持 overlay_existing。
```

## 5. 业务规则

```text
1. 先完成模型定向和落台，再采样 model mask；
2. 先生成普通支撑并完成支撑形态策略；
3. 对最终普通支撑求跨层最大投影；
4. prepend_below_model 在模型下方预留 N 个物理层并把模型整体抬高 N * layerThickness；
5. 将最大投影写入新增的前 N 层；
6. overlay_existing 仅作为历史兼容模式，在既有前 N 层叠加且不增加层数；
7. 不覆盖模型和优先级更高的光油壳层；
8. 铺底结束不等于内部支撑结束；
9. internalVoid 仍只解释逐层二维闭合空洞，直到后续 Gate 明确扩展。
```

## 6. 非目标

```text
不改变 TIFF/RIP 协议；
不引入新材料通道；
不保证任意非甲片模型的语义正面；
不把 30 层写死在切片器；
不使用全画布支撑；
不在本阶段引入 OpenVDB 支撑体。
```

## 7. 完成标准

```text
Reality 五模型 front-up Gate 有确定证据；
配置、core、report、UI、fixture 和真实模型矩阵通过；
默认生产场景使用 30 层铺底；
启用 prepend_below_model 后输出总层数精确增加 N；
旧 fixture 未显式启用时 TIFF 不变；
RIP strict PASS；
阶段报告披露 internalVoid 仍为二维规则的边界。
```
