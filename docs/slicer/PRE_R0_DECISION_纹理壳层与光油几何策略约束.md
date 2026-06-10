# PRE_R0_DECISION_纹理壳层与光油几何策略约束

> 文档版本：v0.1  
> 文档状态：Pre-R0 Decision / 架构约束  
> 适用阶段：R0 输入约束  
> 建议提交目录：`docs/slicer/`

---

## 1. 背景

在进入正式项目架构重构前，需要明确两个会影响核心架构的策略问题：

```text
1. 彩色纹理如何写入模型体积；
2. 光油如何改变或不改变最终模型尺寸。
```

这两个问题不应作为 Demo 中的临时 if/else 继续扩展，而应在 R0 中上升为正式策略对象。

---

## 2. 彩色纹理策略

### 2.1 策略 A：full_volume_texture

定义：

```text
模型实体区域内全部写入 RGB 彩色信息。
```

优点：

```text
实现简单；
当前 Demo 可兼容；
内部剖开也有颜色；
适合透明/半透明或浮雕贯穿色模型。
```

缺点：

```text
彩色材料消耗大；
真实外观件通常不需要内部彩色；
内部填充材料职责不清；
后续材料成本优化困难。
```

建议：

```text
作为 Demo 兼容模式和短期默认模式保留。
```

---

### 2.2 策略 B：surface_shell_texture

定义：

```text
只在距离模型外表面一定厚度的壳层区域写入 RGB；
内部由 fill/base/white/其他材料填充。
```

配置示例：

```json
{
  "textureApplication": {
    "mode": "surface_shell",
    "shellThicknessPx": 3,
    "shellThicknessMm": 0.05,
    "fillRole": "base",
    "shellRegion": "outer_surface"
  }
}
```

优点：

```text
更接近真实彩色 3D 打印；
彩色材料成本更低；
RGB 与内部填充材料职责清晰；
利于材料策略和工艺 profile。
```

缺点：

```text
需要 mask erosion/dilation 或 distance field；
薄壁/尖角/小特征处理复杂；
2D per-layer shell 只是近似；
更精确版本需要 3D SDF / OpenVDB。
```

建议：

```text
R0 设计接口；
R1/R2 后实现 2D per-layer 近似；
09 或几何内核成熟后实现 3D shell/SDF 版本。
```

---

## 3. 光油几何策略

### 3.1 策略 A：additive_varnish

定义：

```text
在原模型表面直接增加光油层，最终模型尺寸可能变大。
```

优点：

```text
实现相对简单；
适合美甲、浮雕、装饰件；
符合 clear coat / varnish 的直观语义；
当前 top_n_layers 接近此策略。
```

缺点：

```text
最终尺寸会变大；
对装配件和尺寸敏感件不合适；
侧面光油/法向光油需要更复杂几何 offset。
```

建议：

```text
作为短期默认策略正式化。
```

---

### 3.2 策略 B：compensated_varnish

定义：

```text
先缩小模型主体，再添加光油层，使最终外包络接近原模型尺寸。
```

配置示例：

```json
{
  "varnishGeometry": {
    "mode": "compensated",
    "thicknessMm": 0.02,
    "compensation": {
      "method": "shrink_body_then_restore_envelope"
    }
  }
}
```

优点：

```text
最终尺寸更可控；
适合工程件和尺寸敏感件；
为材料补偿和设备标定留出空间。
```

缺点：

```text
需要几何 offset、heightfield shrink 或 SDF；
薄壁可能被缩没；
纹理映射和外包络控制更复杂；
不适合在当前 Demo 架构中硬做。
```

建议：

```text
R0 预留接口；
R1 不强制实现；
09 或几何内核成熟后实现。
```

---

## 4. R0 必须新增两个策略对象

### 4.1 TextureApplicationPolicy

建议概念：

```cpp
enum class TextureApplicationMode {
    FullVolume,
    SurfaceShell,
    TopSurfaceOnly,
    OuterSurfaceShell
};

struct TextureApplicationPolicy {
    TextureApplicationMode mode;
    int shell_thickness_px;
    double shell_thickness_mm;
    std::string fill_role;
    std::string shell_region;
};
```

### 4.2 VarnishGeometryPolicy

建议概念：

```cpp
enum class VarnishGeometryMode {
    InPlaceTopLayers,
    AdditiveGrow,
    CompensatedShrink
};

struct VarnishGeometryPolicy {
    VarnishGeometryMode mode;
    int thickness_layers;
    double thickness_mm;
    std::string compensation_method;
};
```

---

## 5. 实现优先级

```text
R0:
  设计接口，不编码实现复杂策略。

R1:
  重构 pipeline，使策略有插入点。

R2:
  实现 full_volume_texture 正式化；
  实现 additive_varnish 正式化；
  实现 surface_shell_texture 的 2D per-layer 近似版本。

09 后:
  实现 3D shell / compensated_varnish / SDF / OpenVDB 版本。
```

---

## 6. 结论

进入 R0 前必须明确：

```text
Demo 当前行为只是策略之一，不是最终唯一行为。
```

R0 必须把彩色纹理壳层策略与光油几何策略作为正式架构约束纳入设计。
