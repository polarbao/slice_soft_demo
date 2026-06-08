# PRD_04A_纹理Fallback样例与支撑割裂诊断修复

> 文档版本：v0.1  
> 文档状态：Draft / PRD  
> 适用阶段：REPORT_04 之后  
> 建议提交目录：`docs/slicer/`

---

## 1. 产品目标

04A 目标是补齐 04 彩色纹理基础阶段的验收缺口：

```text
1. 确保 missing texture fallback 可被真实验证
2. 确保 no-UV fallback 可被真实验证
3. 避免 fallback 测试依赖 38MB 真实大模型
4. 对真实美甲纹理模型中的支撑局部割裂输出清晰诊断
```

04A 完成后，04 才算完整收口，可进入 05 材料策略阶段。

---

## 2. 必须修复内容

### 2.1 重建 missing texture 小型 fixture

新增或恢复：

```text
samples/models/textured/fixtures/missing_texture_small.obj
samples/models/textured/fixtures/missing_texture_small.mtl
```

要求：

```text
OBJ 有 vt
MTL 有 map_Kd
map_Kd 指向不存在的文件
模型足够小，rip_reader_test 快速完成
```

验收：

```text
missingTextures > 0
warnings 非空
fallbackPixels > 0
rip_reader_test pass
```

---

### 2.2 重建 no-UV 小型 fixture

新增或恢复：

```text
samples/models/textured/fixtures/no_uv_small.obj
samples/models/textured/fixtures/no_uv_small.mtl
```

要求：

```text
OBJ face 不包含 vt
可有 MTL / Kd
模型足够小
```

验收：

```text
facesWithUv = 0
facesWithoutUv > 0
fallbackPixels > 0
rip_reader_test pass
```

---

### 2.3 保留真实大模型作为 heavy sample

当前 38MB 真实纹理模型不应继续作为 fallback fixture。

应移动或标记为：

```text
samples/models/textured/heavy/
```

或者在配置中标记：

```json
{
  "testClass": "heavy_real_model"
}
```

不应作为快速 fallback 回归样例。

---

## 3. 支撑割裂诊断需求

对于 `TexturedReliefRgb` 第 68 层支撑局部割裂，04A 不强制修复支撑形态，但必须把该现象诊断清楚。

报告应记录：

```text
support connected components
largest component area
small component count
tiny component count
support fragmentation reason
```

建议输出到：

```text
support_report.json
slice_report.json
或 texture_support_diagnostics.json
```

第一版可以只在 report 中统计，不要求新增 preview 图。

---

## 4. 支撑割裂是否要立即修复

04A 不强制实现业务级支撑形态修复。

但应输出明确结论：

```text
这是支撑形态策略问题，不是 texture RGB 覆盖 support，也不是协议错误。
```

后续有两种路线：

```text
路线 A：05 材料策略中只定义材料组合，不处理支撑形态；
路线 B：05 前新增 04B / 08 支撑形态优化，处理支撑连通、狭缝过滤、小岛合并/剔除。
```

当前建议：

```text
04A 只做诊断与报告，不做形态修复。
```

---

## 5. Regression 需求

`scripts/run_regression.ps1` 应覆盖：

```text
TexturedReliefRgb 主样例
MissingTextureSmallFallback
NoUvSmallFallback
```

快速回归不应依赖 38MB 大模型。

---

## 6. 验收标准

1. `textured_relief_rgb.json` 继续通过。
2. `textured_missing_texture_fallback.json` 改为使用小型 missing texture fixture。
3. `textured_no_uv_fallback.json` 改为使用小型 no-UV fixture。
4. missing texture 用例 `missingTextures > 0`。
5. missing texture 用例 `fallbackPixels > 0`。
6. no-UV 用例 `facesWithUv = 0`。
7. no-UV 用例 `facesWithoutUv > 0`。
8. no-UV 用例 `fallbackPixels > 0`。
9. fallback 用例 rip_reader_test 快速通过。
10. run_regression.ps1 通过。
11. 第 68 层支撑割裂至少有 report 级诊断。
12. 不改变 RGBWSV 协议。

---

## 7. 非目标

04A 不做：

```text
材料策略
白墨 underbase
光油 top layer
texture-driven varnish
color_shell_volume
闭合模型完整外壳纹理
复杂支撑树
支撑可拆结构
OpenVDB
Qt UI
RIP 半色调
ICC / CMYK
```

---

## 8. 结论

04A 是 04 的收口修复阶段，不是新功能扩展阶段。

目标是让 04 的 texture 主链路、fallback 逻辑、回归速度和真实模型诊断都可信。
