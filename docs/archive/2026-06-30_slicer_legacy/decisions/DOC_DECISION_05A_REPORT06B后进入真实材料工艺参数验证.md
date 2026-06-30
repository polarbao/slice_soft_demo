# DOC_DECISION_05A_REPORT06B后进入真实材料工艺参数验证

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_06B 之后  
> 建议提交目录：`docs/slicer/`

## 1. 阶段判断

根据 `REPORT_06B_3MF纹理与ColorGroup当前实现状态.md`，06B 已完成 3MF 彩色输入最小闭环：

```text
ColorGroup 基础颜色解析
Texture2D / Texture2DGroup 基础贴图解析
triangle pid / p1 / p2 / p3 到颜色或 UV 的解析
3MF 包内 PNG 贴图提取并复用 TextureSampler
three_mf_report.json 与 texture_report.json 增强
正向样例、bad package 负向用例和 quick regression 校验
```

因此，06B 主功能可以收口。

## 2. 下一阶段建议

建议进入：

```text
05A：真实材料工艺参数验证
```

原因：

```text
当前已经具备 OBJ/MTL/Texture、3MF basematerial、3MF ColorGroup、3MF Texture2DGroup、MaterialRoleMapping、MaterialPolicy、Support、p0.rgbwsv.2 的完整主链路。
继续扩展 06C 会强化更多 3MF 规范边界，但不能立即回答真实生产工艺问题。
05A 可以验证白墨 underbase、光油 top_n_layers、RGB+W+V 组合 profile、通道覆盖率和层分布。
```

## 3. 05A 阶段定位

05A 是材料工艺参数验证阶段，不是新输入格式阶段。

目标：

```text
在真实或准真实模型上，验证 RGB / W / V / S 的组合输出是否符合打印工艺预期，
并形成可复用、可比较、可回归的 MaterialProcessProfile。
```

## 4. 冻结项

05A 必须保持：

```text
schema = p0.rgbwsv.2
storageMode = stripped / tiled
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
Model > Support > Empty
SupportType 不进入 TIFF 通道
MaterialRoleMapping 语义不变
MaterialPolicy RGB/W/V overlay 语义不变
S support 仍由 Support pipeline 独立生成
```

## 5. 05A 不做

```text
ICC / CMYK
RIP 半色调
真实喷头 bitstream
设备通信
OpenVDB / SDF
Qt UI
3MF CompositeMaterials 完整语义
PBR
支撑形态大改
```

## 6. 05A 后续路线

05A 完成后建议优先进入：

```text
07：Qt 调试 UI
```

原因是材料 profile 完成后，最需要 UI 来查看、对比、调参和诊断。
