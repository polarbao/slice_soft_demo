# DOC_DECISION_04_REPORT03后进入彩色纹理基础阶段

> 文档版本：v0.1  
> 文档状态：Decision  
> 建议提交目录：`docs/slicer/`

## 1. 阶段判断

`REPORT_03_RGBWSV协议固化当前实现状态.md` 显示 03 阶段已经完成协议固化、reader 严格校验、错误码、bad package 负向测试、统计字段和回归脚本。

03 阶段没有新增切片能力，而是把当前 RGBWSV 数据包固定为后续阶段可依赖的数据契约。

因此当前建议进入：

```text
04：彩色纹理模型切片基础版
```

## 2. 为什么可以进入 04

进入 04 的前置条件已经满足：

```text
1. RGBWSV / uint8 / black_is_print 协议稳定
2. manifest schema = p0.rgbwsv.1
3. rip_reader_test 可严格拒绝错误 package
4. Support / Relief / Bad package 已被 run_regression.ps1 覆盖
5. 当前已有 RGB 通道与 preview 基础
```

## 3. 04 阶段定位

04 是彩色纹理基础版：

```text
OBJ + MTL + Texture
→ UV 采样
→ RGB 通道真实颜色输出
→ RGBWSV TIFF package
```

不是：

```text
ICC / 色彩管理
CMYK
RIP 半色调
3MF
OpenVDB
Qt UI
完整光油覆盖策略
```

## 4. 04 首选实现对象

第一版优先支持：

```text
relief_heightfield + top surface texture sampling
```

原因：

```text
当前业务高频模型是美甲 / 浮雕模型；
relief_heightfield 已具备 column / top hit 的可扩展基础；
先验证 RGB 纹理数据链路比直接做完整闭合模型 color shell 更稳。
```

## 5. 冻结项

04 不得改变：

```text
schema = p0.rgbwsv.1
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF 通道
```

## 6. 结论

03 完成后，不需要继续修改 03 主功能。下一阶段应进入 04 彩色纹理基础版。
