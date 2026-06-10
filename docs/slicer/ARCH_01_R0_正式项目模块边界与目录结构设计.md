# ARCH_01_R0_正式项目模块边界与目录结构设计

> 文档版本：v0.1  
> 文档状态：Architecture Draft  
> 适用阶段：R0  
> 建议提交目录：`docs/slicer/`

---

## 1. 正式模块边界

建议正式项目拆分为：

```text
core/config
core/pipeline
core/scene
core/diagnostics
importers/obj
importers/mtl
importers/three_mf
texture
materials
support
raster
output/rgbwsv
reports
tools
apps/slicer_debug_ui
```

---

## 2. 推荐目录结构

```text
src/
  core/
    config/
    pipeline/
    scene/
    diagnostics/

  importers/
    obj/
    mtl/
    three_mf/

  texture/
    image/
    sampler/

  materials/
    role_mapping/
    material_policy/
    process_profile/
    texture_application/
    varnish_geometry/

  support/
    generation/
    diagnostics/

  raster/
    scanline/
    relief/

  output/
    rgbwsv/
    tiff/
    manifest/

  reports/
    writers/
    schema/

apps/
  slicer_cli/
  rip_reader_test/
  slicer_debug_ui/
```

---

## 3. 模块依赖方向

推荐依赖：

```text
importers -> scene
scene -> pipeline
pipeline -> texture/materials/support/raster/output/reports
tools -> core/pipeline
apps -> tools/services/reports
```

禁止反向依赖：

```text
slicer_core 不依赖 Qt UI
importer 不直接写 TIFF
material policy 不直接读取文件系统
support 不直接写 report 文件
```

---

## 4. 需要从现有文件拆出的职责

```text
model.cpp:
  3MF ZIP/XML
  OBJ/MTL parsing
  material resource mapping
  texture metadata

slicer.cpp:
  pipeline orchestration
  rasterization
  material composition
  support generation
  report writing
  preview writing
```

R1 再执行实际拆分，R0 只完成设计与任务边界。
