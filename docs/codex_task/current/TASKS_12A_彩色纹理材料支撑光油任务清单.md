# TASKS_12A_彩色纹理材料支撑光油任务清单

> 文档版本：v0.1
> 文档状态：Task List / Stage 12A
> 生成日期：2026-07-05

---

## 任务边界

12A 只处理彩色纹理/单材料模型的材料语义、模型填充、支撑填充、内部镂空支撑、外侧光油壳层和相关 report/preview。
不处理 OpenVDB 替代结论，不处理 UI 大布局重构，不改变 RGBWSV 协议。

---

## 原子任务

### Task 12A-01 需求术语确认

状态：PENDING

内容：

```text
确认 Model Real Data、TextureSurface、ModelFill、SupportFill、InternalVoidSupport、OuterVarnishShell 定义。
```

验证：

```powershell
git diff --check
```

### Task 12A-02 配置 schema 占位

状态：PENDING

内容：

```text
新增 modelFill / support.internalVoid / outerVarnish 配置字段，默认兼容旧行为。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_cli
```

### Task 12A-03 LayerSemanticReport

状态：PENDING

内容：

```text
在 slice_report 或 layer summary 中统计 textureSurfacePixels、modelFillPixels、supportPixels、internalVoidSupportPixels、outerVarnishPixels。
```

验证：

```powershell
.\build\Debug\slicer_cli.exe --config samples\configs\material_process\obj_mtl_texture_rgb_white_varnish.json
```

### Task 12A-04 ModelFillPolicy

状态：PENDING

内容：

```text
实现 white / varnish / rgb / none / legacyRgbFallback。
```

验证：

```text
检查关键层 TIFF 六通道像素和 report 统计。
```

### Task 12A-05 InternalVoidSupportPolicy

状态：PENDING

内容：

```text
实现 enclosed_by_model_envelope 内部镂空支撑。
```

验证：

```text
internal void fixture supportPixels 增加，模型外部空白不被填充。
```

### Task 12A-06 SupportPlacementPolicy

状态：PENDING

内容：

```text
梳理 lower / upper / both / unsupported_only / full_vertical_projection，并把 full_vertical_projection 标记为 advanced/debug。
```

验证：

```text
不同 placement 的 support_report reason 正确。
```

### Task 12A-07 OuterVarnishShellPolicy

状态：PENDING

内容：

```text
实现外侧光油壳层 thicknessPx/thicknessMm/pixelPitchUm。
```

验证：

```text
V 通道壳层宽度等于配置厚度，report 输出 px/mm。
```

### Task 12A-08 彩色/单材料一致性 fixture

状态：PENDING

内容：

```text
对同一甲片模型建立彩色 Profile 与单材料 Profile 对比。
```

验证：

```text
layerCount/model mask/support mask 可比较，差异只来自材料通道。
```

### Task 12A-09 UI preview 图例与像素探针联动

状态：PENDING

内容：

```text
让 LayerPreview/OverlayPreview 能显示 semantic/sourcePolicy。
```

验证：

```text
UI smoke，手动点击关键像素显示 RGB/W/S/V 与语义。
```

---

## 完成标准

```text
1. 12A 所有 P0/P1 任务完成；
2. 文档、配置、report、preview 一致；
3. 典型真实模型通过；
4. 旧 fixture 默认兼容；
5. 状态报告更新。
```
