# DEV_R1_核心模块边界重构设计

> 文档版本：v0.1  
> 文档状态：DEV / 重构设计  
> 适用阶段：R1  
> 建议提交目录：`docs/slicer/`

---

## 1. R1 技术目标

R1 的目标不是重写算法，而是建立正式项目模块边界。

当前重点对象：

```text
src/slicer_core/model.cpp
src/slicer_core/slicer.cpp
src/slicer_core/config.cpp
```

其中：

```text
model.cpp:
  当前混合了 scene model、OBJ/MTL importer、3MF importer、material resource 解析、texture resource 解析。

slicer.cpp:
  当前混合了 pipeline orchestration、raster、relief、support、materials、output、reports、preview。
```

R1 要做的是把这些职责拆出边界。

---

## 2. 推荐新增目录

R1 第一阶段建议先在 `src/slicer_core/` 内建立子目录，避免一次性改变所有 include 路径：

```text
src/slicer_core/
  scene/
  importers/
    obj/
    mtl/
    three_mf/
  pipeline/
  materials/
    role_mapping/
    material_policy/
    process_profile/
    texture_application/
    varnish_geometry/
  support/
  raster/
  output/
    rgbwsv/
  reports/
  diagnostics/
```

如果当前构建系统对 nested path 支持稳定，再逐步向 `src/core`、`src/importers` 等正式结构迁移。

---

## 3. 拆分优先级

### 3.1 第一步：Scene Model Wrapper

目标：

```text
将当前 Model / Mesh / MaterialInfo / TextureInfo / TriangleTextureInfo 等数据结构梳理为 scene 层。
```

新增建议：

```text
src/slicer_core/scene/SceneModel.h
src/slicer_core/scene/SceneModel.cpp
```

第一步可以只搬类型定义和轻量 helper，不改算法。

---

### 3.2 第二步：Importer Wrapper

目标：

```text
拆分 importer 边界，但先不重写 parser。
```

新增建议：

```text
src/slicer_core/importers/obj/ObjImporter.*
src/slicer_core/importers/mtl/MtlImporter.*
src/slicer_core/importers/three_mf/ThreeMfImporter.*
```

短期允许内部调用 legacy 函数。

原则：

```text
public API 先成型，内部实现可暂时留在 model.cpp 或 legacy helper。
```

---

### 3.3 第三步：Pipeline Step Wrapper

新增：

```text
src/slicer_core/pipeline/SlicePipeline.*
src/slicer_core/pipeline/PipelineContext.*
src/slicer_core/pipeline/PipelineStepResult.*
```

先定义 step：

```text
LoadConfig
LoadInputScene
ResolveMaterials
PrepareTextureSources
SliceGeometry
GenerateSupport
ComposeMaterialChannels
WritePackage
WriteReports
ValidatePackage
```

第一版 step 允许调用当前 `run_slicing` 内部逻辑，不要求立刻完全解耦。

---

### 3.4 第四步：Materials 模块边界

新增：

```text
src/slicer_core/materials/role_mapping/MaterialRoleMapping.*
src/slicer_core/materials/material_policy/MaterialPolicy.*
src/slicer_core/materials/process_profile/MaterialProcessProfile.*
src/slicer_core/materials/texture_application/TextureApplicationPolicy.*
src/slicer_core/materials/varnish_geometry/VarnishGeometryPolicy.*
```

R1 只实现：

```text
TextureApplicationPolicy 数据结构和解析占位；
VarnishGeometryPolicy 数据结构和解析占位；
full_volume / in_place_top_layers 映射到当前 legacy 行为。
```

R1 不实现：

```text
surface_shell_texture
compensated_varnish
```

---

### 3.5 第五步：Reports Writer Wrapper

新增：

```text
src/slicer_core/reports/ReportWriter.*
src/slicer_core/reports/ReportSchema.*
```

第一版只把现有 report 写出入口包一层，不统一 schema。

R2 再做 report schema 统一。

---

## 4. R1 不应做的事情

R1 不做：

```text
1. 不新增切片功能；
2. 不重写 scanline 算法；
3. 不重写 3MF parser；
4. 不实现 shell texture；
5. 不实现 compensated varnish；
6. 不引入 OpenVDB；
7. 不修改 TIFF writer 协议；
8. 不破坏旧 config；
9. 不把 UI 直接接入 slicer_core 内部结构。
```

---

## 5. 构建要求

每个阶段都必须保持：

```text
slicer_core
slicer_cli
rip_reader_test
slicer_debug_ui
```

可以构建。

CMake 修改应采用增量方式：

```text
先新增源文件；
再迁移 include；
最后删除 legacy 函数。
```

---

## 6. 验证要求

每个可提交拆分点执行：

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -Mode quick
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

涉及 UI/preview 相关改动时额外执行：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
```
