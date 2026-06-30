# DEV_04A_纹理FallbackFixture与支撑诊断设计

> 文档版本：v0.1  
> 文档状态：Draft / DEV  
> 适用阶段：04A  
> 建议提交目录：`docs/slicer/`

---

## 1. 技术目标

04A 只做轻量收口：

```text
1. 重建小型 texture fallback fixtures
2. 修改 fallback configs 指向小型 fixture
3. 让 fallback 测试快速稳定
4. 增加支撑连通性诊断统计
```

不改 RGBWSV 协议，不改 texture 主链路。

---

## 2. Fixture 设计

### 2.1 missing_texture_small

文件：

```text
samples/models/textured/fixtures/missing_texture_small.obj
samples/models/textured/fixtures/missing_texture_small.mtl
```

MTL 示例：

```text
newmtl missing_tex
Kd 0.2 0.4 0.8
map_Kd textures/does_not_exist.png
```

期望：

```text
texture_report.missingTextures > 0
texture_report.warnings 非空
fallbackPixels > 0
```

### 2.2 no_uv_small

文件：

```text
samples/models/textured/fixtures/no_uv_small.obj
samples/models/textured/fixtures/no_uv_small.mtl
```

OBJ 要求：

```text
不包含 vt
face 只使用 v 或 v//vn
面数少
尺寸小
```

期望：

```text
facesWithUv = 0
facesWithoutUv > 0
fallbackPixels > 0
```

---

## 3. Config 修改

### 3.1 textured_missing_texture_fallback.json

应指向：

```text
samples/models/textured/fixtures/missing_texture_small.obj
```

输出：

```text
output/TexturedMissingTextureFallback
```

### 3.2 textured_no_uv_fallback.json

应指向：

```text
samples/models/textured/fixtures/no_uv_small.obj
```

输出：

```text
output/TexturedNoUvFallback
```

### 3.3 heavy real model

真实 38MB 模型如需保留，应放入：

```text
samples/models/textured/heavy/
```

对应配置放入：

```text
samples/configs/textured/heavy/
```

并在 regression 中默认跳过。

---

## 4. 支撑连通性诊断

针对 support mask，新增或复用 connected component 统计。

建议字段：

```json
{
  "supportConnectivity": {
    "enabled": true,
    "componentCount": 0,
    "largestComponentPixels": 0,
    "smallComponentCount": 0,
    "tinyComponentCount": 0,
    "components": []
  }
}
```

每个 component 可记录：

```text
areaPx
bbox
```

建议阈值：

```text
tinyComponentAreaPx <= 8
smallComponentAreaPx <= 512
```

这些字段可以写入：

```text
support_report.json
slice_report.layers[i].supportConnectivity
```

---

## 5. 不强制修复支撑形态

04A 不实现：

```text
support island merge
support gap closing
support morphology
support contour constraint
```

只输出诊断。

如果后续确认美甲支撑必须修复，则另开：

```text
08：高级支撑与工艺优化
```

或：

```text
04B：美甲支撑形态修复
```

---

## 6. Regression 修改

`run_regression.ps1` 应覆盖：

```text
TexturedReliefRgb
TexturedMissingTextureFallback
TexturedNoUvFallback
```

并保证 fallback 用例不使用 heavy model。

建议增加参数：

```powershell
-SkipHeavyTexture
```

如有 heavy texture 样例，默认不跑。

---

## 7. 报告修改

04A 完成后生成：

```text
docs/slicer/REPORT_04A_纹理阶段收口修复当前实现状态.md
```

报告必须说明：

```text
1. fallback fixtures 是否已重建
2. missing texture 是否真实触发 fallback
3. no-UV 是否真实触发 fallback
4. run_regression 是否通过
5. 支撑割裂诊断是否输出
6. 是否建议进入 05 材料策略
```
