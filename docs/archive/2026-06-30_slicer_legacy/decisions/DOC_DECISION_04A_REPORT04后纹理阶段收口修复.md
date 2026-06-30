# DOC_DECISION_04A_REPORT04后纹理阶段收口修复

> 文档版本：v0.1  
> 文档状态：Decision / 阶段修正  
> 适用阶段：REPORT_04 之后  
> 建议提交目录：`docs/slicer/`  
> 主题：04 彩色纹理基础链路完成后，先做 04A 收口修复，再进入 05 材料策略

---

## 1. 阶段判断

根据 `REPORT_04_彩色纹理切片当前实现状态.md`，04 阶段已经完成基础 RGB 纹理数据链路：

```text
OBJ + MTL + Texture
→ vt / material 解析
→ relief top surface UV 采样
→ RGB 通道写入
→ RGBWSV TIFF package
→ preview / reports / rip_reader_test
```

主样例 `TexturedReliefRgb` 已通过，说明真实纹理采样到 RGB 输出的主链路已经打通。

---

## 2. 为什么不建议立刻进入 05

04 报告同时暴露两个收口问题。

### 2.1 fallback fixture 失效

当前：

```text
TexturedMissingTextureFallback
TexturedNoUvFallback
```

两个 fallback 用例中的模型文件已经被替换为 38MB 真实纹理模型级别数据，不再是轻量测试 fixture。

结果：

```text
missing texture fallback 没有真正证明 missing texture 行为
no UV fallback 没有真正证明 no-UV fallback 行为
两个大模型 fallback 包的 rip_reader_test 超过 120 秒未完成
```

这说明 04 的 fallback 验收并未真正闭环。

### 2.2 第 68 层支撑局部割裂

`TexturedReliefRgb` 第 68 层出现支撑 connected components 分裂。

报告判断该问题不是 RGB 覆盖支撑，也不是 S 通道缺失，而是：

```text
模型右侧低层侧壁 / 边缘模型像素按 Model > Support 优先级覆盖同位置支撑，
导致主支撑与右侧小支撑岛视觉割裂。
```

这是支撑形态业务问题，不是协议错误。

---

## 3. 决策

当前不直接进入 05 材料策略。

应新增轻量收口阶段：

```text
04A：彩色纹理基础阶段收口修复
```

04A 的目标：

```text
1. 重建小型 missing texture fixture
2. 重建小型 no-UV fixture
3. 让 fallback 用例可以快速回归
4. 将 fallback 行为写入 regression
5. 对第 68 层支撑割裂做 report 级诊断
6. 明确支撑形态优化是否进入 05 或后续 08
```

04A 不是新功能大阶段，而是 04 的验收补洞。

---

## 4. 04A 与 05 的关系

04A 完成后再进入：

```text
05：材料策略与白墨 / 光油控制
```

05 应处理：

```text
RGB + W
RGB + V
white underbase
varnish top layer
top_n_layers
surface_shell
texture-driven varnish / white 的阶段边界
```

但这些必须建立在 04 的 RGB texture 与 fallback 回归可靠的基础上。

---

## 5. 04A 非目标

04A 不做：

```text
完整材料策略
局部光油
纹理驱动白墨
纹理驱动光油
color_shell_volume
闭合模型完整外壳纹理投影
OpenVDB
Qt UI
RIP 半色调
ICC / CMYK
```

---

## 6. 冻结项

04A 仍必须保持：

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

---

## 7. 结论

04 阶段主链路已完成，但 fallback 验证与支撑局部割裂诊断尚未完全收口。

因此下一步建议：

```text
先做 04A 收口修复
再进入 05 材料策略
```
