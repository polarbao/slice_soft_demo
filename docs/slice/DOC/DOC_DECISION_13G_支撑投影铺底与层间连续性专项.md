# DOC_DECISION_13G 支撑投影铺底与层间连续性专项

> 决策状态：IMPLEMENTED / FUNCTIONAL PASS
> 版本：v1.1
> 日期：2026-07-30
> 前置：13E / 13E-R1、12A、13F-R1-06

## 1. 决策

成立 **13G 支撑投影铺底与层间连续性专项**，但按以下顺序执行：

```text
13G-G0：先关闭甲片 Z 正反面 Gate；
13G-G1：再实现最大支撑投影铺底；
13G-G2：最后评估是否需要跨层内部支撑连续性。
```

13G 不把“逐层二维闭合空洞”等同于完整的甲片内侧支撑需求。

## 2. 为什么正反面 Gate 优先

Reality 五模型当前均为中心先接触 Z=0、两侧后出现。若直接在该姿态上扩大支撑：

```text
会为错误摆放生成稳定支撑；
会把本应由 lower bottom projection 解决的问题误归类为 internal_void；
会导致支撑材料用量、接触面和剥离方向均不可解释；
会使 30 层铺底验证建立在错误几何上。
```

因此 `frontUp=true` 是 13G 生产验收的前置条件。

## 3. 支撑铺底正式定义

新增目标配置：

```json
{
  "support": {
    "baseProjection": {
      "enabled": true,
      "layerCount": 30,
      "source": "max_support_footprint"
    }
  }
}
```

语义：

```text
先按当前 placement、internalVoid、unsupported island 和支撑形态策略生成支撑；
对最终有效支撑 mask 跨层求并集，得到 max_support_footprint；
在人类第 1..30 层，即 layerIndex 0..29，使用该 footprint 写入 S；
若同像素存在 Model 或 OuterVarnishShell，继续执行既有优先级；
第 31 层及以后不因 baseProjection 自动延续。
```

默认值：

```text
enabled=true
layerCount=30
source=max_support_footprint
```

历史 fixture 未显式配置该字段时采用兼容关闭；Qt 新建生产场景和正式生产 Profile 默认开启。

## 4. internalVoid 决策

现有规则保留为：

```text
support.internalVoid.fillRule=all_internal_voids
effectiveAlgorithm=layer_enclosed_2d
```

13G 不立即把它改成向上无限延续。正确姿态复测后：

```text
若 lower projection 已形成连续内侧承托，则不新增跨层规则；
若仍有真实承托断层，再设计 engine-neutral cavity envelope；
禁止通过文件名、固定层号或无边界膨胀修补。
```

## 5. SupportType 与协议

新增内部/报告类型候选：

```text
SupportType::ProjectionBase
report name=projection_base
```

它仍只写 S 通道，不新增 TIFF 通道，不改变：

```text
schema=p0.rgbwsv.2
channelOrder=R G B W S V
bitDepth=8
polarity=black_is_print
printValue=0
emptyValue=255
```

## 6. 阶段边界

13G 包含：

```text
Reality 五模型正反面与层序证据；
最大支撑投影铺底配置、core、report、UI 和测试；
铺底层数 0..N 的边界验证；
正确姿态后的支撑连续性复测。
```

13G 不包含：

```text
RIP 半色调或设备喷墨顺序；
支撑材料配方；
OpenVDB 默认化；
复杂树状支撑；
按模型文件名特判；
通过铺底替代模型修复或正确自动定向。
```
