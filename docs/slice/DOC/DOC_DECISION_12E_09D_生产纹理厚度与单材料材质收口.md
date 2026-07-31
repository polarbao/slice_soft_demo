# DOC_DECISION_12E-09D 生产纹理厚度与单材料材质收口

> 文档状态：ACCEPTED PREPARATION / IMPLEMENTATION NOT STARTED
> 日期：2026-07-31
> 执行优先级：P1，排在 03D-LIBTIFF 之后、12E-10A 之前

## 1. 问题

当前 UI “预检与诊断”中的“诊断纹理宽度”是只读分析参数。修改后只更新
`m_diagnosticTextureSurfaceWidthMm`，不会写回生产 Profile，也不会改变切片输出。

与此同时，Legacy 生产 Profile 仍使用：

```text
texture.applyMode=top_surface_band
texture.topSurfaceLayers=<整数层数>
```

它的厚度沿切片 Z 层计算，不等于 Global Surface Shell 的三维法向距离
`texture.surfaceShell.widthMm`。把一个“mm 宽度”控件同时映射到两者会产生错误语义。

单材料浮雕还存在第二个收口缺口：UI 通用“模型内部填充”主要写
`modelFill.material`，但单材料浮雕真实通道由 `modelMaterial.materialChannel`、
`whiteValue/varnishValue` 和 `materialProcessProfile` 共同决定。只改一个字段不能可靠地
把 W 切换为 V。

## 2. 决策

建立 `12E-09D`，把“诊断参数”和“生产参数”彻底分开，并按后端展示真实语义：

```text
Legacy：
  控件名为“顶面纹理层数”；
  生产字段为 texture.topSurfaceLayers；
  同时显示有效 Z 厚度 = 层数 * layerThicknessMm；
  不宣称它是三维表面壳层宽度。

Global Surface Shell：
  控件名为“纹理壳层宽度”；
  生产字段为 texture.surfaceShell.widthMm；
  支持有限宽度与 allTexture；
  仅在已准入 Global Profile 中可编辑。

诊断：
  继续使用独立的“诊断宽度”；
  明确标注不修改生产输出；
  requested/effective/backend/status 都可见。

单材料浮雕：
  提供“白墨 W / 光油 V”材料选择；
  通过单一 resolver 同步所有生产字段；
  不允许 UI 只改 modelFill.material 后继续切片。
```

## 3. 12E-09A 与 09D 边界

```text
09A：只读诊断、宽度上限评估、同层语义 Preview；
09D：生产 Profile 编辑、Effective Config、切片结果；
09A 的控件不得被重命名成生产控件；
09D 不改变 09A 诊断算法或 admission 结论。
```

## 4. Legacy 与 Global 语义

### Legacy

```text
requestedTopSurfaceLayers >= 1；
effectiveZThicknessMm = layers * layerThicknessMm；
纹理 mask 来源仍是每 XY column 的顶部层带；
不能用该值描述侧壁法向壳层宽度。
```

### Global

```text
requestedWidthMm；
effectiveWidthMm；
backend=legacy_cpu_global_distance 或已准入后端；
partitionMode=partial_shell|all_texture；
TextureSurface XOR ModelFill、union=Model 保持不变。
```

用户选择“全纹理”时必须由显式 mode 表达，不能用超大 width 数值模拟。

## 5. 单材料浮雕解析合同

### 白墨

```text
modelMaterial.materialChannel=W；
modelMaterial.rgb=[255,255,255]；
whiteValue=0；
varnishValue=255；
materialProcessProfile.white=enabled/all_model；
materialProcessProfile.varnish=disabled；
requireWhitePixels=true；
requireVarnishPixels=false；
preview 包含 white/support。
```

### 光油

```text
modelMaterial.materialChannel=V；
modelMaterial.rgb=[255,255,255]；
whiteValue=255；
varnishValue=0；
materialProcessProfile.white=disabled；
materialProcessProfile.varnish=enabled/all_model；
requireWhitePixels=false；
requireVarnishPixels=true；
preview 包含 varnish/support。
```

支撑 S、几何轮廓、层数和通道统计除材料角色外应保持一致。

## 6. 非目标

```text
不实现 12G-TCWS 白色/透明分色；
不把纯白 RGB 与背景问题塞进 09D；
不修改 TIFF 协议；
不扩大 Global production admission；
不改变支撑、铺底、外侧光油策略；
不修改模型修复准入；
不实现 LibTIFF。
```

## 7. 执行 Gate

```text
03D-LIBTIFF 具有第一执行优先级；
09D 文档准备可完成，但代码开发等待 03D 当前授权任务结束；
09D 完成后再进入 12E-10A，避免最终收口建立在无效生产控件上；
12G-TCWS 继续冻结。
```
