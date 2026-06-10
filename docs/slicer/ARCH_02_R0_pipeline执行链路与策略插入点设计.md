# ARCH_02_R0_pipeline执行链路与策略插入点设计

> 文档版本：v0.1  
> 文档状态：Architecture Draft  
> 适用阶段：R0  
> 建议提交目录：`docs/slicer/`

---

## 1. 正式 Pipeline

建议 pipeline 拆分：

```text
LoadConfig
ValidateConfig
LoadInputScene
NormalizeScene
ResolveMaterials
PrepareTextureSources
ApplyTextureApplicationPolicy
PrepareVarnishGeometryPolicy
SliceGeometry
GenerateSupport
ComposeMaterialChannels
WriteRGBWSVPackage
WriteReports
ValidatePackage
```

---

## 2. 策略插入点

### 2.1 TextureApplicationPolicy

位置：

```text
PrepareTextureSources 之后
ComposeMaterialChannels 之前
```

职责：

```text
决定 RGB 是 full volume、surface shell、top surface 还是 outer shell。
```

### 2.2 VarnishGeometryPolicy

位置：

```text
NormalizeScene / SliceGeometry / ComposeMaterialChannels 之间
```

职责：

```text
决定 V channel 是 top_n_layers、additive grow 还是 compensated shrink。
```

### 2.3 SupportPolicy

位置：

```text
SliceGeometry 之后
ComposeMaterialChannels 之前
```

职责：

```text
决定 S support mask 与 SupportType diagnostics。
```

---

## 3. Pipeline Step 接口建议

每个 step 应有：

```text
Input
Output
Config
Diagnostics
Warnings
Errors
Timing
```

示意：

```cpp
struct PipelineStepResult {
    bool ok;
    Diagnostics diagnostics;
    std::vector<Warning> warnings;
    std::vector<Error> errors;
};
```

---

## 4. R1 实施原则

R1 重构时不要一次性重写算法。

应先把当前逻辑包进 step，再逐步拆文件：

```text
wrap first
move later
rewrite last
```
