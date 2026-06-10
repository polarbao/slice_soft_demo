# REPORT_R1_核心模块边界重构当前状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 生成日期：2026-06-10  
> 适用阶段：R1

---

## 1. 阶段结论

R1 已完成第一轮核心模块边界重构。

本阶段采用 R0/R1 文档要求的方式：

```text
wrap first
move later
rewrite last
```

本次只建立正式模块目录和可编译 wrapper/API 边界，不重写 parser、不重写 scanline、不重写 support、不修改 TIFF/RGBWSV 输出协议。

---

## 2. 拆分了哪些模块

### 2.1 Scene

新增：

```text
src/slicer_core/scene/SceneModel.h
src/slicer_core/scene/SceneModel.cpp
```

当前状态：

- `SceneModel` 暂时 alias 到 legacy `ModelReport`。
- `SceneTriangle` / `SceneMaterialInfo` / `SceneTriangleTextureInfo` 暂时 alias 到现有类型。
- 新增 `SceneSummary` 和 `summarize_scene()`，用于形成 scene 层 API 雏形。
- 未迁移实际 importer 算法。

### 2.2 Importers

新增：

```text
src/slicer_core/importers/obj/ObjImporter.*
src/slicer_core/importers/mtl/MtlImporter.*
src/slicer_core/importers/three_mf/ThreeMfImporter.*
```

当前状态：

- `ObjImporter` 提供 `load_obj_scene_legacy()`，内部调用 legacy `load_model_report()`。
- `ThreeMfImporter` 提供 `load_three_mf_scene_legacy()`，内部调用 legacy `load_model_report()`。
- `MtlImporter` 提供 `MtlImportSummary` wrapper，用于先建立 MTL 边界。
- 未重写 OBJ/MTL/3MF parser。

### 2.3 Pipeline

新增：

```text
src/slicer_core/pipeline/PipelineContext.*
src/slicer_core/pipeline/PipelineStepResult.*
src/slicer_core/pipeline/SlicePipeline.*
```

当前状态：

- 定义 `PipelineStepResult`、`Diagnostics` 和 `PipelineContext`。
- `default_slice_pipeline_steps()` 已列出 R1/R0 设计中的主 pipeline step。
- `run_slice_pipeline_legacy()` 作为 facade，可调用现有 `run_slicer()`。
- 当前 CLI 仍直接使用 `run_slicer()`，行为保持不变。

### 2.4 Materials

新增：

```text
src/slicer_core/materials/role_mapping/MaterialRoleMapping.*
src/slicer_core/materials/material_policy/MaterialPolicy.*
src/slicer_core/materials/process_profile/MaterialProcessProfile.*
src/slicer_core/materials/texture_application/TextureApplicationPolicy.*
src/slicer_core/materials/varnish_geometry/VarnishGeometryPolicy.*
```

当前状态：

- 已建立 MaterialRoleMapping / MaterialPolicy / MaterialProcessProfile boundary DTO。
- 已建立 `TextureApplicationPolicy` 与 `TextureApplicationMode`。
- 已建立 `VarnishGeometryPolicy` 与 `VarnishGeometryMode`。
- legacy 纹理策略映射为 `full_volume`。
- legacy 光油策略映射为 `in_place_top_layers`。
- 未实现 `surface_shell_texture`。
- 未实现 `compensated_varnish`。

### 2.5 Support / Raster / Output / Reports / Diagnostics

新增：

```text
src/slicer_core/support/SupportPolicy.*
src/slicer_core/raster/RasterBoundary.*
src/slicer_core/output/rgbwsv/RgbwsvPackage.*
src/slicer_core/reports/ReportWriter.*
src/slicer_core/reports/ReportSchema.*
src/slicer_core/diagnostics/Diagnostics.*
```

当前状态：

- `SupportPolicy` 从现有 `SupportConfig` 形成支撑边界对象。
- `RasterBoundary` 记录当前 legacy raster 模式：`closed_mesh_scanline` / `relief_heightfield`。
- `RgbwsvPackage` 固化当前协议引用：`p0.rgbwsv.2`、`R G B W S V`、8-bit、`black_is_print`。
- `ReportWriter` 提供 report JSON 写出 wrapper。
- `ReportSchema` 提供 `preview_report` schema 引用。
- `Diagnostics` 提供 R1 pipeline/report 后续统一诊断的基础类型。

---

## 3. CMake 状态

顶层 `CMakeLists.txt` 已将所有新增 wrapper 文件增量加入 `slicer_core`。

当前 target 保持：

```text
slicer_core
slicer_cli
rip_reader_test
slicer_debug_ui
```

没有新增第三方依赖，没有修改 vcpkg，没有改变 Qt UI target 依赖方式。

---

## 4. 哪些逻辑仍保留在 legacy 文件

### 4.1 仍保留在 `model.cpp`

```text
STL ASCII / Binary parser
OBJ parser
MTL parser
3MF ZIP 解包
3MF restricted XML parser
3MF object/component/material/texture 解析
纹理资源缓存
auto orient
bbox / model statistics
load_model_report()
```

R1 当前只建立 importer facade，未移动上述算法。

### 4.2 仍保留在 `slicer.cpp`

```text
run_slicer() 主流程
grid 计算
scanline raster
relief heightfield
support generation
island diagnostics
material role mapping 执行逻辑
texture runtime / sampler
RGB/W/V/S channel composition
TIFF layer 写出调用
preview PNG/PPM 写出
manifest 写出
report JSON 构造和写出
```

R1 当前只建立 pipeline/material/support/raster/output/reports wrapper，未拆空 `slicer.cpp`。

### 4.3 仍保留在 `config.cpp`

```text
legacy SliceConfig 读取
legacy 字段兼容
字段校验
阶段性限制错误提示
```

R2 再处理 config schema version、migration 和 diagnostics/error code 工程化。

---

## 5. 输出协议是否不变

输出协议保持不变：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

本阶段没有修改 TIFF writer 行为、manifest 生产逻辑、RIP reader 校验逻辑或 CLI 参数语义。

---

## 6. 验证结果

已执行并通过：

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -Mode quick
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\Debug\slicer_cli.exe --config samples\configs\ui_smoke\ui_overlay_rgbwv_preview.json
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
```

关键结果：

```text
Debug build: PASS
Regression complete. mode=quick
UI self-test: PASS
UiSmokeOverlayRgbwv generated: PASS
overlay-load-real: PASS overlay-load-real images=94 channels=rgb,support,varnish,white modes=RGB + W 白墨,RGB + V 光油,RGB + S 支撑
```

---

## 7. R1 完成度判断

R1 当前完成的是第一轮“模块边界建立”：

```text
目录结构已落地；
wrapper/API 已可编译；
legacy 行为未改变；
守门验证通过；
legacy 职责清单明确。
```

R1 没有完成、也不应该在本阶段完成：

```text
完全拆空 model.cpp；
完全拆空 slicer.cpp；
surface_shell_texture；
compensated_varnish；
OpenVDB；
设备通信；
R2 config/report/test/CI schema 工程化。
```

---

## 8. 是否可以进入 R2

可以进入 R2 的设计准备阶段，但建议先由用户确认是否接受当前 R1 作为“边界 wrapper 完成版”。

原因：

- R1 要求的模块目录和 wrapper 边界已经建立。
- `model.cpp` / `slicer.cpp` 的 legacy 留存职责已经记录。
- quick regression 与 UI smoke 均通过。
- 输出协议保持不变。

进入 R2 后应聚焦：

```text
config schema version
config migration
report schema version
diagnostics/error code 统一
unit/golden/schema/regression/ui smoke test 分层
CI 入口
```

如果希望 R1 继续深入，下一步应只做小步移动：优先将 report writer 调用点或 protocol constants 接入现有 `slicer.cpp`，每一步继续用 quick regression 守门。
