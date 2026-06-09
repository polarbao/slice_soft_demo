# DOC_DECISION_06B_REPORT06A后进入3MF纹理与ColorGroup基础支持

> 文档版本：v0.1  
> 文档状态：Decision / 阶段决策  
> 适用阶段：REPORT_06A 之后  
> 建议提交目录：`docs/slicer/`  
> 主题：06A 3MF 兼容性增强完成后，进入 06B：3MF Texture2D / ColorGroup 基础支持

---

## 1. 阶段判断

根据 `REPORT_06A_3MF兼容性增强与负向测试当前实现状态.md`，06A 已完成：

```text
1. ZIP stored entry 支持；
2. ZIP deflate entry 支持；
3. vendored miniz 集成；
4. 3MF entry count / uncompressed size / path traversal 安全限制；
5. 受限 ThreeMfXmlReader 封装；
6. 3MF reference validation；
7. bad 3MF package 负向测试；
8. three_mf_report.json 增强；
9. run_regression.ps1 -Mode quick 通过。
```

因此，06A 的工程可靠性目标可以收口。

---

## 2. 当前仍未支持能力

06A 报告中仍明确未支持：

```text
1. ZIP64；
2. encrypted ZIP；
3. external relationship resources；
4. 完整 3MF Texture2DGroup 采样；
5. 完整 ColorGroup 映射；
6. CompositeMaterials / MultiProperties 完整语义；
7. PBR / metallic / roughness；
8. Production extension 完整语义；
9. Beam lattice / slice extension；
10. OpenVDB；
11. Qt UI；
12. RIP 半色调；
13. ICC / CMYK。
```

其中最贴近当前全彩切片目标的是：

```text
3MF Texture2D / Texture2DGroup
3MF ColorGroup
更稳健的 XML 解析
```

---

## 3. 下一阶段决策

建议进入：

```text
06B：3MF Texture2D / ColorGroup 基础支持
```

06B 的目标是让 3MF 输入从“材料名 / basematerial / displaycolor 基础映射”升级为：

```text
3MF colorgroup → RGB color
3MF texture2d / texture2dgroup → 当前 TextureSampler
3MF triangle property → RGB role color/texture source
```

---

## 4. 为什么先做 06B，而不是 05A / 07 / OpenVDB

当前项目已经完成 OBJ/MTL/Texture 与 3MF 基础输入，但真实 3MF 彩色模型常见颜色来源并不只有 basematerials，还可能来自：

```text
colorgroup
texture2dgroup
triangle p1/p2/p3 texture coordinates
```

如果不做 06B，3MF 虽然可以导入，但对真实彩色 3MF 的颜色还原能力仍不足。

因此优先建议：

```text
06B：3MF 纹理/颜色基础扩展
```

如果业务更关注真实打印效果，可改走：

```text
05A：真实材料工艺参数验证
```

如果业务更关注调试效率，可改走：

```text
07：Qt 调试 UI
```

---

## 5. 06B 阶段边界

06B 做：

```text
1. 3MF ColorGroup 基础解析；
2. 3MF Texture2D 资源解析；
3. 3MF Texture2DGroup 基础解析；
4. 3MF triangle property 到 RGB texture/color 的映射；
5. 复用现有 TextureImage / TextureSampler；
6. 增强 three_mf_report.json；
7. 增加 3MF texture/color 正向与负向样例。
```

06B 不做：

```text
完整 PBR；
ICC / CMYK；
RIP 半色调；
OpenVDB / SDF；
Qt UI；
复杂支撑形态；
Production extension 完整语义；
Beam lattice / slice extension；
CompositeMaterials / MultiProperties 完整语义。
```

---

## 6. XML Parser 决策

06A 当前使用受限字符串 XML reader，已经满足 06A bad package 与基础 3MF 结构校验。

但 06B 的 Texture2DGroup / ColorGroup 会显著增加 XML 结构复杂度。

因此 06B 建议优先执行以下二选一：

```text
方案 A：引入 tinyxml2，作为正式 3MF XML parser；
方案 B：继续封装 ThreeMfXmlReader，但补充 namespace/local-name 处理与更多 XML edge case。
```

推荐：

```text
优先方案 A：tinyxml2
```

如果暂时不想增加 vcpkg 依赖，则至少必须把 XML 解析逻辑集中在 `ThreeMfXmlReader`，不得在 06B 中新增散落字符串解析。

---

## 7. 冻结项

06B 必须保持：

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
MaterialPolicy RGB/W/V 语义不变
S support 默认仍由 support system 生成
```

---

## 8. 06B 完成后的后续路线

06B 完成后再判断：

```text
路线 A：05A 真实模型材料工艺参数验证；
路线 B：07 Qt 调试 UI；
路线 C：06C CompositeMaterials / MultiProperties；
路线 D：09 OpenVDB / SDF 几何内核预研。
```

默认建议：

```text
06B 完成后，转入 05A 或 07。
```

---

## 9. 结论

06A 可以收口。

下一阶段建议：

```text
06B：3MF Texture2D / ColorGroup 基础支持
```

该阶段让 3MF 彩色输入能力从基础材料色升级到更接近真实彩色 3MF 文件。
